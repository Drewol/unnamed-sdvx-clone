#include "stdafx.h"
#include "MapDatabase.hpp"
#include "Database.hpp"
#include "Beatmap.hpp"
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
	};
	List<Event> m_pendingChanges;
	mutex m_pendingChangesLock;

	static const int32 m_version = 8;

public:
	MapDatabase_Impl(MapDatabase& outer) : m_outer(outer)
	{
		String databasePath = "maps.db";
		if(!m_database.Open(databasePath))
		{
			Logf("Failed to open database [%s]", Logger::Warning, databasePath);
			assert(false);
		}

		bool rebuild = false;
		DBStatement versionQuery = m_database.Query("SELECT version FROM `Database`");
		if(versionQuery && versionQuery.Step())
		{
			int32 gotVersion = versionQuery.IntColumn(0);
			if(gotVersion != m_version)
			{
				rebuild = true;
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

		m_searchPaths.Add(path);
	}
	void RemoveSearchPath(const String& path)
	{
		String normalizedPath = Path::Normalize(Path::Absolute(path));
		if(!m_searchPaths.Contains(normalizedPath))
			return;

		m_searchPaths.erase(path);
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

	Map<int32, MapIndex*> GetMaps()
	{
		return m_maps;
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

		DBStatement addDiff = m_database.Query("INSERT INTO Difficulties(path,lwt,metadata,rowid,mapid) VALUES(?,?,?,?,?)");
		DBStatement addMap = m_database.Query("INSERT INTO Maps(path,artist,title,tags,rowid) VALUES(?,?,?,?,?)");
		DBStatement update = m_database.Query("UPDATE Difficulties SET lwt=?,metadata=? WHERE rowid=?");
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
				update.BindInt(3, e.id);
				update.Step();
				update.Rewind();
				
				auto itDiff = m_difficulties.find(e.id);
				assert(itDiff != m_difficulties.end());

				itDiff->second->lwt = e.lwt;
				itDiff->second->settings = *e.mapData;

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

	void AddScore(const DifficultyIndex& diff, int score, int crit, int almost, int miss, float gauge)
	{
		DBStatement addScore = m_database.Query("INSERT INTO Scores(score,crit,near,miss,gauge,diffid) VALUES(?,?,?,?,?,?)");

		m_database.Exec("BEGIN");

		addScore.BindInt(1, score);
		addScore.BindInt(2, crit);
		addScore.BindInt(3, almost);
		addScore.BindInt(4, miss);
		addScore.BindDouble(5, gauge);
		addScore.BindInt(6, diff.id);

		addScore.Step();
		addScore.Rewind();

		m_database.Exec("END");

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
			"(metadata BLOB, path TEXT, lwt INTEGER, mapid INTEGER,"
			"FOREIGN KEY(mapid) REFERENCES Maps(rowid))");

		m_database.Exec("CREATE TABLE Scores"
			"(score INTEGER, crit INTEGER, near INTEGER, miss INTEGER, gauge REAL, diffid INTEGER,"
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
		DBStatement mapScan = m_database.Query("SELECT rowid,path FROM Maps");
		while(mapScan.StepRow())
		{
			MapIndex* map = new MapIndex();
			map->id = mapScan.IntColumn(0);
			map->path = mapScan.StringColumn(1);
			m_maps.Add(map->id, map);
			m_mapsByPath.Add(map->path, map);
		}
		m_nextMapId = m_maps.empty() ? 1 : (m_maps.rbegin()->first + 1);

		// Select Difficulties
		DBStatement diffScan = m_database.Query("SELECT rowid,path,lwt,metadata,mapid FROM Difficulties");
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

			// Add existing diff
			m_difficulties.Add(diff->id, diff);

			SearchState::ExistingDifficulty existing;
			existing.lwt = diff->lwt;
			existing.id = diff->id;
			m_searchState.difficulties.Add(diff->path, existing);

			// Add difficulty to map and resort difficulties
			auto mapIt = m_maps.find(diff->mapId);
			assert(mapIt != m_maps.end());
			mapIt->second->difficulties.Add(diff);
			m_SortDifficulties(mapIt->second);

			// Add to search state
			SearchState::ExistingDifficulty ed;
			ed.id = diff->id;
			ed.lwt = diff->lwt;
			m_searchState.difficulties.Add(diff->path, ed);
		}

		// Select Scores
		DBStatement scoreScan = m_database.Query("SELECT rowid,score,crit,near,miss,gauge,diffid FROM Scores");
		while (scoreScan.StepRow())
		{
			ScoreIndex* score = new ScoreIndex();
			score->id = scoreScan.IntColumn(0);
			score->score = scoreScan.IntColumn(1);
			score->crit = scoreScan.IntColumn(2);
			score->almost = scoreScan.IntColumn(3);
			score->miss = scoreScan.IntColumn(4);
			score->gauge = scoreScan.DoubleColumn(5);
			score->diffid = scoreScan.IntColumn(6);

			// Add difficulty to map and resort difficulties
			auto diffIt = m_difficulties.find(score->diffid);
			if(diffIt == m_difficulties.end()) // If for whatever reason the diff that the score is attatched to is not in the db, ignore the score.
				continue;

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
			return a->score < b->score;
		});
	}

	// Main search thread
	void m_SearchThread()
	{
		Map<String, FileInfo> fileList;

		{
			ProfilerScope $("Map Database - Enumerate Files and Folders");
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
		}

		{
			ProfilerScope $("Map Database - Process Removed Files");

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
		}

		{
			ProfilerScope $("Map Database - Process New Files");

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

				Logf("Discovered Map [%s]", Logger::Info, f.first);

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
				}
				else
				{
					if(!existing) // Never added
					{
						Logf("Skipping corrupted map [%s]", Logger::Warning, f.first);
						continue;
					}
					// Invalid maps get removed from the database
					evt.action = Event::Removed;
				}
				evt.path = f.first;
				AddChange(evt);
				continue;
			}
		}
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
Map<int32, MapIndex*> MapDatabase::GetMaps()
{
	return m_impl->GetMaps();
}
Map<int32, MapIndex*> MapDatabase::FindMaps(const String& search)
{
	return m_impl->FindMaps(search);
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
void MapDatabase::AddScore(const DifficultyIndex& diff, int score, int crit, int almost, int miss, float gauge)
{
	m_impl->AddScore(diff, score, crit, almost, miss, gauge);
}
