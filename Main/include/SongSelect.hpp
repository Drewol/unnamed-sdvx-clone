#pragma once

#include "ApplicationTickable.hpp"
#include "MultiplayerScreen.hpp"
#include <Beatmap/MapDatabase.hpp>

struct SongSelectIndex
{
private:
	FolderIndex* m_folder;
	Vector<ChartIndex*> m_charts;
public:
	SongSelectIndex() = default;
	SongSelectIndex(FolderIndex* folder)
		: m_folder(folder), m_charts(folder->charts),
		id(folder->selectId * 10)
	{
	}

	SongSelectIndex(FolderIndex* map, Vector<ChartIndex*> charts)
		: m_folder(map), m_charts(charts),
		id(map->selectId * 10)
	{
	}

	SongSelectIndex(FolderIndex* map, ChartIndex* chart)
		: m_folder(map)
	{
		m_charts.Add(chart);

		int32 i = 0;
		for (auto mapDiff : map->charts)
		{
			if (mapDiff == chart)
				break;
			i++;
		}

		id = map->selectId * 10 + i + 1;
	}

	// TODO(local): likely make this a function as well
	int32 id;

	// use accessor functions just in case these need to be virtual for some reason later
	// keep the api easy to play with
	FolderIndex* GetFolder() const { return m_folder; }
	Vector<ChartIndex*> GetCharts() const { return m_charts; }

};


/*
	Song select screen
*/
class SongSelect : public IAsyncLoadableApplicationTickable
{
protected:
	SongSelect() = default;
public:
	virtual ~SongSelect() = default;
	static SongSelect* Create();
	static SongSelect* Create(MultiplayerScreen*);

	virtual ChartIndex* GetCurrentSelectedChart() { return 0; }
};
