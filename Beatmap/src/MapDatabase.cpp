#include "stdafx.h"
#include "MapDatabase.hpp"
#include "Database.hpp"
#include "Beatmap.hpp"
#include "TinySHA1.hpp"
#include "Shared/Profiling.hpp"
#include "Shared/Files.hpp"
#include <thread>
#include <mutex>
#include <chrono>
using std::thread;
using std::mutex;
using namespace std;

class MapDatabase_Impl
{
public:
	// For calling delegates
	MapDatabase& m_outer;

	thread m_thread;
	bool m_searching = false;
	bool m_interruptSearch = false;
	Set<String> m_searchPaths;
	Database m_database;

	Map<int32, MapIndex*> m_maps;
	Map<int32, DifficultyIndex*> m_difficulties;
	Map<String, MapIndex*> m_mapsByPath;
	int32 m_nextMapId = 1;
	int32 m_nextDiffId = 1;
	String m_sortField = "title";

	struct SearchState
	{
		struct ExistingDifficulty
		{
			int32 id;
			uint64 lwt;
		};
		// Maps file paths to the id's and last write time's for difficulties already in the database
		Map<String, ExistingDifficulty> difficulties;
	} m_searchState;

	// Represents an event produced from a scan
	//	a difficulty can be removed/added/updated
	//	a BeatmapSettings structure will be provided for added/updated events
	struct Event
	{
		enum Action
		{
			Added,
			Removed,
			Updated
		};
		Action action;
		String path;
		// Current lwt of file
		uint64 lwt;
		// Id of the map
		int32 id;
		// Scanned map data, for added/updated maps
		BeatmapSettings* mapData = nullptr;
		String hash;
	};
	List<Event> m_pendingChanges;
	mutex m_pendingChangesLock;

	static const int32 m_version = 11;

public:
	MapDatabase_Impl(MapDatabase& outer) : m_outer(outer)
	{
		String databasePath = Path::Absolute("maps.db");
		if(!m_database.Open(databasePath))
		{
			Logf("Failed to open database [%s]", Logger::Warning, databasePath);
			assert(false);
		}

		bool rebuild = false;
		bool update = false;
		DBStatement versionQuery = m_database.Query("SELECT version FROM `Database`");
		int32 gotVersion = 0;
		if(versionQuery && versionQuery.Step())
		{
			gotVersion = versionQuery.IntColumn(0);
			if(gotVersion != m_version)
			{
				update = true;
			}
		}
		else
		{
			// Create DB 
			m_database.Exec("DROP TABLE IF EXISTS Database");
			m_database.Exec("CREATE TABLE Database(version INTEGER)");
			m_database.Exec(Utility::Sprintf("INSERT OR REPLACE INTO Database(rowid, version) VALUES(1, %d)", m_version));
			rebuild = true;
		}
		versionQuery.Finish();

		if(rebuild)
		{
			m_CreateTables();

			// Update database version
			m_database.Exec(Utility::Sprintf("UPDATE Database SET `version`=%d WHERE `rowid`=1", m_version));
		}
		else if (update)
		{
			///TODO: Make loop for doing iterative upgrades
			if (gotVersion == 8)  //upgrade from 8 to 9
			{
				m_database.Exec("ALTER TABLE Scores ADD COLUMN hitstats BLOB");
				gotVersion = 9;
			}
			if (gotVersion == 9)  //upgrade from 9 to 10
			{
				m_database.Exec("ALTER TABLE Scores ADD COLUMN timestamp INTEGER");
				gotVersion = 10;
			}
			if (gotVersion == 10)  //upgrade from 10 to 11
			{
				m_database.Exec("ALTER TABLE Difficulties ADD COLUMN hash TEXT");
				gotVersion = 11;
			}
			m_database.Exec(Utility::Sprintf("UPDATE Database SET `version`=%d WHERE `rowid`=1", m_version));
		}
		else
		{
			// Load initial folder tree
			m_LoadInitialData();
		}
	}
	~MapDatabase_Impl()
	{
		StopSearching();
		m_CleanupMapIndex();
	}

	void StartSearching()
	{
		if(m_searching)
			return;

		if(m_thread.joinable())
			m_thread.join();
		// Apply previous diff to prevent duplicated entry 
		Update();
		// Create initial data set to compare to when evaluating if a file is added/removed/updated
		m_LoadInitialData();
		m_interruptSearch = false;
		m_searching = true;
		m_thread = thread(&MapDatabase_Impl::m_SearchThread, this);
	}
	void StopSearching()
	{
		m_interruptSearch = true;
		m_searching = false;
		if(m_thread.joinable())
		{
			m_thread.join();
		}
	}
	void AddSearchPath(const String& path)
	{
		String normalizedPath = Path::Normalize(Path::Absolute(path));
		if(m_searchPaths.Contains(normalizedPath))
			return;

		m_searchPaths.Add(normalizedPath);
	}
	void RemoveSearchPath(const String& path)
	{
		String normalizedPath = Path::Normalize(Path::Absolute(path));
		if(!m_searchPaths.Contains(normalizedPath))
			return;

		m_searchPaths.erase(normalizedPath);
	}

	/* Thread safe event queue functions */
	// Add a new change to the change queue
	void AddChange(Event change)
	{
		m_pendingChangesLock.lock();
		m_pendingChanges.emplace_back(change);
		m_pendingChangesLock.unlock();
	}
	// Removes changes from the queue and returns them
	//	additionally you can specify the maximum amount of changes to remove from the queue
	List<Event> FlushChanges(size_t maxChanges = -1)
	{
		List<Event> changes;
		m_pendingChangesLock.lock();
		if(maxChanges == -1)
		{
			changes = std::move(m_pendingChanges); // All changes
		}
		else
		{
			for(size_t i = 0; i < maxChanges && !m_pendingChanges.empty(); i++)
			{
				changes.AddFront(m_pendingChanges.front());
				m_pendingChanges.pop_front();
			}
		}
		m_pendingChangesLock.unlock();
		return std::move(changes);
	}

	Map<int32, MapIndex*> FindMapsByHash(const String& hash)
	{
		String stmt = "SELECT DISTINCT Maps.rowid FROM Maps INNER JOIN Difficulties ON Maps.rowid = Difficulties.mapid "
					  "WHERE Difficulties.hash = ?";
		DBStatement search = m_database.Query(stmt);
		search.BindString(1, hash);

		Map<int32, MapIndex*> res;
		while (search.StepRow())
		{
			int32 id = search.IntColumn(0);
			MapIndex** map = m_maps.Find(id);
			if (map)
			{
				res.Add(id, *map);
			}
		}

		return res;
	}
	
	Map<int32, MapIndex*> FindMaps(const String& searchString)
	{
		WString test = Utility::ConvertToWString(searchString);
		String stmt = "SELECT rowid FROM Maps WHERE";

		//search.spl
		Vector<String> terms = searchString.Explode(" ");
		int32 i = 0;
		for(auto term : terms)
		{
			if(i > 0)
				stmt += " AND";
			stmt += " (artist LIKE \"%" + term + "%\"" + 
				" OR title LIKE \"%" + term + "%\"" +
				" OR path LIKE \"%" + term + "%\"" +
				" OR tags LIKE \"%" + term + "%\")";
			i++;
		}

		Map<int32, MapIndex*> res;
		DBStatement search = m_database.Query(stmt);
		while(search.StepRow())
		{
			int32 id = search.IntColumn(0);
			MapIndex** map = m_maps.Find(id);
			if(map)
			{
				res.Add(id, *map);
			}
		}


		return res;
	}

	Map<int32, MapIndex*> FindMapsByFolder(const String& folder)
	{
		char csep[2];
		csep[0] = Path::sep;
		csep[1] = 0;
		String sep(csep);
		String stmt = "SELECT rowid FROM Maps WHERE path LIKE \"%" + sep + folder + sep + "%\"";

		Map<int32, MapIndex*> res;
		DBStatement search = m_database.Query(stmt);
		while (search.StepRow())
		{
			int32 id = search.IntColumn(0);
			MapIndex** map = m_maps.Find(id);
			if (map)
			{
				res.Add(id, *map);
			}
		}

		return res;
	}
	// Processes pending database changes
	void Update()
	{
		List<Event> changes = FlushChanges();
		if(changes.empty())
			return;

		DBStatement addDiff = m_database.Query("INSERT INTO Difficulties(path,lwt,metadata,rowid,mapid,hash) VALUES(?,?,?,?,?,?)");
		DBStatement addMap = m_database.Query("INSERT INTO Maps(path,artist,title,tags,rowid) VALUES(?,?,?,?,?)");
		DBStatement update = m_database.Query("UPDATE Difficulties SET lwt=?,metadata=?,hash=? WHERE rowid=?");
		DBStatement removeDiff = m_database.Query("DELETE FROM Difficulties WHERE rowid=?");
		DBStatement removeMap = m_database.Query("DELETE FROM Maps WHERE rowid=?");

		Set<MapIndex*> addedEvents;
		Set<MapIndex*> removeEvents;
		Set<MapIndex*> updatedEvents;

		m_database.Exec("BEGIN");
		for(Event& e : changes)
		{
			if(e.action == Event::Added)
			{
				Buffer metadata;
				MemoryWriter metadataWriter(metadata);
				metadataWriter.SerializeObject(*e.mapData);

				String mapPath = Path::RemoveLast(e.path, nullptr);
				bool existingUpdated;
				MapIndex* map;

				// Add or get map
				auto mapIt = m_mapsByPath.find(mapPath);
				if(mapIt == m_mapsByPath.end())
				{
					// Add map
					map = new MapIndex();
					map->id = m_nextMapId++;
					map->path = mapPath;
					map->selectId = m_maps.size();

					m_maps.Add(map->id, map);
					m_mapsByPath.Add(map->path, map);

					addMap.BindString(1, map->path);
					addMap.BindString(2, e.mapData->artist);
					addMap.BindString(3, e.mapData->title);
					addMap.BindString(4, e.mapData->tags);
					addMap.BindInt(5, map->id);
					addMap.Step();
					addMap.Rewind();

					existingUpdated = false; // New map
				}
				else
				{
					map = mapIt->second;
					existingUpdated = true; // Existing map
				}

				DifficultyIndex* diff = new DifficultyIndex();
				diff->id = m_nextDiffId++;
				diff->lwt = e.lwt;
				diff->mapId = map->id;
				diff->path = e.path;
				diff->settings = *e.mapData;
				diff->hash = e.hash;
				m_difficulties.Add(diff->id, diff);

				// Add diff to map and resort
				map->difficulties.Add(diff);
				m_SortDifficulties(map);

				// Add Diff
				addDiff.BindString(1, diff->path);
				addDiff.BindInt64(2, diff->lwt);
				addDiff.BindBlob(3, metadata);
				addDiff.BindInt64(4, diff->id); // rowid
				addDiff.BindInt64(5, diff->mapId); // mapid
				addDiff.BindString(6, diff->hash);
				addDiff.Step();
				addDiff.Rewind();

				// Send appropriate notification
				if(existingUpdated)
				{
					updatedEvents.Add(map);
				}
				else
				{
					addedEvents.Add(map);
				}
			}
			else if(e.action == Event::Updated)
			{
				Buffer metadata;
				MemoryWriter metadataWriter(metadata);
				metadataWriter.SerializeObject(*e.mapData);

				update.BindInt64(1, e.lwt);
				update.BindBlob(2, metadata);
				update.BindString(3, e.hash);
				update.BindInt(4, e.id);
				update.Step();
				update.Rewind();
				
				auto itDiff = m_difficulties.find(e.id);
				assert(itDiff != m_difficulties.end());

				itDiff->second->lwt = e.lwt;
				itDiff->second->settings = *e.mapData;
				itDiff->second->hash = e.hash;

				auto itMap = m_maps.find(itDiff->second->mapId);
				assert(itMap != m_maps.end());

				// Send notification
				updatedEvents.Add(itMap->second);
			}
			else if(e.action == Event::Removed)
			{
				auto itDiff = m_difficulties.find(e.id);
				assert(itDiff != m_difficulties.end());

				auto itMap = m_maps.find(itDiff->second->mapId);
				assert(itMap != m_maps.end());

				itMap->second->difficulties.Remove(itDiff->second);

				delete itDiff->second;
				m_difficulties.erase(e.id);

				// Remove diff in db
				removeDiff.BindInt(1, e.id);
				removeDiff.Step();
				removeDiff.Rewind();

				if(itMap->second->difficulties.empty()) // Remove map as well
				{
					removeEvents.Add(itMap->second);

					removeMap.BindInt(1, itMap->first);
					removeMap.Step();
					removeMap.Rewind();

					m_mapsByPath.erase(itMap->second->path);
					m_maps.erase(itMap);
				}
				else
				{
					updatedEvents.Add(itMap->second);
				}
			}
			if(e.mapData)
				delete e.mapData;
		}
		m_database.Exec("END");

		// Fire events
		if(!removeEvents.empty())
		{
			Vector<MapIndex*> eventsArray;
			for(auto i : removeEvents)
			{
				// Don't send 'updated' or 'added' events for removed maps
				addedEvents.erase(i);
				updatedEvents.erase(i);
				eventsArray.Add(i);
			}

			m_outer.OnMapsRemoved.Call(eventsArray);
			for(auto e : eventsArray)
			{
				delete e;
			}
		}
		if(!addedEvents.empty())
		{
			Vector<MapIndex*> eventsArray;
			for(auto i : addedEvents)
			{
				// Don't send 'updated' events for added maps
				updatedEvents.erase(i);
				eventsArray.Add(i);
			}

			m_outer.OnMapsAdded.Call(eventsArray);
		}
		if(!updatedEvents.empty())
		{
			Vector<MapIndex*> eventsArray;
			for(auto i : updatedEvents)
			{
				eventsArray.Add(i);
			}

			m_outer.OnMapsUpdated.Call(eventsArray);
		}
	}

	void AddScore(const DifficultyIndex& diff, int score, int crit, int almost, int miss, float gauge, uint32 gameflags, Vector<SimpleHitStat> simpleHitStats, uint64 timestamp)
	{
		DBStatement addScore = m_database.Query("INSERT INTO Scores(score,crit,near,miss,gauge,gameflags,hitstats,timestamp,diffid) VALUES(?,?,?,?,?,?,?,?,?)");
		Buffer hitstats;
		MemoryWriter hitstatWriter(hitstats);
		hitstatWriter.SerializeObject(simpleHitStats);

		m_database.Exec("BEGIN");

		addScore.BindInt(1, score);
		addScore.BindInt(2, crit);
		addScore.BindInt(3, almost);
		addScore.BindInt(4, miss);
		addScore.BindDouble(5, gauge);
		addScore.BindInt(6, gameflags);
		addScore.BindBlob(7, hitstats);
		addScore.BindInt64(8, timestamp);
		addScore.BindInt(9, diff.id);

		addScore.Step();
		addScore.Rewind();

		m_database.Exec("END");

	}

	DifficultyIndex* GetRandomDiff()
	{
		auto it = m_difficulties.begin();
		uint32 selection = Random::IntRange(0, (int32)m_difficulties.size() - 1);
		std::advance(it, selection);
		return it->second;
	}

private:
	void m_CleanupMapIndex()
	{
		for(auto m : m_maps)
		{
			delete m.second;
		}
		for(auto m : m_difficulties)
		{
			for (auto s : m.second->scores)
			{
				s->hitStats.clear();
				delete s;
			}
			m.second->scores.clear();
			delete m.second;
		}
		m_maps.clear();
		m_difficulties.clear();
	}
	void m_CreateTables()
	{
		m_database.Exec("DROP TABLE IF EXISTS Maps");
		m_database.Exec("DROP TABLE IF EXISTS Difficulties");
		m_database.Exec("DROP TABLE IF EXISTS Scores");

		m_database.Exec("CREATE TABLE Maps"
			"(artist TEXT, title TEXT, tags TEXT, path TEXT)");

		m_database.Exec("CREATE TABLE Difficulties"
			"(metadata BLOB, path TEXT, lwt INTEGER, mapid INTEGER, hash TEXT, "
			"FOREIGN KEY(mapid) REFERENCES Maps(rowid))");

		m_database.Exec("CREATE TABLE Scores"
			"(score INTEGER, crit INTEGER, near INTEGER, miss INTEGER, gauge REAL, gameflags INTEGER, diffid INTEGER, hitstats BLOB, timestamp INTEGER, "
			"FOREIGN KEY(diffid) REFERENCES Difficulties(rowid))");
	}
	void m_LoadInitialData()
	{
		assert(!m_searching);

		// Clear search state
		m_searchState.difficulties.clear();

		// Scan original maps
		m_CleanupMapIndex();

		// Select Maps
		DBStatement mapScan = m_database.Query("SELECT rowid,path FROM Maps ORDER BY " + m_sortField + " COLLATE NOCASE");
		while(mapScan.StepRow())
		{
			MapIndex* map = new MapIndex();
			map->id = mapScan.IntColumn(0);
			map->path = mapScan.StringColumn(1);
			map->selectId = m_maps.size();
			m_maps.Add(map->id, map);
			m_mapsByPath.Add(map->path, map);
		}
		m_nextMapId = m_maps.empty() ? 1 : (m_maps.rbegin()->first + 1);

		// Select Difficulties
		DBStatement diffScan = m_database.Query("SELECT rowid,path,lwt,metadata,mapid,hash FROM Difficulties");
		while(diffScan.StepRow())
		{
			DifficultyIndex* diff = new DifficultyIndex();
			diff->id = diffScan.IntColumn(0);
			diff->path = diffScan.StringColumn(1);
			diff->lwt = diffScan.Int64Column(2);
			Buffer metadata = diffScan.BlobColumn(3);
			diff->mapId = diffScan.IntColumn(4);
			MemoryReader metadataReader(metadata);
			metadataReader.SerializeObject(diff->settings);
			diff->hash = diffScan.StringColumnEmptyOnNull(5);

			// Add existing diff
			m_difficulties.Add(diff->id, diff);

			// Add difficulty to map and resort difficulties
			auto mapIt = m_maps.find(diff->mapId);
			assert(mapIt != m_maps.end());
			mapIt->second->difficulties.Add(diff);
			m_SortDifficulties(mapIt->second);

			// Add to search state
			SearchState::ExistingDifficulty ed;
			ed.id = diff->id;
			if (diff->hash.length() == 0)
			{
				ed.lwt = 0;
			}
			else {
				ed.lwt = diff->lwt;

			}
			m_searchState.difficulties.Add(diff->path, ed);
		}

		// Select Scores
		DBStatement scoreScan = m_database.Query("SELECT rowid,score,crit,near,miss,gauge,gameflags,hitstats,timestamp,diffid FROM Scores");
		
		while (scoreScan.StepRow())
		{
			ScoreIndex* score = new ScoreIndex();
			score->id = scoreScan.IntColumn(0);
			score->score = scoreScan.IntColumn(1);
			score->crit = scoreScan.IntColumn(2);
			score->almost = scoreScan.IntColumn(3);
			score->miss = scoreScan.IntColumn(4);
			score->gauge = scoreScan.DoubleColumn(5);
			score->gameflags = scoreScan.IntColumn(6);

			Buffer hitstats = scoreScan.BlobColumn(7);
			MemoryReader hitstatreader(hitstats);
			if(hitstats.size() > 0)
				hitstatreader.SerializeObject(score->hitStats);

			score->timestamp = scoreScan.Int64Column(8);
			score->diffid = scoreScan.IntColumn(9);

			// Add difficulty to map and resort difficulties
			auto diffIt = m_difficulties.find(score->diffid);
			if (diffIt == m_difficulties.end()) // If for whatever reason the diff that the score is attatched to is not in the db, ignore the score.
			{
				score->hitStats.clear();
				delete score;
				continue;
			}

			diffIt->second->scores.Add(score);
			m_SortScores(diffIt->second);
		}



		m_nextDiffId = m_difficulties.empty() ? 1 : (m_difficulties.rbegin()->first + 1);

		m_outer.OnMapsCleared.Call(m_maps);
	}
	void m_SortDifficulties(MapIndex* mapIndex)
	{
		mapIndex->difficulties.Sort([](DifficultyIndex* a, DifficultyIndex* b)
		{
			return a->settings.difficulty < b->settings.difficulty;
		});
	}

	void m_SortScores(DifficultyIndex* diffIndex)
	{
		diffIndex->scores.Sort([](ScoreIndex* a, ScoreIndex* b)
		{
			return a->score > b->score;
		});
	}

	// Main search thread
	void m_SearchThread()
	{
		Map<String, FileInfo> fileList;

		{
			ProfilerScope $("Chart Database - Enumerate Files and Folders");
			m_outer.OnSearchStatusUpdated.Call("[START] Chart Database - Enumerate Files and Folders");
			for(String rootSearchPath : m_searchPaths)
			{
				Vector<FileInfo> files = Files::ScanFilesRecursive(rootSearchPath, "ksh", &m_interruptSearch);
				if(m_interruptSearch)
					return;
				for(FileInfo& fi : files)
				{
					fileList.Add(fi.fullPath, fi);
				}
			}
			m_outer.OnSearchStatusUpdated.Call("[END] Chart Database - Enumerate Files and Folders");
		}

		{
			ProfilerScope $("Chart Database - Process Removed Files");
			m_outer.OnSearchStatusUpdated.Call("[START] Chart Database - Process Removed Files");
			// Process scanned files
			for(auto f : m_searchState.difficulties)
			{
				if(!fileList.Contains(f.first))
				{
					Event evt;
					evt.action = Event::Removed;
					evt.path = f.first;
					evt.id = f.second.id;
					AddChange(evt);
				}
			}
			m_outer.OnSearchStatusUpdated.Call("[END] Chart Database - Process Removed Files");
		}

		{
			ProfilerScope $("Chart Database - Process New Files");
			m_outer.OnSearchStatusUpdated.Call("[START] Chart Database - Process New Files");
			// Process scanned files
			for(auto f : fileList)
			{
				if(!m_searching)
					break;

				uint64 mylwt = f.second.lastWriteTime;
				Event evt;
				evt.lwt = mylwt;

				SearchState::ExistingDifficulty* existing = m_searchState.difficulties.Find(f.first);
				if(existing)
				{
					evt.id = existing->id;
					if(existing->lwt != mylwt)
					{
						// Map Updated
						evt.action = Event::Updated;
					}
					else
					{
						// Skip, not changed
						continue;
					}
				}
				else
				{
					// Map added
					evt.action = Event::Added;
				}

				Logf("Discovered Chart [%s]", Logger::Info, f.first);
				m_outer.OnSearchStatusUpdated.Call(Utility::Sprintf("Discovered Chart [%s]", f.first));
				// Try to read map metadata
				bool mapValid = false;
				File fileStream;
				Beatmap map;
				if(fileStream.OpenRead(f.first))
				{
					FileReader reader(fileStream);

					if(map.Load(reader, true))
					{
						mapValid = true;
					}
				}

				if(mapValid)
				{
					evt.mapData = new BeatmapSettings(map.GetMapSettings());

					ProfilerScope $("Chart Database - Hash Chart Audio");

					// TODO should we cache maps here for when the same file is used 3 times?
					const String audioFile = Path::Normalize(Path::RemoveLast(f.first) + Path::sep + evt.mapData->audioNoFX);

					File audioFileStream;
					if (audioFileStream.OpenRead(audioFile))
					{
						char data_buffer[0x8000];
						uint32_t digest[5];
						sha1::SHA1 s;

						size_t amount_read = 0;
						size_t read_size;
						do
						{
							read_size = audioFileStream.Read(data_buffer + amount_read, sizeof(data_buffer) - amount_read);
							amount_read += read_size;
						}
						while (amount_read < sizeof(data_buffer) && read_size != 0);
						
						s.processBytes(data_buffer, amount_read);
						s.getDigest(digest);

						evt.hash = Utility::Sprintf("%08x%08x%08x%08x%08x", digest[0], digest[1], digest[2], digest[3], digest[4]);
					}
					else
					{
						// If we can't open the file, the map isn't going to work, so remove it
						mapValid = false;
					}
				}

				if (!mapValid)
				{
					if(!existing) // Never added
					{
						Logf("Skipping corrupted chart [%s]", Logger::Warning, f.first);
						m_outer.OnSearchStatusUpdated.Call(Utility::Sprintf("Skipping corrupted chart [%s]", f.first));
						continue;
					}
					// Invalid maps get removed from the database
					evt.action = Event::Removed;
				}
				evt.path = f.first;
				AddChange(evt);
				continue;
			}
			m_outer.OnSearchStatusUpdated.Call("[END] Chart Database - Process New Files");
		}
		m_outer.OnSearchStatusUpdated.Call("");
		m_searching = false;
	}
};
MapDatabase::MapDatabase()
{
	m_impl = new MapDatabase_Impl(*this);
}
MapDatabase::~MapDatabase()
{
	delete m_impl;
}
void MapDatabase::Update()
{
	m_impl->Update();
}
bool MapDatabase::IsSearching() const
{
	return m_impl->m_searching;
}
void MapDatabase::StartSearching()
{
	m_impl->StartSearching();
}
void MapDatabase::StopSearching()
{
	m_impl->StopSearching();
}
Map<int32, MapIndex*> MapDatabase::FindMaps(const String& search)
{
	return m_impl->FindMaps(search);
}
Map<int32, MapIndex*> MapDatabase::FindMapsByHash(const String& hash)
{
	return m_impl->FindMapsByHash(hash);
}
Map<int32, MapIndex*> MapDatabase::FindMapsByFolder(const String & folder)
{
	return m_impl->FindMapsByFolder(folder);
}
MapIndex* MapDatabase::GetMap(int32 idx)
{
	MapIndex** mapIdx = m_impl->m_maps.Find(idx);
	return mapIdx ? *mapIdx : nullptr;
}
void MapDatabase::AddSearchPath(const String& path)
{
	m_impl->AddSearchPath(path);
}
void MapDatabase::RemoveSearchPath(const String& path)
{
	m_impl->RemoveSearchPath(path);
}
void MapDatabase::AddScore(const DifficultyIndex& diff, int score, int crit, int almost, int miss, float gauge, uint32 gameflags, Vector<SimpleHitStat> simpleHitStats, uint64 timestamp)
{
	m_impl->AddScore(diff, score, crit, almost, miss, gauge, gameflags, simpleHitStats, timestamp);
}
DifficultyIndex* MapDatabase::GetRandomDiff()
{
	return m_impl->GetRandomDiff();
}