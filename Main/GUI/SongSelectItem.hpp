#pragma once
#include <GUI/GUIElement.hpp>
#include <GUI/Canvas.hpp>
#include "SongSelectStyle.hpp"
#include <Beatmap/MapDatabase.hpp>

// we create the id for a song select index as (10 * mapId + diffId) where
//  diffId is 0 for multiple diffs.
// This keeps mapIds sorted, and the minor change of difficulty index (not id)
//  should order that way as well (I hope, haven't checked if they must be ordered or not)

struct SongSelectIndex
{
public:
	SongSelectIndex() = default;
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

		id = map->id * 10 + i + 1;
	}

	// TODO(local): likely make this a function as well
	int32 id;

	// use accessor functions just in case these need to be virtual for some reason later
	// keep the api easy to play with
	MapIndex* GetMap() const { return m_map; }
	Vector<DifficultyIndex*> GetDifficulties() const { return m_diffs; }

private:
	MapIndex * m_map;
	Vector<DifficultyIndex*> m_diffs;
};

// Song select item
//	either shows only artist and title in compact mode
//	or shows all the difficulties
class SongSelectItem : public Canvas
{
public:
	SongSelectItem(Ref<SongSelectStyle> style);
	virtual void PreRender(GUIRenderData rd, GUIElementBase*& inputElement) override;
	virtual void Render(GUIRenderData rd) override;
	virtual Vector2 GetDesiredSize(GUIRenderData rd) override;

	// Assigns/Updates the map to be displayed on this item
	// TODO(local): Change this to SetEntry or something
	void SetIndex(struct SongSelectIndex map);

	// Select compact of full display
	void SwitchCompact(bool compact);

	// Set selected difficulty
	void SetSelectedDifficulty(int32 selectedIndex);

	// Fade of text color
	float fade = 1.0f;

	// Offset of inner text/difficulty selector
	float innerOffset = 0.0f;

private:
	Ref<SongSelectStyle> m_style;
	Vector<class SongDifficultyFrame*> m_diffSelectors;
	int32 m_selectedDifficulty = 0;
	class Panel* m_bg = nullptr;
	class LayoutBox* m_mainVert = nullptr;
	class LayoutBox* m_diffSelect = nullptr;
	class Label* m_title = nullptr;
	class Label* m_artist = nullptr;
	class Label* m_score = nullptr;
};

// Song statistics window on the left
class SongStatistics : public Canvas
{
public:
	SongStatistics(Ref<SongSelectStyle> style);

private:
	Ref<SongSelectStyle> m_style;
	class Panel* m_bg = nullptr;
};