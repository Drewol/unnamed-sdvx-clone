#pragma once

#include <Beatmap/MapDatabase.hpp>
#include "ApplicationTickable.hpp"

// we create the id for a song select index as (10 * mapId + diffId) where
//  diffId is 0 for multiple diffs.
// This keeps mapIds sorted, and the minor change of difficulty index (not id)
//  should order that way as well (I hope, haven't checked if they must be ordered or not)

struct SongSelectIndex
{
public:
	SongSelectIndex(MapIndex* map)
		: m_map(map), m_diffs(map->difficulties),
		id(map->id * 10)
	{
	}

	SongSelectIndex(MapIndex* map, Vector<DifficultyIndex*> diffs)
		: m_map(map), m_diffs(diffs),
		id(map->id * 10)
	{
	}

	SongSelectIndex(MapIndex* map, DifficultyIndex* diff)
		: m_map(map)
	{
		m_diffs.Add(diff);

		int32 i = 0;
		for (auto mapDiff : map->difficulties)
		{
			if (mapDiff == diff)
				break;
			i++;
		}

		id = map->id * 10 + i;
	}

	int32 id;

	// use accessor functions just in case these need to be virtual for some reason later
	// keep the api easy to play with
	MapIndex* GetMap() { return m_map; }
	Vector<DifficultyIndex*> GetDifficulties() { return m_diffs; }

private:
	MapIndex * m_map;
	Vector<DifficultyIndex*> m_diffs;
};

/*
	Song select screen
*/
class SongSelect : public IApplicationTickable
{
protected:
	SongSelect() = default;
public:
	virtual ~SongSelect() = default;
	static SongSelect* Create();
};