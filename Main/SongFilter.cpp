#include "stdafx.h"
#include "SongFilter.hpp"

Map<int32, SongSelectIndex> LevelFilter::GetFiltered(const Map<int32, SongSelectIndex>& source)
{
	Map<int32, SongSelectIndex> filtered;
	for (auto kvp : source)
	{
		for (auto diff : kvp.second.GetDifficulties())
		{
			if (diff->settings.level == m_level)
			{
				SongSelectIndex index(kvp.second.GetMap(), diff);
				filtered.Add(index.id, index);
			}
		}
	}
	return filtered;
}

String LevelFilter::GetName()
{
	return Utility::Sprintf("Level: %d", m_level);
}

bool LevelFilter::IsAll()
{
	return false;
}

Map<int32, SongSelectIndex> FolderFilter::GetFiltered(const Map<int32, SongSelectIndex>& source)
{
	Map<int32, SongSelectIndex> filtered;
	String folder = "\\" + m_folder + "\\";
	for (auto kvp : source)
	{
		MapIndex* map = kvp.second.GetMap();
		if (map->path.find(folder) != -1)
		{
			SongSelectIndex index(map);
			filtered.Add(index.id, index);
		}
	}
	Logf("%d items in filter for %s", Logger::Info, filtered.size(), m_folder);
	return filtered;
//	Map<int32, MapIndex*> maps = m_mapDatabase->FindMapsByFolder(m_folder);
}

String FolderFilter::GetName()
{
	return "Folder: " + m_folder;
}

bool FolderFilter::IsAll()
{
	return false;
}
