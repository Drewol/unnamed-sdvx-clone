#pragma once
#include "Beatmap.hpp"
#include "json.hpp"
#include "PlaybackOptions.hpp"

struct SimpleHitStat
{
	// 0 = miss, 1 = near, 2 = crit, 3 = idle
	int8 rating;
	int8 lane;
	int32 time;
	int32 delta;
	// Hold state
	// This is the amount of gotten ticks in a hold sequence
	uint32 hold = 0;
	// This is the amount of total ticks in this hold sequence
	uint32 holdMax = 0;
};

struct ScoreIndex
{
	int32 id;
	int32 score;
	int32 crit;
	int32 almost;
	int32 miss;
	float gauge;
	GaugeType gaugeType;
	uint32 gaugeOption;
	AutoFlags autoFlags;
	bool random;
	bool mirror;
	String replayPath;
	String chartHash;
	uint64 timestamp;
	String userName;
	String userId;
	bool localScore;

	int32 hitWindowPerfect;
	int32 hitWindowGood;
	int32 hitWindowHold;
	int32 hitWindowMiss;
	int32 hitWindowSlam;
};


// Single difficulty of a map
// a single map may contain multiple difficulties
struct DifficultyIndex
{
	// Id of this difficulty
	int32 id;
	// Id of the map that contains this difficulty
	int32 mapId;
	// Full path to the difficulty
	String path;
	// Last time the difficulty changed
	uint64 lwt;
	// Map metadata
	BeatmapSettings settings;
	// Map scores
	Vector<ScoreIndex*> scores;
	// Hash of the song file
	String hash;
};

struct ChartIndex
{
	int32 id;
	int32 folderId;
	String path;
	String title;
	String artist;
	String title_translit;
	String artist_translit;
	String jacket_path;
	String effector;
	String illustrator;
	String diff_name;
	String diff_shortname;
	String bpm;
	int32 diff_index;
	int32 level;
	String hash;
	String preview_file;
	int32 preview_offset;
	int32 preview_length;
	uint64 lwt;
	int32 custom_offset = 0;
	Vector<ScoreIndex*> scores;
};

// Map located in database
//	a map is represented by a single subfolder that contains map file
struct FolderIndex
{
	// Id of this map
	int32 id;
	// Id of this map
	int32 selectId;
	// Full path to the map root folder
	String path;
	// List of charts contained within the folder
	Vector<ChartIndex*> charts;
};


struct ChallengeIndex 
{
	int32 id;
	Vector<ChartIndex*> charts;
	int32 totalNumCharts; // Note: This is not the number found
	nlohmann::json settings;
	String title;
	int32 clearMark;
	int32 bestScore;
	String reqText;
	String path;
	String hash;
	int32 level;
	uint64 lwt;
	bool missingChart;

	// Access settings and check if they are actually loaded
	const nlohmann::json& GetSettings()
	{
		if (settings.is_null() || settings.is_discarded())
			ReloadSettings();
		return settings;
	}
	void ReloadSettings()
	{
		settings = LoadJson(path);
		if (!BasicValidate()) // Only keep if valid
			settings = nlohmann::json();
	}

	// This will validate the overall objects/arrays but not option types
	bool BasicValidate() const { return BasicValidate(settings, path); };
	void FindCharts(class MapDatabase*, const nlohmann::json& v);
	static nlohmann::json LoadJson(const String& path);
	static nlohmann::json LoadJson(const Buffer& buffer, const String& path);
	static bool BasicValidate(const nlohmann::json& v, const String& path);
	void GenerateDescription();
private:
	static String ChallengeDescriptionVal(const nlohmann::json&, const String&, const String&, bool, int, int);
	static String ChallengeDescriptionVal(const nlohmann::json&, const String&, bool, int mult=1, int add=0);
	static const Map<String, std::pair<String, String>> ChallengeDescriptionStrings;
};

struct PracticeSetupIndex
{
	int32 id = -1;
	// ID of the chart
	int32 chartId;
	// Name of this setup (currently not used)
	String setupTitle;

	// Setup options
	int32 loopSuccess;
	int32 loopFail;
	int32 rangeBegin;
	int32 rangeEnd;
	int32 failCondType;
	int32 failCondValue;
	double playbackSpeed;

	int32 incSpeedOnSuccess;
	double incSpeed;
	int32 incStreak;

	int32 decSpeedOnFail;
	double decSpeed;
	double minPlaybackSpeed;

	int32 maxRewind;
	int32 maxRewindMeasure;
};

class MapDatabase : public Unique
{
public:
	MapDatabase();
	// Postpone initialization to allow for hooks
	MapDatabase(bool postponeInit /* = false */);
	~MapDatabase();

	// Finish initialization if postponed
	void FinishInit();

	// Checks the background scanning and actualized the current map database
	void Update();

	bool IsSearching() const;
	void StartSearching();
	void PauseSearching();
	void ResumeSearching();
	void StopSearching();

	// Finds maps using the search query provided
	// search artist/title/tags for maps for any space separated terms
	Map<int32, FolderIndex*> FindFolders(const String& search);
	Map<int32, FolderIndex*> FindFoldersByPath(const String& search);
	Map<int32, FolderIndex*> FindFoldersByHash(const String& hash);
	Map<int32, FolderIndex*> FindFoldersByFolder(const String& folder);
	Map<int32, FolderIndex*> FindFoldersByCollection(const String& collection);
	Map<int32, ChallengeIndex*> FindChallenges(const String& search);
	ChartIndex* FindFirstChartByPath(const String&);
	ChartIndex* FindFirstChartByHash(const String&);
	ChartIndex* FindFirstChartByNameAndLevel(const String&, int32 level);
	FolderIndex* GetFolder(int32 idx);
	Vector<String> GetCollections();
	Vector<String> GetCollectionsForMap(int32 mapid);
	Vector<PracticeSetupIndex*> GetOrAddPracticeSetups(int32 chartId);

	// Get a random chart
	ChartIndex* GetRandomChart();

	//Attempts to add to collection, if that fails attempt to remove from collection
	void AddOrRemoveToCollection(const String& name, int32 mapid);
	void AddSearchPath(const String& path);
	void AddScore(ScoreIndex* score);

	void UpdatePracticeSetup(PracticeSetupIndex* practiceSetup);
	void UpdateChallengeResult(ChallengeIndex*, uint32 clearMark, uint32 bestScore);
	
	void RemoveSearchPath(const String& path);
	void UpdateChartOffset(const ChartIndex* chart);

	void SetChartUpdateBehavior(bool transferScores);

	Delegate<String> OnSearchStatusUpdated;
	// (mapId, mapIndex)
	Delegate<Vector<FolderIndex*>> OnFoldersRemoved;
	// (mapId, mapIndex)
	Delegate<Vector<FolderIndex*>> OnFoldersAdded;
	// (mapId, mapIndex)
	Delegate<Vector<FolderIndex*>> OnFoldersUpdated;
	// Called when all maps are cleared
	// (newMapList)
	Delegate<Map<int32, FolderIndex*>> OnFoldersCleared;

	Delegate<Vector<ChallengeIndex*>> OnChallengesRemoved;
	Delegate<Vector<ChallengeIndex*>> OnChallengesAdded;
	Delegate<Vector<ChallengeIndex*>> OnChallengesUpdated;
	Delegate<Map<int32, ChallengeIndex*>> OnChallengesCleared;

	Delegate<int> OnDatabaseUpdateStarted;
	Delegate<int, int> OnDatabaseUpdateProgress;
	Delegate<> OnDatabaseUpdateDone;

private:
	class MapDatabase_Impl* m_impl;
	bool m_transferScores = false;
};
