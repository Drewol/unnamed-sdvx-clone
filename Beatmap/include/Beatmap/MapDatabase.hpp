#pragma once
#include "Beatmap.hpp"

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
	int32 diffid;
	int32 score;
	int32 crit;
	int32 almost;
	int32 miss;
	float gauge;
	uint32 gameflags;
	Vector<SimpleHitStat> hitStats;
	uint64 timestamp;
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

// Map located in database
//	a map is represented by a single subfolder that contains map file
struct MapIndex
{
	// Id of this map
	int32 id;
	// Id of this map
	int32 selectId;
	// Full path to the map root folder
	String path;
	// List of difficulties contained within the map
	Vector<DifficultyIndex*> difficulties;
};

class MapDatabase : public NonCopyable
{
public:
	MapDatabase();
	~MapDatabase();

	// Checks the background scanning and actualized the current map database
	void Update();

	bool IsSearching() const;
	void StartSearching();
	void StopSearching();

	// Grab all the maps, with their id's
	Map<int32, MapIndex*> GetMaps();
	// Finds maps using the search query provided
	// search artist/title/tags for maps for any space separated terms
	Map<int32, MapIndex*> FindMaps(const String& search);
	Map<int32, MapIndex*> FindMapsByHash(const String& hash);
	Map<int32, MapIndex*> FindMapsByFolder(const String& folder);
	Map<int32, MapIndex*> FindMapsByCollection(const String& collection);
	MapIndex* GetMap(int32 idx);
	Vector<String> GetCollections();
	Vector<String> GetCollectionsForMap(int32 mapid);

	// Get a random chart
	DifficultyIndex* GetRandomDiff();

	//Attempts to add to collection, if that fails attempt to remove from collection
	void AddOrRemoveToCollection(const String& name, int32 mapid);
	void AddSearchPath(const String& path);
	void AddScore(const DifficultyIndex& diff, int score, int crit, int almost, int miss, float gauge, uint32 gameflags, Vector<SimpleHitStat> simpleHitStats, uint64 timestamp);
	void RemoveSearchPath(const String& path);


	Delegate<String> OnSearchStatusUpdated;
	// (mapId, mapIndex)
	Delegate<Vector<MapIndex*>> OnMapsRemoved;
	// (mapId, mapIndex)
	Delegate<Vector<MapIndex*>> OnMapsAdded;
	// (mapId, mapIndex)
	Delegate<Vector<MapIndex*>> OnMapsUpdated;
	// Called when all maps are cleared
	// (newMapList)
	Delegate<Map<int32, MapIndex*>> OnMapsCleared;

private:
	class MapDatabase_Impl* m_impl;
};