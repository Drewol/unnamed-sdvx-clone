#include "stdafx.h"
#include "MapDatabase.hpp"
#include "Database.hpp"
#include "Beatmap.hpp"
#include "TinySHA1.hpp"
#include "Shared/Profiling.hpp"
#include "Shared/Files.hpp"
#include "Shared/Time.hpp"
#include "KShootMap.hpp"
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
#include <iostream>
#include <fstream>
#include <condition_variable>
using std::thread;
using std::mutex;
using namespace std;

class MapDatabase_Impl
{
public:
	// For calling delegates
	MapDatabase& m_outer;

	thread m_thread;
	condition_variable m_cvPause;
	mutex m_pauseMutex;
	std::atomic<bool> m_paused;
	bool m_searching = false;
	bool m_interruptSearch = false;
	Set<String> m_searchPaths;
	Database m_database;

	Map<int32, FolderIndex*> m_folders;
	Map<int32, ChartIndex*> m_charts;
	Map<int32, ChallengeIndex*> m_challenges;
	Map<int32, PracticeSetupIndex*> m_practiceSetups;

	Map<String, ChartIndex*> m_chartsByHash;
	Map<String, FolderIndex*> m_foldersByPath;
	Multimap<int32, PracticeSetupIndex*> m_practiceSetupsByChartId;

	int32 m_nextFolderId = 1;
	int32 m_nextChartId = 1;
	int32 m_nextChalId = 1;
	String m_sortField = "title";
	bool m_transferScores = true;

	struct SearchState
	{
		struct ExistingFileEntry
		{
			int32 id;
			uint64 lwt;
		};
		// Maps file paths to the id's and last write time's for difficulties already in the database
		Map<String, ExistingFileEntry> difficulties;
		Map<String, ExistingFileEntry> challenges;
	} m_searchState;

	// Represents an event produced from a scan
	//	a difficulty can be removed/added/updated
	//	a BeatmapSettings structure will be provided for added/updated events
	struct Event
	{
		enum Type{
			Chart,
			Challenge
		};
		Type type;
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
		nlohmann::json json;
		String hash;
	};
	List<Event> m_pendingChanges;
	mutex m_pendingChangesLock;

	static const int32 m_version = 19;

public:
	MapDatabase_Impl(MapDatabase& outer, bool transferScores) : m_outer(outer)
	{
		m_transferScores = transferScores;
		String databasePath = Path::Absolute("maps.db");
		if(!m_database.Open(databasePath))
		{
			Logf("Failed to open database [%s]", Logger::Severity::Warning, databasePath);
			assert(false);
		}
		m_paused.store(false);
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
			ProfilerScope $(Utility::Sprintf("Upgrading db (%d -> %d)", gotVersion, m_version));

			//back up old db file
			Path::Copy(Path::Absolute("maps.db"), Path::Absolute("maps.db_" + Shared::Time::Now().ToString() + ".bak"));

			m_outer.OnDatabaseUpdateStarted.Call(1);


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
			if (gotVersion == 11) //upgrade from 11 to 12
			{
				m_database.Exec("CREATE TABLE Collections"
					"(collection TEXT, mapid INTEGER, "
					"UNIQUE(collection,mapid), "
					"FOREIGN KEY(mapid) REFERENCES Maps(rowid))");
				gotVersion = 12;
			}
			if (gotVersion == 12) //upgrade from 12 to 13
			{

				int diffCount = 1;
				{
					// Do in its own scope so that it will destruct before we modify anything else
					DBStatement diffCountStmt = m_database.Query("SELECT COUNT(rowid) FROM Difficulties");
					if (diffCountStmt.StepRow())
					{
						diffCount = diffCountStmt.IntColumn(0);
					}
				}
				m_outer.OnDatabaseUpdateProgress.Call(0, diffCount);

				int progress = 0;
				int totalScoreCount = 0;

				DBStatement diffScan = m_database.Query("SELECT rowid,path FROM Difficulties");

				Vector<ScoreIndex> scoresToAdd;
				while (diffScan.StepRow())
				{
					if (progress % 16 == 0)
						m_outer.OnDatabaseUpdateProgress.Call(progress, diffCount);
					progress++;

					bool noScores = true;
					int diffid = diffScan.IntColumn(0);
					String diffpath = diffScan.StringColumn(1);
					DBStatement scoreCount = m_database.Query("SELECT COUNT(*) FROM scores WHERE diffid=?");
					scoreCount.BindInt(1, diffid);
					if (scoreCount.StepRow())
					{
						noScores = scoreCount.IntColumn(0) == 0;
					}

					if (noScores)
					{
						continue;
					}

					String hash;
					File diffFile;
					if (diffFile.OpenRead(diffpath))
					{
						char data_buffer[0x80];
						uint32_t digest[5];
						sha1::SHA1 s;

						size_t amount_read = 0;
						size_t read_size;
						do
						{
							read_size = diffFile.Read(data_buffer, sizeof(data_buffer));
							amount_read += read_size;
							s.processBytes(data_buffer, read_size);
						} while (read_size != 0);

						s.getDigest(digest);
						hash = Utility::Sprintf("%08x%08x%08x%08x%08x", digest[0], digest[1], digest[2], digest[3], digest[4]);
					}
					else {
						Logf("Could not open chart file at \"%s\" scores will be lost.", Logger::Severity::Warning, diffpath);
						continue;
					}


					DBStatement scoreScan = m_database.Query("SELECT rowid,score,crit,near,miss,gauge,gameflags,hitstats,timestamp,diffid FROM Scores WHERE diffid=?");
					scoreScan.BindInt(1, diffid);
					while (scoreScan.StepRow())
					{
						ScoreIndex score;
						score.id = scoreScan.IntColumn(0);
						score.score = scoreScan.IntColumn(1);
						score.crit = scoreScan.IntColumn(2);
						score.almost = scoreScan.IntColumn(3);
						score.miss = scoreScan.IntColumn(4);
						score.gauge = (float) scoreScan.DoubleColumn(5);
						score.gaugeOption = scoreScan.IntColumn(6);
						Buffer hitstats = scoreScan.BlobColumn(7);
						score.timestamp = scoreScan.Int64Column(8);
						auto timestamp = Shared::Time(score.timestamp);
						score.chartHash = hash;
						score.replayPath = Path::Normalize(Path::Absolute("replays/" + hash + "/" + timestamp.ToString() + ".urf"));
						Path::CreateDir(Path::Absolute("replays/" + hash));
						File replayFile;
						if (replayFile.OpenWrite(score.replayPath))
						{
							replayFile.Write(hitstats.data(), hitstats.size());
						}
						else
						{
							Logf("Could not open replay file at \"%s\" replay data will be lost.", Logger::Severity::Warning, score.replayPath);
						}
						scoresToAdd.Add(score);
						totalScoreCount++;
					}

				}

				progress = 0;

				m_database.Exec("DROP TABLE IF EXISTS Maps");
				m_database.Exec("DROP TABLE IF EXISTS Difficulties");
				m_CreateTables();

				DBStatement addScore = m_database.Query("INSERT INTO Scores(score,crit,near,miss,gauge,gameflags,replay,timestamp,chart_hash) VALUES(?,?,?,?,?,?,?,?,?)");
				
				m_database.Exec("BEGIN");
				for (ScoreIndex& score : scoresToAdd)
				{
					addScore.BindInt(1, score.score);
					addScore.BindInt(2, score.crit);
					addScore.BindInt(3, score.almost);
					addScore.BindInt(4, score.miss);
					addScore.BindDouble(5, score.gauge);
					addScore.BindInt(6, score.gaugeOption);
					addScore.BindString(7, score.replayPath);
					addScore.BindInt64(8, score.timestamp);
					addScore.BindString(9, score.chartHash);

					addScore.Step();
					addScore.Rewind();

					if (progress % 16 == 0)
					{
						m_outer.OnDatabaseUpdateProgress.Call(progress, totalScoreCount);
					}
					progress++;
				}
				m_database.Exec("END");
				m_database.Exec("VACUUM");
				gotVersion = 13;
			}
			if (gotVersion == 13) //from 13 to 14
			{
				m_database.Exec("ALTER TABLE Charts ADD COLUMN custom_offset INTEGER");
				m_database.Exec("ALTER TABLE Scores ADD COLUMN user_name TEXT");
				m_database.Exec("ALTER TABLE Scores ADD COLUMN user_id TEXT");
				m_database.Exec("ALTER TABLE Scores ADD COLUMN local_score INTEGER");
				m_database.Exec("UPDATE Charts SET custom_offset=0");
				m_database.Exec("UPDATE Scores SET local_score=1");
				m_database.Exec("UPDATE Scores SET local_score=1");
				m_database.Exec("UPDATE Scores SET user_name=\"\"");
				m_database.Exec("UPDATE Scores SET user_id=\"\"");
				gotVersion = 14;
			}
			if (gotVersion == 14)
			{
				m_database.Exec("CREATE TABLE PracticeSetups ("
					"chart_id INTEGER,"
					"setup_title TEXT,"
					"loop_success INTEGER,"
					"loop_fail INTEGER,"
					"range_begin INTEGER,"
					"range_end INTEGER,"
					"fail_cond_type INTEGER,"
					"fail_cond_value INTEGER,"
					"playback_speed REAL,"
					"inc_speed_on_success INTEGER,"
					"inc_speed REAL,"
					"inc_streak INTEGER,"
					"dec_speed_on_fail INTEGER,"
					"dec_speed REAL,"
					"min_playback_speed REAL,"
					"max_rewind INTEGER,"
					"max_rewind_measure INTEGER,"
					"FOREIGN KEY(chart_id) REFERENCES Charts(rowid)"
				")");
				gotVersion = 15;
			}
			if (gotVersion == 15)
			{
				m_database.Exec("ALTER TABLE Scores ADD COLUMN window_perfect INTEGER");
				m_database.Exec("ALTER TABLE Scores ADD COLUMN window_good INTEGER");
				m_database.Exec("ALTER TABLE Scores ADD COLUMN window_hold INTEGER");
				m_database.Exec("ALTER TABLE Scores ADD COLUMN window_miss INTEGER");
				m_database.Exec("UPDATE Scores SET window_perfect=46");
				m_database.Exec("UPDATE Scores SET window_good=150");
				m_database.Exec("UPDATE Scores SET window_hold=150");
				m_database.Exec("UPDATE Scores SET window_miss=300");
				gotVersion = 16;
			}
			if (gotVersion == 16)
			{
				m_database.Exec("CREATE TABLE Challenges"
					"("
					"title TEXT,"
					"charts TEXT,"
					"chart_meta TEXT," // used for search
					"clear_mark INTEGER,"
					"best_score INTEGER,"
					"req_text TEXT,"
					"path TEXT,"
					"hash TEXT,"
					"level INTEGER,"
					"lwt INTEGER"
					")");
				gotVersion = 17;
			}
			if (gotVersion == 17)
			{
				Map<int32, PlaybackOptions> optionMap;
				int totalScoreCount = 0;
				DBStatement scoreScan = m_database.Query("SELECT rowid,gameflags FROM Scores");
				while (scoreScan.StepRow())
				{
					optionMap.Add(scoreScan.IntColumn(0), PlaybackOptions::FromFlags(scoreScan.IntColumn(1)));
					totalScoreCount++;
				}

				m_outer.OnDatabaseUpdateProgress.Call(0, totalScoreCount);
				int progress = 0;

				//alter table.
				// if we were on a newer sqlite version the gameflags column could easily be renamed but it will
				// instead still exist in the db after this update but will be unused.
				m_database.Exec("BEGIN");
				m_database.Exec("ALTER TABLE Scores ADD COLUMN gauge_type INTEGER");
				m_database.Exec("ALTER TABLE Scores ADD COLUMN auto_flags INTEGER");
				m_database.Exec("ALTER TABLE Scores ADD COLUMN gauge_opt INTEGER");
				m_database.Exec("ALTER TABLE Scores ADD COLUMN mirror INTEGER");
				m_database.Exec("ALTER TABLE Scores ADD COLUMN random INTEGER");

				DBStatement setScoreOpt = m_database.Query("UPDATE Scores set gauge_type=?, gauge_opt=?, mirror=?, random=?, auto_flags=? WHERE rowid=?");
				for (auto& o : optionMap)
				{
					setScoreOpt.BindInt(1, (int32)o.second.gaugeType);
					setScoreOpt.BindInt(2, o.second.gaugeOption);
					setScoreOpt.BindInt(3, o.second.mirror ? 1 : 0);
					setScoreOpt.BindInt(4, o.second.random ? 1 : 0);
					setScoreOpt.BindInt(5, (int32)o.second.autoFlags);
					setScoreOpt.BindInt(6, o.first);

					setScoreOpt.StepRow();
					setScoreOpt.Rewind();

					progress++;
					m_outer.OnDatabaseUpdateProgress.Call(progress, totalScoreCount);
				}
				m_database.Exec("END");
				gotVersion = 18;

			}
			if (gotVersion == 18)
			{
				m_database.Exec("ALTER TABLE Scores ADD COLUMN window_slam INTEGER");
				m_database.Exec("UPDATE Scores SET window_slam=84");
				gotVersion = 19;
			}
			m_database.Exec(Utility::Sprintf("UPDATE Database SET `version`=%d WHERE `rowid`=1", m_version));

			m_outer.OnDatabaseUpdateDone.Call();
		}
		else
		{
			// NOTE: before we loaded the database here. This was redundant since we always do StartSearching
			//       Plus we don't do this when doing an update so if it was a problem to not do it here we would
			//       have already noticed it being broken in a launch where the db is updated
			//       Also with challenges we can't do this until the constructor is done so we can access the
			//       MapDatabase wrapper while loading challenges
			//m_LoadInitialData();
		}
	}
	~MapDatabase_Impl()
	{
		StopSearching();
		m_CleanupMapIndex();

		//discard pending changes, probably should apply them(?)
		auto changes = FlushChanges();
		for (auto& c : changes)
		{
			if(c.mapData)
				delete c.mapData;
		}
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
		ResumeSearching();
		m_interruptSearch = false;
		m_searching = true;
		m_thread = thread(&MapDatabase_Impl::m_SearchThread, this);
	}
	void StopSearching()
	{
		ResumeSearching();
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

	// TODO(itszn) make sure this is not case sensitive
	ChartIndex* FindFirstChartByPath(const String& searchString)
	{
		String stmt = "SELECT DISTINCT rowid FROM Charts WHERE path LIKE ? LIMIT 1";

		DBStatement search = m_database.Query(stmt);
		search.BindString(1, "%"+searchString+"%");
		while(search.StepRow())
		{
			int32 id = search.IntColumn(0);
			ChartIndex** chart = m_charts.Find(id);
			if (!chart)
				return nullptr;
			return *chart;
		}

		return nullptr;
	}

	ChartIndex* FindFirstChartByNameAndLevel(const String& name, uint32 level, bool exact=true)
	{
		String stmt = "SELECT DISTINCT rowid FROM Charts WHERE title LIKE ? and level=? LIMIT 1";

		DBStatement search = m_database.Query(stmt);
		if (exact)
			search.BindString(1, name);
		else
			search.BindString(1, "%"+name+"%");
		search.BindInt(2, level);
		while(search.StepRow())
		{
			int32 id = search.IntColumn(0);
			ChartIndex** chart = m_charts.Find(id);
			if (!chart)
				return nullptr;
			return *chart;
		}

		// Try non exact now
		if (exact)
			return FindFirstChartByNameAndLevel(name, level, false);

		return nullptr;
	}

	ChartIndex* FindFirstChartByHash(const String& hash)
	{
		ChartIndex** chart = m_chartsByHash.Find(hash);
		if (!chart)
			return nullptr;
		return *chart;
	}

	Map<int32, FolderIndex*> FindFoldersByHash(const String& hash)
	{
		String stmt = "SELECT DISTINCT folderId FROM Charts WHERE hash = ?";
		DBStatement search = m_database.Query(stmt);
		search.BindString(1, hash);

		Map<int32, FolderIndex*> res;
		while (search.StepRow())
		{
			int32 id = search.IntColumn(0);
			FolderIndex** folder = m_folders.Find(id);
			if (folder)
			{
				res.Add(id, *folder);
			}
		}

		return res;
	}

	// TODO(itszn) make this not case sensitive
	Map<int32, FolderIndex*> FindFoldersByPath(const String& searchString)
	{
		String stmt = "SELECT DISTINCT folderId FROM Charts WHERE path LIKE ?";
		DBStatement search = m_database.Query(stmt);
		search.BindString(1, "%" + searchString + "%");

		Map<int32, FolderIndex*> res;
		while(search.StepRow())
		{
			int32 id = search.IntColumn(0);
			FolderIndex** folder = m_folders.Find(id);
			if(folder)
			{
				res.Add(id, *folder);
			}
		}

		return res;
	}

	Map<int32, ChallengeIndex*> FindChallenges(const String& searchString)
	{
		WString test = Utility::ConvertToWString(searchString);
		String stmt = "SELECT DISTINCT rowid FROM Challenges WHERE";

		Vector<String> terms = searchString.Explode(" ");
		int32 i = 0;
		for (auto term : terms)
		{
			if (i > 0)
				stmt += " AND";
			stmt += String(" (title LIKE ?") +
				" OR chart_meta LIKE ?"
				" OR path LIKE ?)";
			i++;
		}
		DBStatement search = m_database.Query(stmt);

		i = 1;
		for (auto term : terms)
		{
			// Bind all the terms
			for (int j = 0; j < 3; j++)
			{
				search.BindString(i+j, "%" + term + "%");
			}
			i+=6;
		}
		
		Map<int32, ChallengeIndex*> res;
		while(search.StepRow())
		{
			int32 id = search.IntColumn(0);
			ChallengeIndex** challenge = m_challenges.Find(id);
			if(challenge)
			{
				res.Add(id, *challenge);
			}
		}

		return res;
	}
	
	Map<int32, FolderIndex*> FindFolders(const String& searchString)
	{
		WString test = Utility::ConvertToWString(searchString);
		String stmt = "SELECT DISTINCT folderId FROM Charts WHERE";

		Vector<String> terms = searchString.Explode(" ");
		int32 i = 0;
		for(auto term : terms)
		{
			if(i > 0)
				stmt += " AND";
			stmt += String(" (artist LIKE ?") +
				" OR title LIKE ?" +
				" OR path LIKE ?" +
				" OR effector LIKE ?" +
				" OR artist_translit LIKE ?" +
				" OR title_translit LIKE ?)";
			i++;
		}
		DBStatement search = m_database.Query(stmt);

		i = 1;
		for (auto term : terms)
		{
			// Bind all the terms
			for (int j = 0; j < 6; j++)
			{
				search.BindString(i+j, "%" + term + "%");
			}
			i+=6;
		}

		Map<int32, FolderIndex*> res;
		while(search.StepRow())
		{
			int32 id = search.IntColumn(0);
			FolderIndex** folder = m_folders.Find(id);
			if(folder)
			{
				res.Add(id, *folder);
			}
		}


		return res;
	}

	Vector<String> GetCollections()
	{
		Vector<String> res;
		DBStatement search = m_database.Query("SELECT DISTINCT collection FROM collections");
		while (search.StepRow())
		{
			res.Add(search.StringColumn(0));
		}
		return res;
	}

	Vector<String> GetCollectionsForMap(int32 mapid)
	{
		Vector<String> res;
		DBStatement search = m_database.Query(Utility::Sprintf("SELECT DISTINCT collection FROM collections WHERE folderid==%d", mapid));
		while (search.StepRow())
		{
			res.Add(search.StringColumn(0));
		}
		return res;
	}

	Vector<PracticeSetupIndex*> GetOrAddPracticeSetups(int32 chartId)
	{
		Vector<PracticeSetupIndex*> res;

		auto it = m_practiceSetupsByChartId.equal_range(chartId);
		for (auto it1 = it.first; it1 != it.second; ++it1)
		{
			res.Add(it1->second);
		}

		if (res.empty())
		{
			PracticeSetupIndex* practiceSetup = new PracticeSetupIndex();
			practiceSetup->id = -1;
			practiceSetup->chartId = chartId;

			practiceSetup->setupTitle = "";
			practiceSetup->loopSuccess = 0;
			practiceSetup->loopFail = 1;
			practiceSetup->rangeBegin = 0;
			practiceSetup->rangeEnd = 0;
			practiceSetup->failCondType = 0;
			practiceSetup->failCondValue = 0;
			practiceSetup->playbackSpeed = 1.0;

			practiceSetup->incSpeedOnSuccess = 0;
			practiceSetup->incSpeed = 0.0;
			practiceSetup->incStreak = 1;

			practiceSetup->decSpeedOnFail = 0;
			practiceSetup->decSpeed = 0.0;
			practiceSetup->minPlaybackSpeed = 0.1;

			practiceSetup->maxRewind = 0;
			practiceSetup->maxRewindMeasure = 1;

			UpdateOrAddPracticeSetup(practiceSetup);
			res.emplace_back(practiceSetup);
		}

		return res;
	}

	Map<int32, FolderIndex*> FindFoldersByCollection(const String& collection)
	{
		String stmt = "SELECT folderid FROM Collections WHERE collection==?";
		DBStatement search = m_database.Query(stmt);
		search.BindString(1, collection);

		Map<int32, FolderIndex*> res;
		while (search.StepRow())
		{
			int32 id = search.IntColumn(0);
			FolderIndex** folder = m_folders.Find(id);
			if (folder)
			{
				res.Add(id, *folder);
			}
		}

		return res;
	}


	Map<int32, FolderIndex*> FindFoldersByFolder(const String& folder)
	{
		char csep[2];
		csep[0] = Path::sep;
		csep[1] = 0;
		String sep(csep);
		String stmt = "SELECT rowid FROM folders WHERE path LIKE ?";
		DBStatement search = m_database.Query(stmt);
		search.BindString(1, "%" + sep + folder + sep + "%");


		Map<int32, FolderIndex*> res;
		while (search.StepRow())
		{
			int32 id = search.IntColumn(0);
			FolderIndex** folder = m_folders.Find(id);
			if (folder)
			{
				res.Add(id, *folder);
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

		DBStatement addChart = m_database.Query("INSERT INTO Charts("
			"folderId,path,title,artist,title_translit,artist_translit,jacket_path,effector,illustrator,"
			"diff_name,diff_shortname,bpm,diff_index,level,hash,preview_file,preview_offset,preview_length,lwt) "
			"VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
		DBStatement addFolder = m_database.Query("INSERT INTO Folders(path,rowid) VALUES(?,?)");
		DBStatement addChallenge = m_database.Query("INSERT INTO Challenges("
			"title,charts,chart_meta,clear_mark,best_score,req_text,path,hash,level,lwt) "
			"VALUES(?,?,?,?,?,?,?,?,?,?)");
		DBStatement update = m_database.Query("UPDATE Charts SET path=?,title=?,artist=?,title_translit=?,artist_translit=?,jacket_path=?,effector=?,illustrator=?,"
			"diff_name=?,diff_shortname=?,bpm=?,diff_index=?,level=?,hash=?,preview_file=?,preview_offset=?,preview_length=?,lwt=? WHERE rowid=?"); //TODO: update
		DBStatement updateChallenge = m_database.Query("UPDATE Challenges SET title=?,charts=?,chart_meta=?,clear_mark=?,best_score=?,req_text=?,path=?,hash=?,level=?,lwt=? WHERE rowid=?");
		DBStatement removeChart = m_database.Query("DELETE FROM Charts WHERE rowid=?");
		DBStatement removeChallenge = m_database.Query("DELETE FROM Challenges WHERE rowid=?");
		DBStatement removeFolder = m_database.Query("DELETE FROM Folders WHERE rowid=?");
		DBStatement scoreScan = m_database.Query("SELECT "
			"rowid,score,crit,near,miss,gauge,auto_flags,replay,timestamp,chart_hash,user_name,user_id,local_score,window_perfect,window_good,window_hold,window_miss,window_slam,gauge_type,gauge_opt,mirror,random "
			"FROM Scores WHERE chart_hash=?");
		DBStatement moveScores = m_database.Query("UPDATE Scores set chart_hash=? where chart_hash=?");

		Set<FolderIndex*> addedChartEvents;
		Set<FolderIndex*> removeChartEvents;
		Set<FolderIndex*> updatedChartEvents;

		Set<ChallengeIndex*> addedChalEvents;
		Set<ChallengeIndex*> removeChalEvents;
		Set<ChallengeIndex*> updatedChalEvents;

		const String diffShortNames[4] = { "NOV", "ADV", "EXH", "INF" };
		const String diffNames[4] = { "Novice", "Advanced", "Exhaust", "Infinite" };

		m_database.Exec("BEGIN");
		for(Event& e : changes)
		{
			if (e.type == Event::Challenge && (e.action == Event::Added || e.action == Event::Updated))
			{
				ChallengeIndex* chal;
				if (e.action == Event::Added)
				{
					chal = new ChallengeIndex();
					chal->id = m_nextChalId++;
				}
				else
				{
					auto itChal = m_challenges.find(e.id);
					assert(itChal != m_challenges.end());
					chal = itChal->second;
				}

				if (e.json.is_discarded() || e.json.is_null())
				{
					Log("Tried to process invalid json in Challenge Add event", Logger::Severity::Warning);
					continue;
				}
				chal->settings = e.json;
				chal->settings["title"].get_to(chal->title);
				chal->path = e.path;
				chal->settings["level"].get_to(chal->level);
				if (e.action == Event::Added)
				{
					chal->clearMark = 0;
					chal->bestScore = 0;
				}
				chal->hash = e.hash;
				chal->missingChart = false;
				chal->lwt = e.lwt;
				chal->charts.clear();

				String chartMeta = "";
				// Grab the charts
				chal->FindCharts(&m_outer, chal->settings["charts"]);
				chal->GenerateDescription();

				String chartString = chal->settings["charts"].dump();

				if (e.action == Event::Added)
				{
					m_challenges.Add(chal->id, chal);

					// Add Chart
					// ("title,charts,chart_meta,clear_mark,best_score,req_text,path,hash,level,lwt) "
					addChallenge.BindString(1, chal->title);
					addChallenge.BindString(2, chartString);
					addChallenge.BindString(3, chartMeta);
					addChallenge.BindInt(4, chal->clearMark);
					addChallenge.BindInt(5, chal->bestScore);
					addChallenge.BindString(6, chal->reqText);
					addChallenge.BindString(7, chal->path);
					addChallenge.BindString(8, chal->hash);
					addChallenge.BindInt(9, chal->level);
					addChallenge.BindInt64(10, chal->lwt);

					addChallenge.Step();
					addChallenge.Rewind();

					addedChalEvents.Add(chal);
				}
				else if (e.action == Event::Updated)
				{
					updateChallenge.BindString(1, chal->title);
					updateChallenge.BindString(2, chartString);
					updateChallenge.BindString(3, chartMeta);
					updateChallenge.BindInt(4, chal->clearMark);
					updateChallenge.BindInt(5, chal->bestScore);
					updateChallenge.BindString(6, chal->reqText);
					updateChallenge.BindString(7, chal->path);
					updateChallenge.BindString(8, chal->hash);
					updateChallenge.BindInt(9, chal->level);
					updateChallenge.BindInt64(10, chal->lwt);
					updateChallenge.BindInt(11, e.id);

					updateChallenge.Step();
					updateChallenge.Rewind();

					updatedChalEvents.Add(chal);
				}
			}
			else if(e.type == Event::Challenge && e.action == Event::Removed)
			{
				auto itChal = m_challenges.find(e.id);
				assert(itChal != m_challenges.end());

				delete itChal->second;
				m_challenges.erase(e.id);

				// Remove diff in db
				removeChallenge.BindInt(1, e.id);
				removeChallenge.Step();
				removeChallenge.Rewind();
			}
			if(e.type == Event::Chart && e.action == Event::Added)
			{
				String folderPath = Path::RemoveLast(e.path, nullptr);
				bool existingUpdated;
				FolderIndex* folder;

				// Add or get folder
				auto folderIt = m_foldersByPath.find(folderPath);
				if(folderIt == m_foldersByPath.end())
				{
					// Add folder
					folder = new FolderIndex();
					folder->id = m_nextFolderId++;
					folder->path = folderPath;
					folder->selectId = (int32) m_folders.size();

					m_folders.Add(folder->id, folder);
					m_foldersByPath.Add(folder->path, folder);

					addFolder.BindString(1, folder->path);
					addFolder.BindInt(2, folder->id);
					addFolder.Step();
					addFolder.Rewind();

					existingUpdated = false; // New folder
				}
				else
				{
					folder = folderIt->second;
					existingUpdated = true; // Existing folder
				}


				ChartIndex* chart = new ChartIndex();
				chart->id = m_nextChartId++;
				chart->lwt = e.lwt;
				chart->folderId = folder->id;
				chart->path = e.path;
				chart->title = e.mapData->title;
				chart->artist = e.mapData->artist;
				chart->level = e.mapData->level;
				chart->effector = e.mapData->effector;
				chart->preview_file = e.mapData->audioNoFX;
				chart->preview_offset = e.mapData->previewOffset;
				chart->preview_length = e.mapData->previewDuration;
				chart->diff_index = e.mapData->difficulty;
				chart->diff_name = diffNames[e.mapData->difficulty];
				chart->diff_shortname = diffShortNames[e.mapData->difficulty];
				chart->bpm = e.mapData->bpm;
				chart->illustrator = e.mapData->illustrator;
				chart->jacket_path = e.mapData->jacketPath;
				chart->hash = e.hash;

				// Check for existing scores for this chart
				scoreScan.BindString(1, chart->hash);
				while (scoreScan.StepRow())
				{
					ScoreIndex* score = new ScoreIndex();
					score->id = scoreScan.IntColumn(0);
					score->score = scoreScan.IntColumn(1);
					score->crit = scoreScan.IntColumn(2);
					score->almost = scoreScan.IntColumn(3);
					score->miss = scoreScan.IntColumn(4);
					score->gauge = (float)scoreScan.DoubleColumn(5);
					score->autoFlags = (AutoFlags)scoreScan.IntColumn(6);
					score->replayPath = scoreScan.StringColumn(7);

					score->timestamp = scoreScan.Int64Column(8);
					score->chartHash = scoreScan.StringColumn(9);
					score->userName = scoreScan.StringColumn(10);
					score->userId = scoreScan.StringColumn(11);
					score->localScore = scoreScan.IntColumn(12);

					score->hitWindowPerfect = scoreScan.IntColumn(13);
					score->hitWindowGood = scoreScan.IntColumn(14);
					score->hitWindowHold = scoreScan.IntColumn(15);
					score->hitWindowMiss = scoreScan.IntColumn(16);
					score->hitWindowSlam = scoreScan.IntColumn(17);

					score->gaugeType = (GaugeType)scoreScan.IntColumn(18);
					score->gaugeOption = scoreScan.IntColumn(19);
					score->mirror = scoreScan.IntColumn(20) == 1;
					score->random = scoreScan.IntColumn(21) == 1;
					chart->scores.Add(score);
				}
				scoreScan.Rewind();

				m_SortScores(chart);

				m_charts.Add(chart->id, chart);
				m_chartsByHash.Add(chart->hash, chart);
				// Add diff to map and resort
				folder->charts.Add(chart);
				m_SortCharts(folder);

				// Add Chart
				addChart.BindInt(1, chart->folderId);
				addChart.BindString(2, chart->path);
				addChart.BindString(3, chart->title);
				addChart.BindString(4, chart->artist);
				addChart.BindString(5, chart->title_translit);
				addChart.BindString(6, chart->artist_translit);
				addChart.BindString(7, chart->jacket_path);
				addChart.BindString(8, chart->effector);
				addChart.BindString(9, chart->illustrator);
				addChart.BindString(10, chart->diff_name);
				addChart.BindString(11, chart->diff_shortname);
				addChart.BindString(12, chart->bpm);
				addChart.BindInt(13, chart->diff_index);
				addChart.BindInt(14, chart->level);
				addChart.BindString(15, chart->hash);
				addChart.BindString(16, chart->preview_file);
				addChart.BindInt(17, chart->preview_offset);
				addChart.BindInt(18, chart->preview_length);
				addChart.BindInt64(19, chart->lwt);

				addChart.Step();
				addChart.Rewind();

				// Send appropriate notification
				if(existingUpdated)
				{
					updatedChartEvents.Add(folder);
				}
				else
				{
					addedChartEvents.Add(folder);
				}
			}
			else if(e.type == Event::Chart && e.action == Event::Updated)
			{
				update.BindString(1, e.path);
				update.BindString(2, e.mapData->title);
				update.BindString(3, e.mapData->artist);
				update.BindString(4, "");
				update.BindString(5, "");
				update.BindString(6, e.mapData->jacketPath);
				update.BindString(7, e.mapData->effector);
				update.BindString(8, e.mapData->illustrator);
				update.BindString(9, diffNames[e.mapData->difficulty]);
				update.BindString(10, diffShortNames[e.mapData->difficulty]);
				update.BindString(11, e.mapData->bpm);
				update.BindInt(12, e.mapData->difficulty);
				update.BindInt(13, e.mapData->level);
				update.BindString(14, e.hash);
				update.BindString(15, e.mapData->audioNoFX);
				update.BindInt(16, e.mapData->previewOffset);
				update.BindInt(17, e.mapData->previewDuration);
				update.BindInt64(18, e.lwt);
				update.BindInt(19, e.id);

				update.Step();
				update.Rewind();

				auto itChart = m_charts.find(e.id);
				assert(itChart != m_charts.end());

				ChartIndex* chart = itChart->second;
				chart->lwt = e.lwt;
				chart->path = e.path;
				chart->title = e.mapData->title;
				chart->artist = e.mapData->artist;
				chart->level = e.mapData->level;
				chart->effector = e.mapData->effector;
				chart->preview_file = e.mapData->audioNoFX;
				chart->preview_offset = e.mapData->previewOffset;
				chart->preview_length = e.mapData->previewDuration;
				chart->diff_index = e.mapData->difficulty;
				chart->diff_name = diffNames[e.mapData->difficulty];
				chart->diff_shortname = diffShortNames[e.mapData->difficulty];
				chart->bpm = e.mapData->bpm;
				chart->illustrator = e.mapData->illustrator;
				chart->jacket_path = e.mapData->jacketPath;


				// Check if the hash has changed...
				if (chart->hash != e.hash && m_transferScores) {
					moveScores.BindString(1, e.hash);
					moveScores.BindString(2, chart->hash);
					moveScores.Step();
					moveScores.Rewind();
				}
				chart->hash = e.hash;


				auto itFolder = m_folders.find(chart->folderId);
				assert(itFolder != m_folders.end());

				// Send notification
				updatedChartEvents.Add(itFolder->second);
			}
			else if(e.type == Event::Chart && e.action == Event::Removed)
			{
				auto itChart = m_charts.find(e.id);
				assert(itChart != m_charts.end());

				auto itFolder = m_folders.find(itChart->second->folderId);
				assert(itFolder != m_folders.end());

				itFolder->second->charts.Remove(itChart->second);

				for (auto s : itChart->second->scores)
				{
					delete s;
				}
				itChart->second->scores.clear();
				delete itChart->second;
				m_charts.erase(e.id);

				// Remove diff in db
				removeChart.BindInt(1, e.id);
				removeChart.Step();
				removeChart.Rewind();

				if(itFolder->second->charts.empty()) // Remove map as well
				{
					removeChartEvents.Add(itFolder->second);

					removeFolder.BindInt(1, itFolder->first);
					removeFolder.Step();
					removeFolder.Rewind();

					m_foldersByPath.erase(itFolder->second->path);
					m_folders.erase(itFolder);
				}
				else
				{
					updatedChartEvents.Add(itFolder->second);
				}
			}
			if(e.mapData)
				delete e.mapData;
		}
		m_database.Exec("END");

		// Fire events
		if(!removeChartEvents.empty())
		{
			Vector<FolderIndex*> eventsArray;
			for(auto i : removeChartEvents)
			{
				// Don't send 'updated' or 'added' events for removed maps
				addedChartEvents.erase(i);
				updatedChartEvents.erase(i);
				eventsArray.Add(i);
			}

			m_outer.OnFoldersRemoved.Call(eventsArray);
			for(auto e : eventsArray)
			{
				delete e;
			}
		}
		if(!addedChartEvents.empty())
		{
			Vector<FolderIndex*> eventsArray;
			for(auto i : addedChartEvents)
			{
				// Don't send 'updated' events for added maps
				updatedChartEvents.erase(i);
				eventsArray.Add(i);
			}

			m_outer.OnFoldersAdded.Call(eventsArray);
		}
		if(!addedChalEvents.empty())
		{
			Vector<ChallengeIndex*> eventsArray;
			for(auto i : addedChalEvents)
			{
				// Don't send 'updated' events for added maps
				updatedChalEvents.erase(i);
				eventsArray.Add(i);
			}

			m_outer.OnChallengesAdded.Call(eventsArray);
		}
		if(!updatedChartEvents.empty())
		{
			Vector<FolderIndex*> eventsArray;
			for(auto i : updatedChartEvents)
			{
				eventsArray.Add(i);
			}

			m_outer.OnFoldersUpdated.Call(eventsArray);
		}
		if(!updatedChalEvents.empty())
		{
			Vector<ChallengeIndex*> eventsArray;
			for(auto i : updatedChalEvents)
			{
				eventsArray.Add(i);
			}

			m_outer.OnChallengesUpdated.Call(eventsArray);
		}
	}

	void AddScore(ScoreIndex* score)
	{
		DBStatement addScore = m_database.Query("INSERT INTO "
			"Scores(score,crit,near,miss,gauge,auto_flags,replay,timestamp,chart_hash,user_name,user_id,local_score,window_perfect,window_good,window_hold,window_miss,window_slam,gauge_type,gauge_opt,mirror,random) "
			"VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");

		m_database.Exec("BEGIN");
		addScore.BindInt(1, score->score);
		addScore.BindInt(2, score->crit);
		addScore.BindInt(3, score->almost);
		addScore.BindInt(4, score->miss);
		addScore.BindDouble(5, score->gauge);
		addScore.BindInt(6, (int32)score->autoFlags);
		addScore.BindString(7, score->replayPath);
		addScore.BindInt64(8, score->timestamp);
		addScore.BindString(9, score->chartHash);
		addScore.BindString(10, score->userName);
		addScore.BindString(11, score->userId);
		addScore.BindInt(12, score->localScore);
		addScore.BindInt(13, score->hitWindowPerfect);
		addScore.BindInt(14, score->hitWindowGood);
		addScore.BindInt(15, score->hitWindowHold);
		addScore.BindInt(16, score->hitWindowMiss);
		addScore.BindInt(17, score->hitWindowSlam);
		addScore.BindInt(18, (int32)score->gaugeType);
		addScore.BindInt(19, score->gaugeOption);
		addScore.BindInt(20, score->mirror ? 1 : 0);
		addScore.BindInt(21, score->random ? 1 : 0);

		addScore.Step();
		addScore.Rewind();

		m_database.Exec("END");
	}

	void UpdateChallengeResult(ChallengeIndex* chal, uint32 clearMark, uint32 bestScore)
	{
		assert(chal != nullptr);
		assert(m_challenges.Contains(chal->id));

		chal->clearMark = clearMark;
		chal->bestScore = bestScore;

		const constexpr char* updateQuery = "UPDATE Challenges SET "
			"clear_mark=?, best_score=?"
			" WHERE rowid=?";

		DBStatement statement = m_database.Query(updateQuery);

		m_database.Exec("BEGIN");

		statement.BindInt(1, clearMark);
		statement.BindInt(2, bestScore);
		statement.BindInt(3, chal->id);
		if (!statement.Step())
		{
			Log("Failed to execute query for UpdateChallengeResult", Logger::Severity::Warning);
		}

		statement.Rewind();

		m_database.Exec("END");
	}

	void UpdateOrAddPracticeSetup(PracticeSetupIndex* practiceSetup)
	{
		if (!m_charts.Contains(practiceSetup->chartId))
		{
			Logf("UpdateOrAddPracticeSetup called for invalid chart %d", Logger::Severity::Warning, practiceSetup->chartId);
			return;
		}

		const bool isUpdate = practiceSetup->id >= 0;
		
		if (isUpdate && !m_practiceSetups.Contains(practiceSetup->id))
		{
			Logf("UpdateOrAddPracticeSetup called for invalid index %d", Logger::Severity::Warning, practiceSetup->id);
			return;
		}

		const constexpr char* addQuery = "INSERT INTO PracticeSetups("
			"chart_id, setup_title, loop_success, loop_fail, range_begin, range_end, fail_cond_type, fail_cond_value, "
			"playback_speed, inc_speed_on_success, inc_speed, inc_streak, dec_speed_on_fail, dec_speed, min_playback_speed, max_rewind, max_rewind_measure"
			") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

		const constexpr char* updateQuery = "UPDATE PracticeSetups SET "
			"chart_id=?, setup_title=?, loop_success=?, loop_fail=?, range_begin=?, range_end=?, fail_cond_type=?, fail_cond_value=?, "
			"playback_speed=?, inc_speed_on_success=?, inc_speed=?, inc_streak=?, dec_speed_on_fail=?, dec_speed=?, min_playback_speed=?, max_rewind=?, max_rewind_measure=?"
			" WHERE rowid=?";

		DBStatement statement = m_database.Query(isUpdate ? updateQuery : addQuery);

		m_database.Exec("BEGIN");

		statement.BindInt(1, practiceSetup->chartId);
		statement.BindString(2, practiceSetup->setupTitle);
		statement.BindInt(3, practiceSetup->loopSuccess);
		statement.BindInt(4, practiceSetup->loopFail);
		statement.BindInt(5, practiceSetup->rangeBegin);
		statement.BindInt(6, practiceSetup->rangeEnd);
		statement.BindInt(7, practiceSetup->failCondType);
		statement.BindInt(8, practiceSetup->failCondValue);
		statement.BindDouble(9, practiceSetup->playbackSpeed);
		statement.BindInt(10, practiceSetup->incSpeedOnSuccess);
		statement.BindDouble(11, practiceSetup->incSpeed);
		statement.BindInt(12, practiceSetup->incStreak);
		statement.BindInt(13, practiceSetup->decSpeedOnFail);
		statement.BindDouble(14, practiceSetup->decSpeed);
		statement.BindDouble(15, practiceSetup->minPlaybackSpeed);
		statement.BindInt(16, practiceSetup->maxRewind);
		statement.BindInt(17, practiceSetup->maxRewindMeasure);

		if (isUpdate)
			statement.BindInt(18, practiceSetup->id);

		if (statement.Step())
		{
			if (!isUpdate)
			{
				DBStatement rowidStatement = m_database.Query("SELECT last_insert_rowid()");
				if (rowidStatement.StepRow())
				{
					practiceSetup->id = rowidStatement.IntColumn(0);
					assert(!m_practiceSetups.Contains(practiceSetup->id));

					m_practiceSetups.Add(practiceSetup->id, practiceSetup);
					m_practiceSetupsByChartId.Add(practiceSetup->chartId, practiceSetup);
				}
				else
				{
					Log("Failed to retrieve rowid for UpdateOrAddPracticeSetup", Logger::Severity::Warning);
				}

				rowidStatement.Rewind();
			}
		}
		else
		{
			Log("Failed to execute query for UpdateOrAddPracticeSetup", Logger::Severity::Warning);
		}

		statement.Rewind();

		m_database.Exec("END");
	}

	void UpdateChartOffset(const ChartIndex* chart)
	{
		// Safe from sqli bc hash will be alphanum
		m_database.Exec(Utility::Sprintf("UPDATE Charts SET custom_offset=%d WHERE hash LIKE '%s'", chart->custom_offset, *chart->hash));
	}

	void AddOrRemoveToCollection(const String& name, int32 mapid)
	{
		DBStatement addColl = m_database.Query("INSERT INTO Collections(folderid,collection) VALUES(?,?)");
		m_database.Exec("BEGIN");

		addColl.BindInt(1, mapid);
		addColl.BindString(2, name);

		bool result = addColl.Step();
		addColl.Rewind();

		m_database.Exec("END");

		if (!result) //Failed to add, try to remove
		{
			DBStatement remColl = m_database.Query("DELETE FROM collections WHERE folderid==? AND collection==?");
			remColl.BindInt(1, mapid);
			remColl.BindString(2, name);
			remColl.Step();
			remColl.Rewind();
		}
	}

	ChartIndex* GetRandomChart()
	{
		auto it = m_charts.begin();
		uint32 selection = Random::IntRange(0, (int32)m_charts.size() - 1);
		std::advance(it, selection);
		return it->second;
	}

	// TODO: Research thread pausing more
	// ugly but should work
	void PauseSearching() {
		if (m_paused.load())
			return;

		m_paused.store(true);
	}

	void ResumeSearching() {
		if (!m_paused.load())
			return;

		m_paused.store(false);
		m_cvPause.notify_all();
	}

	void SetChartUpdateBehavior(bool transferScores) {
		m_transferScores = transferScores;
	}

private:
	void m_CleanupMapIndex()
	{
		for(auto m : m_folders)
		{
			delete m.second;
		}
		for(auto m : m_charts)
		{
			for (auto s : m.second->scores)
			{
				delete s;
			}
			m.second->scores.clear();
			delete m.second;
		}
		for (auto m : m_practiceSetups)
		{
			delete m.second;
		}
		m_folders.clear();
		m_charts.clear();
		m_practiceSetups.clear();
		m_practiceSetupsByChartId.clear();
	}
	void m_CreateTables()
	{
		m_database.Exec("DROP TABLE IF EXISTS Folders");
		m_database.Exec("DROP TABLE IF EXISTS Charts");
		m_database.Exec("DROP TABLE IF EXISTS Scores");
		m_database.Exec("DROP TABLE IF EXISTS Collections");

		m_database.Exec("CREATE TABLE Folders"
			"(path TEXT)");

		m_database.Exec("CREATE TABLE Charts"
			"(folderid INTEGER,"
			"title TEXT,"
			"artist TEXT,"
			"title_translit TEXT,"
			"artist_translit TEXT,"
			"jacket_path TEXT,"
			"effector TEXT,"
			"illustrator TEXT,"
			"diff_name TEXT,"
			"diff_shortname TEXT,"
			"path TEXT,"
			"bpm TEXT,"
			"diff_index INTEGER,"
			"level INTEGER,"
			"preview_offset INTEGER,"
			"preview_length INTEGER,"
			"lwt INTEGER,"
			"hash TEXT,"
			"preview_file TEXT,"
			"custom_offset INTEGER, "
			"FOREIGN KEY(folderid) REFERENCES folders(rowid))");

		m_database.Exec("CREATE TABLE Scores"
			"(score INTEGER,"
			"crit INTEGER,"
			"near INTEGER,"
			"miss INTEGER,"
			"gauge REAL,"
			"gauge_type INTEGER,"
			"gauge_opt INTEGER,"
			"auto_flags INTEGER,"
			"mirror INTEGER,"
			"random INTEGER,"
			"timestamp INTEGER,"
			"replay TEXT,"
			"user_name TEXT,"
			"user_id TEXT,"
			"local_score INTEGER,"
			"window_perfect INTEGER,"
			"window_good INTEGER,"
			"window_hold INTEGER,"
			"window_miss INTEGER,"
			"window_slam INTEGER,"
			"chart_hash TEXT)");

		m_database.Exec("CREATE TABLE Collections"
			"(collection TEXT, folderid INTEGER, "
			"UNIQUE(collection,folderid), "
			"FOREIGN KEY(folderid) REFERENCES Folders(rowid))");

		m_database.Exec("CREATE TABLE PracticeSetups ("
			"chart_id INTEGER,"
			"setup_title TEXT,"
			"loop_success INTEGER,"
			"loop_fail INTEGER,"
			"range_begin INTEGER,"
			"range_end INTEGER,"
			"fail_cond_type INTEGER,"
			"fail_cond_value INTEGER,"
			"playback_speed REAL,"
			"inc_speed_on_success INTEGER,"
			"inc_speed REAL,"
			"inc_streak INTEGER,"
			"dec_speed_on_fail INTEGER,"
			"dec_speed REAL,"
			"min_playback_speed REAL,"
			"max_rewind INTEGER,"
			"max_rewind_measure INTEGER,"
			"FOREIGN KEY(chart_id) REFERENCES Charts(rowid)"
		")");

		m_database.Exec("CREATE TABLE Challenges"
			"("
			"title TEXT,"
			"charts TEXT,"
			"chart_meta TEXT," // used for search
			"clear_mark INTEGER,"
			"best_score INTEGER,"
			"req_text TEXT,"
			"path TEXT,"
			"hash TEXT,"
			"level INTEGER,"
			"lwt INTEGER"
			")");
	}
	void m_LoadInitialData()
	{
		assert(!m_searching);

		// Clear search state
		m_searchState.difficulties.clear();

		// Scan original maps
		m_CleanupMapIndex();

		// Select Maps
		DBStatement mapScan = m_database.Query("SELECT rowid, path FROM Folders");
		while(mapScan.StepRow())
		{
			FolderIndex* folder = new FolderIndex();
			folder->id = mapScan.IntColumn(0);
			folder->path = mapScan.StringColumn(1);
			folder->selectId = (int32) m_folders.size();
			m_folders.Add(folder->id, folder);
			m_foldersByPath.Add(folder->path, folder);
		}
		m_nextFolderId = m_folders.empty() ? 1 : (m_folders.rbegin()->first + 1);

		// Select Difficulties
		DBStatement chartScan = m_database.Query("SELECT rowid"
			",folderId"
			",path"
			",title"
			",artist"
			",title_translit"
			",artist_translit"
			",jacket_path"
			",effector"
			",illustrator"
			",diff_name"
			",diff_shortname"
			",bpm"
			",diff_index"
			",level"
			",hash"
			",preview_file"
			",preview_offset"
			",preview_length"
			",lwt"
			",custom_offset "
			"FROM Charts");
		while(chartScan.StepRow())
		{
			ChartIndex* chart = new ChartIndex();
			chart->id = chartScan.IntColumn(0);
			chart->folderId = chartScan.IntColumn(1);
			chart->path = chartScan.StringColumn(2);
			chart->title = chartScan.StringColumn(3);
			chart->artist = chartScan.StringColumn(4);
			chart->title_translit = chartScan.StringColumn(5);
			chart->artist_translit = chartScan.StringColumn(6);
			chart->jacket_path = chartScan.StringColumn(7);
			chart->effector = chartScan.StringColumn(8);
			chart->illustrator = chartScan.StringColumn(9);
			chart->diff_name = chartScan.StringColumn(10);
			chart->diff_shortname = chartScan.StringColumn(11);
			chart->bpm = chartScan.StringColumn(12);
			chart->diff_index = chartScan.IntColumn(13);
			chart->level = chartScan.IntColumn(14);
			chart->hash = chartScan.StringColumn(15);
			chart->preview_file = chartScan.StringColumn(16);
			chart->preview_offset = chartScan.IntColumn(17);
			chart->preview_length = chartScan.IntColumn(18);
			chart->lwt = chartScan.Int64Column(19);
			chart->custom_offset = chartScan.IntColumn(20);

			// Add existing diff
			m_charts.Add(chart->id, chart);
			m_chartsByHash.Add(chart->hash, chart);

			// Add difficulty to map and resort difficulties
			auto folderIt = m_folders.find(chart->folderId);
			assert(folderIt != m_folders.end());
			folderIt->second->charts.Add(chart);
			m_SortCharts(folderIt->second);

			// Add to search state
			SearchState::ExistingFileEntry ed;
			ed.id = chart->id;
			if (chart->hash.length() == 0)
			{
				ed.lwt = 0;
			}
			else {
				ed.lwt = chart->lwt;

			}
			m_searchState.difficulties.Add(chart->path, ed);
		}

		// Select Scores
		DBStatement scoreScan = m_database.Query("SELECT "
			"rowid,score,crit,near,miss,gauge,auto_flags,replay,timestamp,chart_hash,user_name,user_id,local_score,window_perfect,window_good,window_hold,window_miss,window_slam,gauge_type,gauge_opt,mirror,random "
			"FROM Scores");
		
		while (scoreScan.StepRow())
		{
			ScoreIndex* score = new ScoreIndex();
			score->id = scoreScan.IntColumn(0);
			score->score = scoreScan.IntColumn(1);
			score->crit = scoreScan.IntColumn(2);
			score->almost = scoreScan.IntColumn(3);
			score->miss = scoreScan.IntColumn(4);
			score->gauge = (float) scoreScan.DoubleColumn(5);
			score->autoFlags = (AutoFlags)scoreScan.IntColumn(6);
			score->replayPath = scoreScan.StringColumn(7);

			score->timestamp = scoreScan.Int64Column(8);
			score->chartHash = scoreScan.StringColumn(9);
			score->userName = scoreScan.StringColumn(10);
			score->userId = scoreScan.StringColumn(11);
			score->localScore = scoreScan.IntColumn(12);

			score->hitWindowPerfect = scoreScan.IntColumn(13);
			score->hitWindowGood = scoreScan.IntColumn(14);
			score->hitWindowHold = scoreScan.IntColumn(15);
			score->hitWindowMiss = scoreScan.IntColumn(16);
			score->hitWindowSlam = scoreScan.IntColumn(17);

			score->gaugeType = (GaugeType)scoreScan.IntColumn(18);
			score->gaugeOption = scoreScan.IntColumn(19);
			score->mirror = scoreScan.IntColumn(20) == 1;
			score->random = scoreScan.IntColumn(21) == 1;

			// Add difficulty to map and resort difficulties
			auto diffIt = m_chartsByHash.find(score->chartHash);
			if (diffIt == m_chartsByHash.end()) // If for whatever reason the diff that the score is attatched to is not in the db, ignore the score.
			{
				delete score;
				continue;
			}

			diffIt->second->scores.Add(score);
			m_SortScores(diffIt->second);
		}

		// Select Practice setups
		DBStatement practiceSetupScan = m_database.Query("SELECT rowid, chart_id, setup_title, loop_success, loop_fail, range_begin, range_end, fail_cond_type, fail_cond_value,"
			"playback_speed, inc_speed_on_success, inc_speed, inc_streak, dec_speed_on_fail, dec_speed, min_playback_speed, max_rewind, max_rewind_measure FROM PracticeSetups");

		while (practiceSetupScan.StepRow())
		{
			PracticeSetupIndex* practiceSetup = new PracticeSetupIndex();
			practiceSetup->id = practiceSetupScan.IntColumn(0);
			practiceSetup->chartId = practiceSetupScan.IntColumn(1);
			practiceSetup->setupTitle = practiceSetupScan.StringColumn(2);
			practiceSetup->loopSuccess = practiceSetupScan.IntColumn(3);
			practiceSetup->loopFail = practiceSetupScan.IntColumn(4);
			practiceSetup->rangeBegin = practiceSetupScan.IntColumn(5);
			practiceSetup->rangeEnd = practiceSetupScan.IntColumn(6);
			practiceSetup->failCondType = practiceSetupScan.IntColumn(7);
			practiceSetup->failCondValue = practiceSetupScan.IntColumn(8);

			practiceSetup->playbackSpeed = practiceSetupScan.DoubleColumn(9);
			practiceSetup->incSpeedOnSuccess = practiceSetupScan.IntColumn(10);
			practiceSetup->incSpeed = practiceSetupScan.DoubleColumn(11);
			practiceSetup->incStreak = practiceSetupScan.IntColumn(12);
			practiceSetup->decSpeedOnFail = practiceSetupScan.IntColumn(13);
			practiceSetup->decSpeed = practiceSetupScan.DoubleColumn(14);
			practiceSetup->minPlaybackSpeed = practiceSetupScan.DoubleColumn(15);
			practiceSetup->maxRewind = practiceSetupScan.IntColumn(16);
			practiceSetup->maxRewindMeasure = practiceSetupScan.IntColumn(17);

			if (!m_charts.Contains(practiceSetup->chartId))
			{
				delete practiceSetup;
				continue;
			}

			m_practiceSetups.Add(practiceSetup->id, practiceSetup);
			m_practiceSetupsByChartId.Add(practiceSetup->chartId, practiceSetup);
		}

		m_outer.OnFoldersCleared.Call(m_folders);

		DBStatement chalScan = m_database.Query("SELECT rowid"
			",title"
			",charts"
			",clear_mark"
			",best_score"
			",req_text"
			",path"
			",hash"
			",level"
			",lwt"
			" FROM Challenges");
		while (chalScan.StepRow())
		{
			ChallengeIndex* chal = new ChallengeIndex();
			chal->id = chalScan.IntColumn(0);
			chal->title = chalScan.StringColumn(1);
			String chartsString = chalScan.StringColumn(2);
			chal->clearMark = chalScan.IntColumn(3);
			chal->bestScore = chalScan.IntColumn(4);
			chal->reqText = chalScan.StringColumnEmptyOnNull(5);
			chal->path = chalScan.StringColumn(6);
			chal->hash = chalScan.StringColumn(7);
			chal->level = chalScan.IntColumn(8);
			chal->lwt = chalScan.Int64Column(9);
			chal->missingChart = false;
			chal->charts.clear();

			nlohmann::json charts = nlohmann::json::parse(chartsString, nullptr, false);
			chal->FindCharts(&m_outer, charts);

			if (chal->charts.size() == 0)
			{
				Logf("Unable to parse charts for challenge %s in database", Logger::Severity::Warning, chal->path);
				// Try to reload from the file
				chal->lwt = 0;
			}

			// If we don't have req text, update
			if (chal->reqText == "")
				chal->lwt = 0;

			m_challenges.Add(chal->id, chal);

			// Add to search state
			SearchState::ExistingFileEntry ed;
			ed.id = chal->id;
			if (chal->hash.length() == 0)
			{
				ed.lwt = 0;
			}
			else {
				ed.lwt = chal->lwt;

			}
			m_searchState.challenges.Add(chal->path, ed);
		}
		m_nextChalId = m_challenges.empty() ? 1 : (m_challenges.rbegin()->first + 1);

		m_outer.OnChallengesCleared.Call(m_challenges);
	}
	void m_SortCharts(FolderIndex* folderIndex)
	{
		folderIndex->charts.Sort([](ChartIndex* a, ChartIndex* b)
		{
			if (a->diff_index == b->diff_index)
				return a->level < b->level;
			return a->diff_index < b->diff_index;
		});
	}

	void m_SortScores(ChartIndex* diffIndex)
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
		Map<String, FileInfo> challengeFileList;
		Map<String, FileInfo> legacyChallengeFileList;
		{
			ProfilerScope $("Chart Database - Enumerate Files and Charts");
			m_outer.OnSearchStatusUpdated.Call("[START] Chart Database - Enumerate Files and Folders");
			for(String rootSearchPath : m_searchPaths)
			{
				Vector<String> exts(3);
				exts[0] = "ksh";
				exts[1] = "chal";
				exts[2] = "kco";
				Map<String, Vector<FileInfo>> files = Files::ScanFilesRecursive(rootSearchPath, exts, &m_interruptSearch);
				if(m_interruptSearch)
					return;
				for(FileInfo& fi : files["ksh"])
				{
					fileList.Add(fi.fullPath, fi);
				}
				for(FileInfo& fi : files["chal"])
				{
					challengeFileList.Add(fi.fullPath, fi);
				}
				for(FileInfo& fi : files["kco"])
				{
					legacyChallengeFileList.Add(fi.fullPath, fi);
				}
			}
			m_outer.OnSearchStatusUpdated.Call("[END] Chart Database - Enumerate Files and Folders");
		}

		{
			ProfilerScope $("Chart Database - Process Removed Charts");
			m_outer.OnSearchStatusUpdated.Call("[START] Chart Database - Process Removed Charts");
			// Process scanned files
			for(auto f : m_searchState.difficulties)
			{
				if(!fileList.Contains(f.first))
				{
					Event evt;
					evt.type = Event::Chart;
					evt.action = Event::Removed;
					evt.path = f.first;
					evt.id = f.second.id;
					AddChange(evt);
				}
			}
			m_outer.OnSearchStatusUpdated.Call("[END] Chart Database - Process Removed Charts");
		}

		{
			ProfilerScope $("Chart Database - Process New Charts");
			m_outer.OnSearchStatusUpdated.Call("[START] Chart Database - Process New Charts");
			// Process scanned files
			for(auto f : fileList)
			{
				if (m_paused.load())
				{
					unique_lock<mutex> lock(m_pauseMutex);
					m_cvPause.wait(lock);
				}

				if(!m_searching)
					break;

				uint64 mylwt = f.second.lastWriteTime;
				Event evt;
				evt.type = Event::Chart;
				evt.lwt = mylwt;

				SearchState::ExistingFileEntry* existing = m_searchState.difficulties.Find(f.first);
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

				Logf("Discovered Chart [%s]", Logger::Severity::Info, f.first);
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
					fileStream.Seek(0);
					evt.mapData = new BeatmapSettings(map.GetMapSettings());

					ProfilerScope $("Chart Database - Hash Chart");
					char data_buffer[0x80];
					uint32_t digest[5];
					sha1::SHA1 s;

					size_t amount_read = 0;
					size_t read_size;
					do
					{
						read_size = fileStream.Read(data_buffer, sizeof(data_buffer));
						amount_read += read_size;
						s.processBytes(data_buffer, read_size);
					}
					while (read_size != 0);
						
					s.getDigest(digest);

					evt.hash = Utility::Sprintf("%08x%08x%08x%08x%08x", digest[0], digest[1], digest[2], digest[3], digest[4]);
				}

				if (!mapValid)
				{
					if(!existing) // Never added
					{
						Logf("Skipping corrupted chart [%s]", Logger::Severity::Warning, f.first);
						m_outer.OnSearchStatusUpdated.Call(Utility::Sprintf("Skipping corrupted chart [%s]", f.first));
						if(evt.mapData)
							delete evt.mapData;
						evt.mapData = nullptr;
						continue;
					}
					// XXX does remove actually use / free mapData
					// Invalid maps get removed from the database
					evt.action = Event::Removed;
				}
				evt.path = f.first;
				AddChange(evt);
				continue;
			}
			m_outer.OnSearchStatusUpdated.Call("[END] Chart Database - Process New Charts");
		}
		m_outer.OnSearchStatusUpdated.Call("");
		
		{
			ProfilerScope $("Chart Database - Process Removed Challenges");
			m_outer.OnSearchStatusUpdated.Call("[START] Chart Database - Process Removed Challenges");
			// Process scanned files
			for(auto f : m_searchState.challenges)
			{
				if(!challengeFileList.Contains(f.first))
				{
					Event evt;
					evt.type = Event::Challenge;
					evt.action = Event::Removed;
					evt.path = f.first;
					evt.id = f.second.id;
					AddChange(evt);
				}
			}
			m_outer.OnSearchStatusUpdated.Call("[END] Chart Database - Process Removed Challenges");
		}

		if (legacyChallengeFileList.size() > 0)
		{
			bool addedNewJson = false;
			ProfilerScope $("Chart Database - Converting Legacy Challenges");
			for (auto f : legacyChallengeFileList)
			{
				if (m_paused.load())
				{
					unique_lock<mutex> lock(m_pauseMutex);
					m_cvPause.wait(lock);
				}

				if (!m_searching)
					break;

				String newName = f.first + ".chal";
				// XXX if the kco was modified then after converting, it will not be updated
				if (Path::FileExists(newName))
				{
					// If we already did a convert, check if the kco has been updated
					uint64 mylwt = f.second.lastWriteTime;
					FileInfo* conv = challengeFileList.Find(newName);
					if (conv != nullptr && conv->lastWriteTime >= mylwt)
					{
						// No update
						continue;
					}
				}
				Logf("Converting legacy KShoot course %s", Logger::Severity::Info, f.first);

				File legacyFile;
				if (!legacyFile.OpenRead(f.first))
				{
					m_outer.OnSearchStatusUpdated.Call(Utility::Sprintf("Unable to open KShoot course [%s]", f.first));
					continue;
				}

				FileReader legacyReader(legacyFile);
				Map<String, String> courseSettings;
				Vector<String> courseCharts;
				if (!ParseKShootCourse(legacyReader, courseSettings, courseCharts)
					|| !courseSettings.Contains("title")
					|| courseCharts.size() == 0)
				{
					m_outer.OnSearchStatusUpdated.Call(Utility::Sprintf("Skipping corrupted KShoot course [%s]", f.first));
					continue;
				}

				nlohmann::json newJson = "{\"charts\":[], \"level\":0, \"global\":{\"clear\":true}}"_json;
				newJson["title"] = courseSettings["title"];
				for (const String& chart : courseCharts)
					newJson["charts"].push_back(chart);

				File newJsonFile;
				if (!newJsonFile.OpenWrite(newName))
				{
					m_outer.OnSearchStatusUpdated.Call(Utility::Sprintf("Unable to open KShoot course json file [%s]", newName));
					continue;
				}
				String jsonData = newJson.dump(4);
				newJsonFile.Write(*jsonData, jsonData.length());
				addedNewJson = true;
			}

			// If we added a json file we have to rescan for chals, this only happens when converting legacy courses
			if (addedNewJson)
			{
				for (String rootSearchPath : m_searchPaths)
				{
					challengeFileList.clear();
					Vector<FileInfo> files = Files::ScanFilesRecursive(rootSearchPath, "chal", &m_interruptSearch);
					if (m_interruptSearch)
						return;
					for (FileInfo& fi : files)
					{
						challengeFileList.Add(fi.fullPath, fi);
					}
				}
			}
			m_outer.OnSearchStatusUpdated.Call("[END] Chart Database - Converting Legacy Challenges");
		}

		if (challengeFileList.size() > 0)
		{
			ProfilerScope $("Chart Database - Process New Challenges");
			m_outer.OnSearchStatusUpdated.Call("[START] Chart Database - Process New Challenges");
			// Process scanned files
			for (auto f : challengeFileList)
			{
				if (m_paused.load())
				{
					unique_lock<mutex> lock(m_pauseMutex);
					m_cvPause.wait(lock);
				}

				if (!m_searching)
					break;

				uint64 mylwt = f.second.lastWriteTime;
				Event evt;
				evt.type = Event::Challenge;
				evt.lwt = mylwt;

				SearchState::ExistingFileEntry* existing = m_searchState.challenges.Find(f.first);
				if (existing)
				{
					evt.id = existing->id;
					if (existing->lwt != mylwt)
					{
						// Challenge Updated
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
					// Challenge added
					evt.action = Event::Added;
				}

				if (existing)
					Logf("Discovered Updated Challenge [%s]", Logger::Severity::Info, f.first);
				else
					Logf("Discovered Challenge [%s]", Logger::Severity::Info, f.first);
				m_outer.OnSearchStatusUpdated.Call(Utility::Sprintf("Discovered Challenge [%s]", f.first));

				bool chalValid = true;
				// TODO support old style courses
				nlohmann::json settings;

				File chalFile;
				if (!chalFile.OpenRead(f.first))
					chalValid = false;

				if (chalValid)
				{
					// TODO support old style courses
					Buffer jsonBuf;
					jsonBuf.resize(chalFile.GetSize());
					chalFile.Read(jsonBuf.data(), jsonBuf.size());
					settings = ChallengeIndex::LoadJson(jsonBuf, f.first);
					chalValid = ChallengeIndex::BasicValidate(settings, f.first);

					if (chalValid)
					{
						sha1::SHA1 s;

						s.processBytes(jsonBuf.data(), jsonBuf.size());

						uint32_t digest[5];
						s.getDigest(digest);

						evt.hash = Utility::Sprintf("%08x%08x%08x%08x%08x", digest[0], digest[1], digest[2], digest[3], digest[4]);

					}
				}

				if (!chalValid)
				{
					// Reset json entry
					evt.json = nlohmann::json();

					if(!existing) // Never added
					{
						Logf("Skipping corrupted challenge [%s]", Logger::Severity::Warning, f.first);
						m_outer.OnSearchStatusUpdated.Call(Utility::Sprintf("Skipping corrupted challenge [%s]", f.first));
						continue;
					}
					// Invalid chals get removed from the database
					evt.action = Event::Removed;
				}
				else
				{
					evt.json = settings;
				}
				evt.path = f.first;
				AddChange(evt);
				continue;
			}
			m_outer.OnSearchStatusUpdated.Call("[END] Chart Database - Process New Challenges");
		}
		m_outer.OnSearchStatusUpdated.Call("");

		m_searching = false;
	}

};

void MapDatabase::FinishInit()
{
	assert(!m_impl);
	m_impl = new MapDatabase_Impl(*this, m_transferScores);
}
MapDatabase::MapDatabase(bool postponeInit)
{
	if (!postponeInit)
		FinishInit();
	else
		m_impl = NULL;
}
MapDatabase::MapDatabase()
{
	m_impl = new MapDatabase_Impl(*this, true);
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
	assert(m_impl);
	m_impl->StartSearching();
}
void MapDatabase::PauseSearching()
{
	m_impl->PauseSearching();
}
void MapDatabase::ResumeSearching()
{
	m_impl->ResumeSearching();
}
void MapDatabase::StopSearching()
{
	m_impl->StopSearching();
}
Map<int32, FolderIndex*> MapDatabase::FindFoldersByPath(const String& search)
{
	return m_impl->FindFoldersByPath(search);
}
Map<int32, ChallengeIndex*> MapDatabase::FindChallenges(const String& search)
{
	return m_impl->FindChallenges(search);
}
Map<int32, FolderIndex*> MapDatabase::FindFolders(const String& search)
{
	return m_impl->FindFolders(search);
}
Map<int32, FolderIndex*> MapDatabase::FindFoldersByHash(const String& hash)
{
	return m_impl->FindFoldersByHash(hash);
}
Map<int32, FolderIndex*> MapDatabase::FindFoldersByFolder(const String & folder)
{
	return m_impl->FindFoldersByFolder(folder);
}
Map<int32, FolderIndex*> MapDatabase::FindFoldersByCollection(const String& category)
{
	return m_impl->FindFoldersByCollection(category);
}
FolderIndex* MapDatabase::GetFolder(int32 idx)
{
	FolderIndex** folderIdx = m_impl->m_folders.Find(idx);
	return folderIdx ? *folderIdx : nullptr;
}
Vector<String> MapDatabase::GetCollections()
{
	return m_impl->GetCollections();
}
Vector<String> MapDatabase::GetCollectionsForMap(int32 mapid)
{
	return m_impl->GetCollectionsForMap(mapid);
}
Vector<PracticeSetupIndex*> MapDatabase::GetOrAddPracticeSetups(int32 chartId)
{
	return m_impl->GetOrAddPracticeSetups(chartId);
}
void MapDatabase::AddOrRemoveToCollection(const String& name, int32 mapid)
{
	m_impl->AddOrRemoveToCollection(name, mapid);
}
void MapDatabase::AddSearchPath(const String& path)
{
	m_impl->AddSearchPath(path);
}
void MapDatabase::RemoveSearchPath(const String& path)
{
	m_impl->RemoveSearchPath(path);
}
void MapDatabase::UpdateChartOffset(const ChartIndex* chart)
{
	m_impl->UpdateChartOffset(chart);
}
void MapDatabase::AddScore(ScoreIndex* score)
{
	m_impl->AddScore(score);
}
void MapDatabase::UpdatePracticeSetup(PracticeSetupIndex* practiceSetup)
{
	m_impl->UpdateOrAddPracticeSetup(practiceSetup);
}
void MapDatabase::UpdateChallengeResult(ChallengeIndex* chal, uint32 clearMark, uint32 bestScore)
{
	m_impl->UpdateChallengeResult(chal, clearMark, bestScore);
}
ChartIndex* MapDatabase::GetRandomChart()
{
	return m_impl->GetRandomChart();
}
void MapDatabase::SetChartUpdateBehavior(bool transferScores) {
	m_transferScores = transferScores;
	if (m_impl != NULL)
		m_impl->SetChartUpdateBehavior(transferScores);
}
ChartIndex* MapDatabase::FindFirstChartByPath(const String& s)
{
	return m_impl->FindFirstChartByPath(s);
}
ChartIndex* MapDatabase::FindFirstChartByHash(const String& s)
{
	return m_impl->FindFirstChartByHash(s);
}
ChartIndex* MapDatabase::FindFirstChartByNameAndLevel(const String& s, int32 level)
{
	return m_impl->FindFirstChartByNameAndLevel(s, level);
}
