#include "stdafx.h"
#include "SongFilter.hpp"

Map<int32, SongSelectIndex> LevelFilter::GetFiltered(const Map<int32, SongSelectIndex>& source)
{
	Map<int32, SongSelectIndex> filtered;
	for (auto kvp : source)
	{
		for (auto chart: kvp.second.GetCharts())
		{
			if (chart->level == m_level)
			{
				SongSelectIndex index(kvp.second.GetFolder(), chart);
				filtered.Add(index.id, index);
			}
		}
	}
	return filtered;
}

String LevelFilter::GetName() const
{
	return Utility::Sprintf("Level: %d", m_level);
}

bool LevelFilter::IsAll() const
{
	return false;
}

Map<int32, SongSelectIndex> FolderFilter::GetFiltered(const Map<int32, SongSelectIndex>& source)
{
	Map<int32, FolderIndex*> folders = m_mapDatabase->FindFoldersByFolder(m_folder);

	Map<int32, SongSelectIndex> filtered;
	for (auto m : folders)
	{
		SongSelectIndex index(m.second);
		filtered.Add(index.id, index);
	}
	return filtered;
}

String FolderFilter::GetName() const
{
	return "Folder: " + m_folder;
}

bool FolderFilter::IsAll() const
{
	return false;
}

Map<int32, SongSelectIndex> CollectionFilter::GetFiltered(const Map<int32, SongSelectIndex>& source)
{
	Map<int32, FolderIndex*> folders = m_mapDatabase->FindFoldersByCollection(m_collection);

	Map<int32, SongSelectIndex> filtered;
	for (auto m : folders)
	{
		SongSelectIndex index(m.second);
		filtered.Add(index.id, index);
	}
	return filtered;
}

String CollectionFilter::GetName() const
{
	return "Collection: " + m_collection;
}

bool CollectionFilter::IsAll() const
{
	return false;
}

ChartSetFilter::ChartSetFilter(String name, String sortKey, FilterType type, const Vector<int32>& chartIds)
	: m_type(type)
{
	Update(std::move(name), std::move(sortKey), chartIds);
}

void ChartSetFilter::Update(String name, String sortKey, const Vector<int32>& chartIds)
{
	m_name = std::move(name);
	m_sortKey = std::move(sortKey);
	m_chartIds.clear();
	for (int32 chartId : chartIds)
		m_chartIds.insert(chartId);
}

Map<int32, SongSelectIndex> ChartSetFilter::GetFiltered(const Map<int32, SongSelectIndex>& source)
{
	Map<int32, SongSelectIndex> filtered;
	for (const auto& entry : source)
	{
		Vector<ChartIndex*> charts;
		for (ChartIndex* chart : entry.second.GetCharts())
		{
			if (m_chartIds.count(chart->id))
				charts.Add(chart);
		}
		if (!charts.empty())
		{
			SongSelectIndex index(entry.second.GetFolder(), charts);
			filtered.Add(index.id, index);
		}
	}
	return filtered;
}

Map<int32, ChallengeSelectIndex> ChallengeLevelFilter::GetFiltered(const Map<int32, ChallengeSelectIndex>& source)
{
	Map<int32, ChallengeSelectIndex> filtered;
	for (auto kvp : source)
	{
		const auto& chal = kvp.second.GetChallenge();
		if (chal->level == m_level)
		{
			ChallengeSelectIndex index(chal);
			filtered.Add(index.id, index);
		}
	}
	return filtered;
}

String ChallengeLevelFilter::GetName() const
{
	//return "Level: " + "\u221E";
	if (m_level == 12)
		return "Level: \xe2\x88\x9e";
	return Utility::Sprintf("Level: %d", m_level);
}

bool ChallengeLevelFilter::IsAll() const
{
	return false;
}
