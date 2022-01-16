#include "stdafx.h"
#include "SongSelect.hpp"
#include "TitleScreen.hpp"
#include "Application.hpp"
#include <Shared/Profiling.hpp>
#include "Scoring.hpp"
#include "Input.hpp"
#include "Game.hpp"
#include "TransitionScreen.hpp"
#include "GameConfig.hpp"
#include "SongFilter.hpp"
#include "ChatOverlay.hpp"
#include "CollectionDialog.hpp"
#include "GameplaySettingsDialog.hpp"
#include <Audio/Audio.hpp>
#include "lua.hpp"
#include <iterator>
#include <mutex>
#include <MultiplayerScreen.hpp>
#include <unordered_set>
#include "SongSort.hpp"
#include "DBUpdateScreen.hpp"
#include "PreviewPlayer.hpp"
#include "ItemSelectionWheel.hpp"
#include "Audio/OffsetComputer.hpp"
#include "Search.hpp"

/*
	Song preview player with fade-in/out
*/
void PreviewPlayer::FadeTo(Ref<AudioStream> stream, int32 restartPos /* = -1 */)
{
	// Has the preview not begun fading out yet?
	if (m_fadeOutTimer >= m_fadeDuration)
		m_fadeOutTimer = 0.0f;

	m_fadeDelayTimer = 0.0f;
	m_nextStream = stream;
	m_nextRestartPos = restartPos;
	stream.reset();
}
void PreviewPlayer::Update(float deltaTime)
{
	if (m_fadeDelayTimer < m_fadeDelayDuration)
	{
		m_fadeDelayTimer += deltaTime;

		// Is the delay time over?
		if (m_fadeDelayTimer >= m_fadeDelayDuration)
		{
			// Start playing the next stream.
			m_fadeInTimer = 0.0f;
			if (m_nextStream)
			{
				m_nextStream->SetVolume(0.0f);
				m_nextStream->Play();
			}
		}
	}

	if (m_fadeOutTimer < m_fadeDuration)
	{
		m_fadeOutTimer += deltaTime;
		float fade = m_fadeOutTimer / m_fadeDuration;
		if (m_currentStream)
			m_currentStream->SetVolume(1.0f - fade);

		if (m_fadeOutTimer >= m_fadeDuration)
			if (m_currentStream)
				m_currentStream.reset();
	}

	if (m_fadeDelayTimer >= m_fadeDelayDuration && m_fadeInTimer < m_fadeDuration)
	{
		m_fadeInTimer += deltaTime;
		if (m_fadeInTimer >= m_fadeDuration)
		{
			m_currentStream = m_nextStream;
			if (m_currentStream)
				m_currentStream->SetVolume(1.0f);
			m_nextStream.reset();
			m_currentRestartPos = m_nextRestartPos;
		}
		else
		{
			float fade = m_fadeInTimer / m_fadeDuration;

			if (m_nextStream)
				m_nextStream->SetVolume(fade);
		}
	}

	if (m_currentStream)
	{
		if (m_currentRestartPos != -1 && m_currentStream->HasEnded())
		{
			m_currentStream->SetPosition(m_currentRestartPos);
			m_currentStream->Play();
		}
	}
}
void PreviewPlayer::Pause()
{
	if (m_nextStream)
		m_nextStream->Pause();
	if (m_currentStream)
		m_currentStream->Pause();
}
void PreviewPlayer::Restore()
{
	if (m_nextStream)
		m_nextStream->Play();
	if (m_currentStream)
		m_currentStream->Play();
}

void PreviewPlayer::StopCurrent()
{
	if (m_nextStream)
		m_nextStream.reset();
	if (m_currentStream)
		m_currentStream.reset();
}


const float PreviewPlayer::m_fadeDuration = 0.5f;
const float PreviewPlayer::m_fadeDelayDuration = 0.5f;

//TODO: Abstract away some selection-wheel stuff(?)

/*
	Song selection wheel
*/
using SongItemSelectionWheel = ItemSelectionWheel<SongSelectIndex, FolderIndex>;
class SelectionWheel : public SongItemSelectionWheel
{
	// Current difficulty index
	int32 m_currentlySelectedDiff = 0;

public:
	SelectionWheel(IApplicationTickable* owner) : SongItemSelectionWheel(owner)
	{
	}

	bool Init() override
	{
		SongItemSelectionWheel::Init();
		CheckedLoad(m_lua = g_application->LoadScript("songselect/songwheel"));
		lua_newtable(m_lua);
		{
			//text
			lua_pushstring(m_lua, "searchText");
			lua_pushstring(m_lua, "");
			lua_settable(m_lua, -3);
			//enabled
			lua_pushstring(m_lua, "searchInputActive");
			lua_pushboolean(m_lua, false);
			lua_settable(m_lua, -3);
		}
		lua_setglobal(m_lua, "songwheel");
		return true;
	}
	void ReloadScript() override
	{
		g_application->ReloadScript("songselect/songwheel", m_lua);

		m_SetLuaItemIndex();
		m_SetLuaDiffIndex();
	}
	void Render(float deltaTime) override
	{
		m_lock.lock();
		lua_getglobal(m_lua, "songwheel");
		lua_pushstring(m_lua, "searchStatus");
		lua_pushstring(m_lua, *m_lastStatus);
		lua_settable(m_lua, -3);
		lua_setglobal(m_lua, "songwheel");
		m_lock.unlock();

		lua_getglobal(m_lua, "render");
		lua_pushnumber(m_lua, deltaTime);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			g_application->ScriptError("songselect/songwheel", m_lua);
		}
	}
	virtual ~SelectionWheel()
	{
		g_gameConfig.Set(GameConfigKeys::LastSelected, m_currentlySelectedItemId);
		if (m_lua)
			g_application->DisposeLua(m_lua);
	}

	void ResetLuaTables()
	{
		const SortType sort = GetSortType();
		if (sort == SortType::SCORE_ASC || sort == SortType::SCORE_DESC)
			m_doSort();

		m_SetAllItems(); //for force calculation
		m_SetCurrentItems(); //for displaying the correct songs
	}

	// Override normal item behavior to be able to reselect specific difficulties
	int32 SelectLastItemIndex(bool mapsFirst) override
	{
		if (m_lastItemIndex == -1)
			return -1;

		// Get mapid from diffid
		int32 lastMapIndex = m_lastItemIndex - (m_lastItemIndex % 10);

		int32 res = SelectItemByItemIndex(mapsFirst ? lastMapIndex : m_lastItemIndex);

		if (res == -1)
			// Try other form
			res = SelectItemByItemIndex(mapsFirst ? m_lastItemIndex : lastMapIndex);

		if (res == -1)
		{
			Logf("Couldn't find original map %u after map set change", Logger::Severity::Info, lastMapIndex);
			SelectItemBySortIndex(0);
		}

		return res;
	}

	// Selects an item based on a chosen diff
	void SelectDifficulty(int32 newDiff)
	{
		m_currentlySelectedDiff = newDiff;
		m_SetLuaDiffIndex();

		const Map<int32, SongSelectIndex> &maps = m_SourceCollection();
		const SongSelectIndex *map = maps.Find(m_getCurrentlySelectedItemIndex());
		if (map == NULL)
			return;
		OnChartSelected.Call(map[0].GetCharts()[m_currentlySelectedDiff]);
	}

	void AdvanceDifficultySelection(int32 offset)
	{
		const Map<int32, SongSelectIndex> &maps = m_SourceCollection();

		const SongSelectIndex *map = maps.Find(m_getCurrentlySelectedItemIndex());
		if (map == NULL)
			return;

		int32 newIdx = m_currentlySelectedDiff + offset;
		newIdx = Math::Clamp(newIdx, 0, (int32)map->GetCharts().size() - 1);
		SelectDifficulty(newIdx);

		m_lastItemIndex = newIdx;
	}

	// Called when a new map is selected
	Delegate<FolderIndex *> OnFolderSelected;
	Delegate<ChartIndex *> OnChartSelected;

	// Extra method to get the chart for the current diff
	ChartIndex *GetSelectedChart() const
	{
		SongSelectIndex const *map = m_SourceCollection().Find(
			m_getCurrentlySelectedItemIndex());
		if (map)
			return map->GetCharts()[m_currentlySelectedDiff];
		return nullptr;
	}

	void SetSearchFieldLua(Ref<TextInput> search) override
	{
		if (m_lua == nullptr)
			return;
		lua_getglobal(m_lua, "songwheel");
		//text
		lua_pushstring(m_lua, "searchText");
		lua_pushstring(m_lua, (search->input).c_str());
		lua_settable(m_lua, -3);
		//enabled
		lua_pushstring(m_lua, "searchInputActive");
		lua_pushboolean(m_lua, search->active);
		lua_settable(m_lua, -3);
		//set global
		lua_setglobal(m_lua, "songwheel");
	}

	int GetSelectedDifficultyIndex() const
	{
		return m_currentlySelectedDiff;
	}

	void SelectMapByMapId(uint32 id)
	{
		SelectItemByItemId(id);
	}

	int32 SelectMapByMapIndex(int32 mapIndex)
	{
		return SelectItemByItemIndex(mapIndex);
	}

	uint32 GetCurrentSongIndex()
	{
		return GetCurrentItemIndex();
	}

	// The delegate templater is unhappy if we use the super class function directly
	void OnSearchStatusUpdated(String status) override
	{
		SongItemSelectionWheel::OnSearchStatusUpdated(status);
	}

	void OnItemsAdded(Vector<FolderIndex*> items) override
	{
		SongItemSelectionWheel::OnItemsAdded(items);
	}

	void OnItemsRemoved(Vector<FolderIndex*> items) override
	{
		SongItemSelectionWheel::OnItemsRemoved(items);
	}

	void OnItemsUpdated(Vector<FolderIndex*> items) override
	{
		SongItemSelectionWheel::OnItemsUpdated(items);
	}

	void OnItemsCleared(Map<int32, FolderIndex*> newList) override
	{
		SongItemSelectionWheel::OnItemsCleared(newList);
	}


private:
	// grab the actual FolderIndex from a given selection
	FolderIndex* m_getDBEntryFromItemIndex(const SongSelectIndex ind) const {
		return ind.GetFolder();
	}
	FolderIndex* m_getDBEntryFromItemIndex(const SongSelectIndex* ind) const {
		return ind->GetFolder();
	}
	void m_SetLuaDiffIndex()
	{
		lua_getglobal(m_lua, "set_diff");
		lua_pushinteger(m_lua, (uint64)m_currentlySelectedDiff + 1);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			g_application->ScriptError("songselect/songwheel", m_lua);
		}
	}
	// Set all songs into lua
	void m_SetAllItems() override
	{
		//set all songs
		m_SetLuaMaps("allSongs", m_items, false);
		lua_getglobal(m_lua, "songs_changed");
		if (!lua_isfunction(m_lua, -1))
		{
			lua_pop(m_lua, 1);
			return;
		}
		lua_pushboolean(m_lua, true);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			g_application->ScriptError("songselect/songwheel", m_lua);
		}
	}
	void m_SetCurrentItems() override
	{
		m_SetLuaMaps("songs", m_SourceCollection(), true);

		lua_getglobal(m_lua, "songs_changed");
		if (!lua_isfunction(m_lua, -1))
		{
			lua_pop(m_lua, 1);
			return;
		}
		lua_pushboolean(m_lua, false);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			g_application->ScriptError("songselect/songwheel", m_lua);
		}
	}

	void m_SetLuaMaps(const char *key, const Map<int32, SongSelectIndex> &collection, bool sorted)
	{
		lua_getglobal(m_lua, "songwheel");
		lua_pushstring(m_lua, key);
		lua_newtable(m_lua);
		int songIndex = 0;


		if (sorted)
		{
			// sortVec should only have the current maps in the collection
			for (auto& mapIndex : m_sortVec)
			{
				// Grab the song for this sort index
				SongSelectIndex song = collection.find(mapIndex)->second;
				m_PushSongToLua(song, ++songIndex);
			}
		}
		else {

			for (auto& song : collection)
			{
				m_PushSongToLua(song.second, ++songIndex);
			}
		}
		lua_settable(m_lua, -3);
		lua_setglobal(m_lua, "songwheel");
	}

	void m_PushSongToLua(SongSelectIndex song, int index)
	{
		lua_pushinteger(m_lua, index);
		lua_newtable(m_lua);
		m_PushStringToTable("title", song.GetCharts()[0]->title.c_str());
		m_PushStringToTable("artist", song.GetCharts()[0]->artist.c_str());
		m_PushStringToTable("bpm", song.GetCharts()[0]->bpm.c_str());
		m_PushIntToTable("id", song.GetFolder()->id);
		m_PushStringToTable("path", song.GetFolder()->path.c_str());
		int diffIndex = 0;
		lua_pushstring(m_lua, "difficulties");
		lua_newtable(m_lua);
		for (auto diff : song.GetCharts())
		{
			lua_pushinteger(m_lua, ++diffIndex);
			lua_newtable(m_lua);
			m_PushStringToTable("jacketPath", Path::Normalize(song.GetFolder()->path + "/" + diff->jacket_path).c_str());
			m_PushIntToTable("level", diff->level);
			m_PushIntToTable("difficulty", diff->diff_index);
			m_PushIntToTable("id", diff->id);
			m_PushStringToTable("hash", diff->hash.c_str());
			m_PushStringToTable("effector", diff->effector.c_str());
			m_PushStringToTable("illustrator", diff->illustrator.c_str());
			m_PushIntToTable("topBadge", static_cast<int>(Scoring::CalculateBestBadge(diff->scores)));
			lua_pushstring(m_lua, "scores");
			lua_newtable(m_lua);
			int scoreIndex = 0;
			for (auto& score : diff->scores)
			{
				lua_pushinteger(m_lua, ++scoreIndex);
				lua_newtable(m_lua);
				m_PushFloatToTable("gauge", score->gauge);

				m_PushIntToTable("gauge_type", (uint32)score->gaugeType);
				m_PushIntToTable("gauge_option", score->gaugeOption);
				m_PushIntToTable("random", score->random);
				m_PushIntToTable("mirror", score->mirror);
				m_PushIntToTable("auto_flags", (uint32)score->autoFlags);

				m_PushIntToTable("score", score->score);
				m_PushIntToTable("perfects", score->crit);
				m_PushIntToTable("goods", score->almost);
				m_PushIntToTable("misses", score->miss);
				m_PushIntToTable("timestamp", score->timestamp);
				m_PushIntToTable("badge", static_cast<int>(Scoring::CalculateBadge(*score)));
				lua_settable(m_lua, -3);
			}
			lua_settable(m_lua, -3);
			lua_settable(m_lua, -3);
		}
		lua_settable(m_lua, -3);
		lua_settable(m_lua, -3);
	}

	void m_OnItemSelected(SongSelectIndex index) override
	{
		// Clamp diff selection
		int32 selectDiff = m_currentlySelectedDiff;
		if (m_currentlySelectedDiff >= (int32)index.GetCharts().size())
		{
			selectDiff = (int32)index.GetCharts().size() - 1;
		}
		SelectDifficulty(selectDiff);

		OnFolderSelected.Call(index.GetFolder());
		m_currentlySelectedItemId = index.GetFolder()->id;
	}
};

/*
	Filter selection element
*/
class FilterSelection
{
public:
	FilterSelection(Ref<SelectionWheel> selectionWheel) : m_selectionWheel(selectionWheel)
	{
	}

	bool Init()
	{
		SongFilter *lvFilter = new SongFilter();
		SongFilter *flFilter = new SongFilter();

		AddFilter(lvFilter, FilterType::Level);
		AddFilter(flFilter, FilterType::Folder);
		for (size_t i = 1; i <= 20; i++)
		{
			AddFilter(new LevelFilter(i), FilterType::Level);
		}
		CheckedLoad(m_lua = g_application->LoadScript("songselect/filterwheel"));
		return true;
	}
	void ReloadScript()
	{
		g_application->ReloadScript("songselect/filterwheel", m_lua);
	}
	void Render(float deltaTime)
	{
		lua_getglobal(m_lua, "render");
		lua_pushnumber(m_lua, deltaTime);
		lua_pushboolean(m_lua, Active);
		if (lua_pcall(m_lua, 2, 0, 0) != 0)
		{
			g_application->ScriptError("songselect/filterwheel", m_lua);
		}
	}
	~FilterSelection()
	{
		g_gameConfig.Set(GameConfigKeys::FolderFilter, m_currentFolderSelection);
		g_gameConfig.Set(GameConfigKeys::LevelFilter, m_currentLevelSelection);

		for (auto filter : m_levelFilters)
		{
			delete filter;
		}
		for (auto filter : m_folderFilters)
		{
			if (filter->GetType() == FilterType::Folder)
			{
				FolderFilter *f = (FolderFilter *)filter;
				delete f;
			}
			else
			{
				delete filter;
			}
		}
		m_levelFilters.clear();
		m_folderFilters.clear();

		if (m_lua)
			g_application->DisposeLua(m_lua);
	}

	bool Active = false;

	bool IsAll()
	{
		bool isFiltered = false;
		for (size_t i = 0; i < 2; i++)
		{
			if (!m_currentFilters[i]->IsAll())
				return true;
		}
		return false;
	}

	void AddFilter(SongFilter *filter, FilterType type)
	{
		if (type == FilterType::Level)
			m_levelFilters.Add(filter);
		else
			m_folderFilters.Add(filter);
	}

	void SelectFilter(SongFilter *filter, FilterType type)
	{
		uint8 t = type == FilterType::Folder ? 0 : 1;
		int index = 0;
		if (type != FilterType::Level)
		{
			index = std::find(m_folderFilters.begin(), m_folderFilters.end(), filter) - m_folderFilters.begin();
		}
		else
		{
			index = std::find(m_levelFilters.begin(), m_levelFilters.end(), filter) - m_levelFilters.begin();
		}

		lua_getglobal(m_lua, "set_selection");
		lua_pushnumber(m_lua, index + 1);
		lua_pushboolean(m_lua, type == FilterType::Folder);
		if (lua_pcall(m_lua, 2, 0, 0) != 0)
		{
			g_application->ScriptError("songselect/filterwheel", m_lua);
		}
		m_currentFilters[t] = filter;
		m_selectionWheel->SetFilter(m_currentFilters);
	}

	void SetFiltersByIndex(uint32 level, uint32 folder)
	{
		if (level >= m_levelFilters.size())
		{
			Log("LevelFilter out of range.", Logger::Severity::Error);
		}
		else
		{
			m_currentLevelSelection = level;
			SelectFilter(m_levelFilters[level], FilterType::Level);
		}

		if (folder >= m_folderFilters.size())
		{
			Log("FolderFilter out of range.", Logger::Severity::Error);
		}
		else
		{
			m_currentFolderSelection = folder;
			SelectFilter(m_folderFilters[folder], FilterType::Folder);
		}
	}

	void AdvanceSelection(int32 offset)
	{
		if (m_selectingFolders)
		{
			m_currentFolderSelection = ((int)m_currentFolderSelection + offset) % (int)m_folderFilters.size();
			if (m_currentFolderSelection < 0)
				m_currentFolderSelection = m_folderFilters.size() + m_currentFolderSelection;
			SelectFilter(m_folderFilters[m_currentFolderSelection], FilterType::Folder);
		}
		else
		{
			m_currentLevelSelection = ((int)m_currentLevelSelection + offset) % (int)m_levelFilters.size();
			if (m_currentLevelSelection < 0)
				m_currentLevelSelection = m_levelFilters.size() + m_currentLevelSelection;
			SelectFilter(m_levelFilters[m_currentLevelSelection], FilterType::Level);
		}
	}

	void SetMapDB(MapDatabase *db)
	{
		m_mapDB = db;
		UpdateFilters();
		m_SetLuaTable();
	}

	void ToggleSelectionMode()
	{
		m_selectingFolders = !m_selectingFolders;
		lua_getglobal(m_lua, "set_mode");
		lua_pushboolean(m_lua, m_selectingFolders);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			g_application->ScriptError("songselect/filterwheel", m_lua);
		}
	}
	void OnSongsChanged()
	{
		UpdateFilters();
	}

	// Check if any new folders or collections should be added and add them
	void UpdateFilters()
	{
		for (std::string c : m_mapDB->GetCollections())
		{
			if (m_collections.find(c) == m_collections.end())
			{
				CollectionFilter *filter = new CollectionFilter(c, m_mapDB);
				AddFilter(filter, FilterType::Collection);
				m_collections.insert(c);
			}
		}

		for (std::string p : Path::GetSubDirs(Path::Absolute(g_gameConfig.GetString(GameConfigKeys::SongFolder))))
		{
			if (m_folders.find(p) == m_folders.end())
			{
				FolderFilter *filter = new FolderFilter(p, m_mapDB);
				if (filter->GetFiltered(Map<int32, SongSelectIndex>()).size() > 0)
				{
					AddFilter(filter, FilterType::Folder);
					m_folders.insert(p);
				}
				else
				{
					delete filter;
				}
			}
		}

		//sort the new folderfilter vector
		m_folderFilters.Sort([](const SongFilter *a, const SongFilter *b) {
			String aupper = a->GetName();
			aupper.ToUpper();
			String bupper = b->GetName();
			bupper.ToUpper();
			return aupper.compare(bupper) < 0;
		});

		//set the selection index to match the selected filter
		m_currentFolderSelection = std::find(m_folderFilters.begin(), m_folderFilters.end(), m_currentFilters[0]) - m_folderFilters.begin();
		m_SetLuaTable();
		SelectFilter(m_currentFilters[0], FilterType::Folder);
	}

	String GetStatusText()
	{
		return Utility::Sprintf("%s / %s", m_currentFilters[0]->GetName(), m_currentFilters[1]->GetName());
	}

private:
	void m_PushStringToTable(const char *name, const char *data)
	{
		lua_pushstring(m_lua, name);
		lua_pushstring(m_lua, data);
		lua_settable(m_lua, -3);
	}
	void m_PushStringToArray(int index, const char *data)
	{
		lua_pushinteger(m_lua, index);
		lua_pushstring(m_lua, data);
		lua_settable(m_lua, -3);
	}

	void m_SetLuaTable()
	{
		lua_newtable(m_lua);
		{
			lua_pushstring(m_lua, "level");
			lua_newtable(m_lua); //level filters
			{
				for (size_t i = 0; i < m_levelFilters.size(); i++)
				{
					m_PushStringToArray(i + 1, m_levelFilters[i]->GetName().c_str());
				}
			}
			lua_settable(m_lua, -3);

			lua_pushstring(m_lua, "folder");
			lua_newtable(m_lua); //folder filters
			{
				for (size_t i = 0; i < m_folderFilters.size(); i++)
				{
					m_PushStringToArray(i + 1, m_folderFilters[i]->GetName().c_str());
				}
			}
			lua_settable(m_lua, -3);
		}
		lua_setglobal(m_lua, "filters");

		lua_getglobal(m_lua, "tables_set");
		if (lua_isfunction(m_lua, -1))
		{
			if (lua_pcall(m_lua, 0, 0, 0) != 0)
			{
				g_application->ScriptError("songselect/filterwheel", m_lua);
			}
		}
	}

	Ref<SelectionWheel> m_selectionWheel;
	Vector<SongFilter *> m_folderFilters;
	Vector<SongFilter *> m_levelFilters;
	int32 m_currentFolderSelection = 0;
	int32 m_currentLevelSelection = 0;
	bool m_selectingFolders = true;
	SongFilter *m_currentFilters[2] = {nullptr};
	MapDatabase *m_mapDB;
	lua_State *m_lua = nullptr;
	std::unordered_set<std::string> m_folders;
	std::unordered_set<std::string> m_collections;
};

/*
	Sorting selection element
*/
class SortSolection
{
public:
	bool Active = false;
	bool Initialized = false;

	SortSolection(Ref<SelectionWheel> selectionWheel) : m_selectionWheel(selectionWheel) {}
	~SortSolection()
	{
		g_gameConfig.Set(GameConfigKeys::LastSort, m_selection);
		for (SongSort *s : m_sorts)
		{
			delete s;
			// TitleSort *t = (TitleSort *)s;
			// ScoreSort *sc = (ScoreSort *)s;
			// DateSort *d = (DateSort *)s;
			// EffectorSort *e = (EffectorSort *)s;
			// ArtistSort* a = (ArtistSort*)s;
			// switch (s->GetType())
			// {
			// case TITLE_ASC:
			// case TITLE_DESC:
			// 	delete t;
			// 	break;
			// case SCORE_DESC:
			// case SCORE_ASC:
			// 	delete sc;
			// 	break;
			// case DATE_DESC:
			// case DATE_ASC:
			// 	delete d;
			// 	break;
			// case EFFECTOR_DESC:
			// case EFFECTOR_ASC:
			// 	delete e;
			// 	break;
			// case ARTIST_ASC:
			// case ARTIST_DESC:
			// 	delete a;
			// 	break;
			// default:
			// 	break;
			// }
		}
		m_sorts.clear();

		if (m_lua)
		{
			g_application->DisposeLua(m_lua);
			m_lua = nullptr;
		}
	}

	bool Init()
	{
		if (m_sorts.size() == 0)
		{
			m_sorts.Add(new TitleSort("Title ^", false));
			m_sorts.Add(new TitleSort("Title v", true));
			m_sorts.Add(new ScoreSort("Score ^", false));
			m_sorts.Add(new ScoreSort("Score v", true));
			m_sorts.Add(new DateSort("Date ^", false));
			m_sorts.Add(new DateSort("Date v", true));
			m_sorts.Add(new ClearMarkSort("Badge ^", false));
			m_sorts.Add(new ClearMarkSort("Badge v", true));
			m_sorts.Add(new ArtistSort("Artist ^", false));
			m_sorts.Add(new ArtistSort("Artist v", true));
			m_sorts.Add(new EffectorSort("Effector ^", false));
			m_sorts.Add(new EffectorSort("Effector v", true));
		}

		CheckedLoad(m_lua = g_application->LoadScript("songselect/sortwheel"));
		m_SetLuaTable();

		Initialized = true;
		m_selection = g_gameConfig.GetInt(GameConfigKeys::LastSort);
		AdvanceSelection(0);
		return true;
	}

	void SetSelection(SongSort *s)
	{
		m_selection = std::find(m_sorts.begin(), m_sorts.end(), s) - m_sorts.begin();
		m_selectionWheel->SetSort(s);

		lua_getglobal(m_lua, "set_selection");
		lua_pushnumber(m_lua, m_selection + 1);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			g_application->ScriptError("songselect/sortwheel", m_lua);
		}
	}

	void Render(float deltaTime)
	{
		lua_getglobal(m_lua, "render");
		lua_pushnumber(m_lua, deltaTime);
		lua_pushboolean(m_lua, Active);
		if (lua_pcall(m_lua, 2, 0, 0) != 0)
		{
			g_application->ScriptError("songselect/sortwheel", m_lua);
		}
	}

	void AdvanceSelection(int offset)
	{
		int newSelection = (m_selection + offset) % (int)m_sorts.size();
		if (newSelection < 0)
		{
			newSelection = m_sorts.size() + newSelection;
		}

		SetSelection(m_sorts.at(newSelection));
	}

private:
	void m_PushStringToArray(int index, const char *data)
	{
		lua_pushinteger(m_lua, index);
		lua_pushstring(m_lua, data);
		lua_settable(m_lua, -3);
	}

	void m_SetLuaTable()
	{
		lua_newtable(m_lua);
		{
			for (size_t i = 0; i < m_sorts.size(); i++)
			{
				m_PushStringToArray(i + 1, m_sorts[i]->GetName().c_str());
			}
		}
		lua_setglobal(m_lua, "sorts");

		lua_getglobal(m_lua, "tables_set");
		if (lua_isfunction(m_lua, -1))
		{
			if (lua_pcall(m_lua, 0, 0, 0) != 0)
			{
				g_application->ScriptError("songselect/sortwheel", m_lua);
			}
		}
	}

	Ref<SelectionWheel> m_selectionWheel;
	Vector<SongSort *> m_sorts;
	int m_selection = 0;
	lua_State *m_lua = nullptr;
};

/*
	Song select window/screen
*/
class SongSelect_Impl : public SongSelect
{
private:
	PreviewParams m_previewParams;

	Timer m_dbUpdateTimer;
	MapDatabase* m_mapDatabase;

	// Map selection wheel
	Ref<SelectionWheel> m_selectionWheel;
	// Filter selection
	Ref<FilterSelection> m_filterSelection;
	// Sort selection
	Ref<SortSolection> m_sortSelection;
	// Search text logic object
	Ref<TextInput> m_searchInput;

	// Player of preview music
	PreviewPlayer m_previewPlayer;

	// Select sound
	Sample m_selectSound;

	// Navigation variables
	float m_advanceSong = 0.0f;
	float m_advanceDiff = 0.0f;
	float m_sensMult = 1.0f;
	MouseLockHandle m_lockMouse;
	bool m_suspended = true;
	bool m_hasRestored = false;
	Map<Input::Button, float> m_timeSinceButtonPressed;
	Map<Input::Button, float> m_timeSinceButtonReleased;
	lua_State* m_lua = nullptr;

	MultiplayerScreen* m_multiplayer = nullptr;
	CollectionDialog m_collDiag;
	GameplaySettingsDialog m_settDiag;

	int m_shiftDown = 0;

	bool m_hasCollDiag = false;
	bool m_transitionedToGame = false;
	int32 m_lastMapIndex = -1;

	DBUpdateScreen* m_dbUpdateScreen = nullptr;

public:
	SongSelect_Impl() : m_settDiag(this) {}

	bool AsyncLoad() override
	{
		m_selectSound = g_audio->CreateSample("audio/menu_click.wav");
		m_selectionWheel = Ref<SelectionWheel>(new SelectionWheel(this));
		m_filterSelection = Ref<FilterSelection>(new FilterSelection(m_selectionWheel));
		m_mapDatabase = new MapDatabase(true);

		// Add database update hook before triggering potential update
		m_mapDatabase->OnDatabaseUpdateStarted.Add(this, &SongSelect_Impl::m_onDatabaseUpdateStart);
		m_mapDatabase->OnDatabaseUpdateDone.Add(this, &SongSelect_Impl::m_onDatabaseUpdateDone);
		m_mapDatabase->OnDatabaseUpdateProgress.Add(this, &SongSelect_Impl::m_onDatabaseUpdateProgress);
		m_mapDatabase->SetChartUpdateBehavior(g_gameConfig.GetBool(GameConfigKeys::TransferScoresOnChartUpdate));
		m_mapDatabase->FinishInit();

		// Setup the map database
		m_mapDatabase->AddSearchPath(g_gameConfig.GetString(GameConfigKeys::SongFolder));

		return true;
	}

	void m_onDatabaseUpdateStart(int max)
	{
		m_dbUpdateScreen = new DBUpdateScreen(max);
		g_application->AddTickable(m_dbUpdateScreen);
	}

	void m_onDatabaseUpdateProgress(int current, int max)
	{
		if (!m_dbUpdateScreen)
			return;
		m_dbUpdateScreen->SetCurrent(current, max);
	}

	void m_onDatabaseUpdateDone()
	{
		if (!m_dbUpdateScreen)
			return;
		g_application->RemoveTickable(m_dbUpdateScreen);
		m_dbUpdateScreen = NULL;
	}

	String m_getCurrentChartName()
	{
		return String();
	}

	void m_SetCurrentChartOffset(int newValue)
	{
		if (ChartIndex* chart = GetCurrentSelectedChart())
		{
			chart->custom_offset = newValue;
			m_mapDatabase->UpdateChartOffset(chart);
		}
	}

	bool AsyncFinalize() override
	{
		CheckedLoad(m_lua = g_application->LoadScript("songselect/background"));
		g_input.OnButtonPressed.Add(this, &SongSelect_Impl::m_OnButtonPressed);
		g_input.OnButtonReleased.Add(this, &SongSelect_Impl::m_OnButtonReleased);
		g_gameWindow->OnMouseScroll.Add(this, &SongSelect_Impl::m_OnMouseScroll);

		if (!m_selectionWheel->Init())
			return false;
		if (!m_filterSelection->Init())
			return false;
		m_filterSelection->SetMapDB(m_mapDatabase);


		m_mapDatabase->OnFoldersAdded.Add(m_selectionWheel.get(), &SelectionWheel::OnItemsAdded);
		m_mapDatabase->OnFoldersUpdated.Add(m_selectionWheel.get(), &SelectionWheel::OnItemsUpdated);
		m_mapDatabase->OnFoldersCleared.Add(m_selectionWheel.get(), &SelectionWheel::OnItemsCleared);
		m_mapDatabase->OnFoldersRemoved.Add(m_selectionWheel.get(), &SelectionWheel::OnItemsRemoved);
		m_mapDatabase->OnSearchStatusUpdated.Add(m_selectionWheel.get(), &SelectionWheel::OnSearchStatusUpdated);
		m_selectionWheel->OnItemsChanged.Add(m_filterSelection.get(), &FilterSelection::OnSongsChanged);
		m_mapDatabase->StartSearching();

		m_filterSelection->SetFiltersByIndex(g_gameConfig.GetInt(GameConfigKeys::LevelFilter), g_gameConfig.GetInt(GameConfigKeys::FolderFilter));

		//sort selection
		m_sortSelection = Ref<SortSolection>(new SortSolection(m_selectionWheel));
		if (!m_sortSelection->Init())
			return false;

		m_selectionWheel->SelectMapByMapId(g_gameConfig.GetInt(GameConfigKeys::LastSelected));

		m_selectionWheel->OnFolderSelected.Add(this, &SongSelect_Impl::OnFolderSelected);
		m_selectionWheel->OnChartSelected.Add(this, &SongSelect_Impl::OnChartSelected);

		m_searchInput = Ref<TextInput>(new TextInput());
		m_searchInput->OnTextChanged.Add(this, &SongSelect_Impl::OnSearchTermChanged);

		/// TODO: Check if debugmute is enabled
		g_audio->SetGlobalVolume(g_gameConfig.GetFloat(GameConfigKeys::MasterVolume));

		m_sensMult = g_gameConfig.GetFloat(GameConfigKeys::SongSelSensMult);
		m_previewParams = {"", 0, 0};
		m_hasCollDiag = m_collDiag.Init(m_mapDatabase);

		if (!m_settDiag.Init())
			return false;

		m_settDiag.onSongOffsetChange.Add(this, &SongSelect_Impl::m_SetCurrentChartOffset);

		m_settDiag.onPressAutoplay.AddLambda([this]() {
			if (m_multiplayer != nullptr) return;

			ChartIndex* chart = GetCurrentSelectedChart();
			if (chart == nullptr) return;
			Game* game = Game::Create(chart, Game::PlaybackOptionsFromSettings());
			if (!game)
			{
				Log("Failed to start game", Logger::Severity::Error);
				return;
			}

			game->GetScoring().autoplayInfo.autoplay = true;

			if(m_settDiag.IsActive()) m_settDiag.Close();
			m_suspended = true;

			// Transition to game
			g_transition->TransitionTo(game);
		});

		m_settDiag.onPressPractice.AddLambda([this]() {
			if (m_multiplayer != nullptr) return;

			ChartIndex* chart = GetCurrentSelectedChart();
			if (chart == nullptr) return;
			m_mapDatabase->UpdateChartOffset(chart);

			Game* practiceGame = Game::CreatePractice(chart, Game::PlaybackOptionsFromSettings());
			if (!practiceGame)
			{
				Log("Failed to start practice", Logger::Severity::Error);
				return;
			}

			if (m_settDiag.IsActive()) m_settDiag.Close();
			m_suspended = true;

			practiceGame->SetSongDB(m_mapDatabase);

			// Transition to practice mode
			g_transition->TransitionTo(practiceGame);
		});

		m_settDiag.onPressComputeSongOffset.AddLambda([this]() {
			ChartIndex* chart = GetCurrentSelectedChart();
			if (!chart) return;

			m_previewPlayer.Pause();

			if (OffsetComputer::Compute(chart, chart->custom_offset))
			{
				m_mapDatabase->UpdateChartOffset(chart);
			}

			m_previewPlayer.Restore();
		});

		if (m_hasCollDiag)
		{
			m_collDiag.OnCompletion.Add(this, &SongSelect_Impl::m_OnSongAddedToCollection);
		}

		return true;
	}

	bool Init() override
	{
		return true;
	}
	~SongSelect_Impl()
	{
		// Clear callbacks
		if (m_mapDatabase)
		{
			m_mapDatabase->OnFoldersCleared.Clear();
			delete m_mapDatabase;
		}
		g_input.OnButtonPressed.RemoveAll(this);
		g_input.OnButtonReleased.RemoveAll(this);
		g_gameWindow->OnMouseScroll.RemoveAll(this);

		if (m_lua)
			g_application->DisposeLua(m_lua);
	}

	void m_OnSongAddedToCollection()
	{
		m_filterSelection->UpdateFilters();
		OnSearchTermChanged(m_searchInput->input);
	}

	void m_updatePreview(ChartIndex *diff, bool mapChanged)
	{
		String mapRootPath = diff->path.substr(0, diff->path.find_last_of(Path::sep));

		// Set current preview audio
		String audioPath = mapRootPath + Path::sep + diff->preview_file;

		PreviewParams params = {audioPath, static_cast<uint32>(diff->preview_offset), static_cast<uint32>(diff->preview_length)};

		/* A lot of pre-effected charts use different audio files for each difficulty; these
		 * files differ only in their effects, so the preview offset and duration remain the
		 * same. So, if the audio file is different but offset and duration equal the previously
		 * playing preview, we know that it was just a change to a different difficulty of the
		 * same chart. To avoid restarting the preview when changing difficulty, we say that
		 * charts with this setup all have the same preview.
		 *
		 * Note that if the chart is using the `previewfile` field, then all this is ignored. */
		bool newPreview = mapChanged ? m_previewParams != params : (m_previewParams.duration != params.duration || m_previewParams.offset != params.offset);

		if (newPreview)
		{
			Ref<AudioStream> previewAudio = g_audio->CreateStream(audioPath);
			if (previewAudio)
			{
				previewAudio->SetPosition(diff->preview_offset);

				m_previewPlayer.FadeTo(previewAudio);

				m_previewParams = params;
			}
			else
			{
				params = {"", 0, 0};

				Logf("Failed to load preview audio from [%s]", Logger::Severity::Warning, audioPath);
				if (m_previewParams != params)
					m_previewPlayer.FadeTo(Ref<AudioStream>());
			}

			m_previewParams = params;
		}
	}

	// When a map is selected in the song wheel
	void OnFolderSelected(FolderIndex *folder)
	{
		if (!folder->charts.empty() && (int)folder->charts.size() > m_selectionWheel->GetSelectedDifficultyIndex())
			m_updatePreview(folder->charts[m_selectionWheel->GetSelectedDifficultyIndex()], true);
	}
	// When a difficulty is selected in the song wheel
	void OnChartSelected(ChartIndex *chart)
	{
		m_updatePreview(chart, false);
	}

	/// TODO: Fix some conflicts between search field and filter selection
	void OnSearchTermChanged(const String &search)
	{
		if (search.empty())
			m_filterSelection->AdvanceSelection(0);
		else
		{
			String effector;
			String author;
			String bpm;
			const String query = SearchParser::Parse(search, {
				{ "effector", &effector },
				{ "author", &author },
				{ "bpm", &bpm },
			});
			Map<int32, FolderIndex*> filter = m_mapDatabase->FindFoldersWithFilter(query, {
				{ "effector", effector },
				{ "author", author },
				{ "bpm", bpm }
			});
			m_selectionWheel->SetFilter(filter);
		}
	}

	void m_OnButtonPressed(Input::Button buttonCode, int32 delta)
	{
		if (m_multiplayer && m_multiplayer->GetChatOverlay()->IsOpen())
			return;
		if (m_suspended || m_collDiag.IsActive() || m_settDiag.IsActive())
			return;

		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Keyboard && m_searchInput->active)
			if (buttonCode != Input::Button::SongSelect_OpenSearch && buttonCode != Input::Button::SongSelect_CloseSearch)
				return;

		m_timeSinceButtonPressed[buttonCode] = 0;

		if (buttonCode == Input::Button::BT_S && !m_filterSelection->Active && !m_sortSelection->Active && !IsSuspended() && !m_transitionedToGame)
		{
			bool autoplay = (g_gameWindow->GetModifierKeys() & ModifierKeys::Ctrl) == ModifierKeys::Ctrl;
			FolderIndex *folder = m_selectionWheel->GetSelection();
			if (folder)
			{
				if (m_multiplayer != nullptr)
				{
					// When we are in multiplayer, just report the song and exit instead
					m_multiplayer->SetSelectedMap(folder, GetCurrentSelectedChart());

					m_suspended = true;
					g_application->RemoveTickable(this);
					return;
				}

				ChartIndex* chart = GetCurrentSelectedChart();
				m_mapDatabase->UpdateChartOffset(chart);
				Game *game = Game::Create(chart, Game::PlaybackOptionsFromSettings());
				if (!game)
				{
					Log("Failed to start game", Logger::Severity::Error);
					return;
				}
				game->GetScoring().autoplayInfo.autoplay = autoplay;

				// Transition to game
				g_transition->TransitionTo(game);
				m_transitionedToGame = true;
			}
		}
		else
		{
			if (m_sortSelection->Active && buttonCode == Input::Button::BT_S)
			{
				m_sortSelection->Active = false;
			}
			else if (m_filterSelection->Active)
			{
				switch (buttonCode)
				{
				case Input::Button::BT_S:
					m_filterSelection->ToggleSelectionMode();
					break;
				case Input::Button::Back:
					m_filterSelection->Active = false;
					break;
				default:
					break;
				}
			}
			else if (m_sortSelection->Active)
			{
				switch (buttonCode)
				{
				case Input::Button::Back:
					m_sortSelection->Active = false;
					break;
				default:
					break;
				}
			}
			else
			{
				switch (buttonCode)
				{
				case Input::Button::BT_1:
					if (g_input.GetButton(Input::Button::BT_2))
						m_collDiag.Open(GetCurrentSelectedChart());
					break;
				case Input::Button::BT_2:
					if (g_input.GetButton(Input::Button::BT_1))
						m_collDiag.Open(GetCurrentSelectedChart());
					break;

				case Input::Button::FX_1:
					if (g_input.GetButton(Input::Button::FX_0))
					{
						m_settDiag.Open();
					}
					break;
				case Input::Button::FX_0:
					if (g_input.GetButton(Input::Button::FX_1))
					{
						m_settDiag.Open();
					}
					break;
				case Input::Button::BT_S:
					break;
				case Input::Button::Back:
					m_suspended = true;
					g_application->RemoveTickable(this);
					break;
				default:
					break;
				}
			}
		}

		if (m_filterSelection->Active) {
			switch (buttonCode) {
				case Input::Button::SongSelect_Down:
					m_filterSelection->AdvanceSelection(1);
					break;
				case Input::Button::SongSelect_Up:
					m_filterSelection->AdvanceSelection(-1);
					break;
			}
		} else if (m_sortSelection->Active) {
			switch (buttonCode) {
				case Input::Button::SongSelect_Down:
					m_sortSelection->AdvanceSelection(1);
					break;
				case Input::Button::SongSelect_Up:
					m_sortSelection->AdvanceSelection(-1);
					break;
			}
		} else {
			switch (buttonCode) {
				case Input::Button::SongSelect_Down:
					m_selectionWheel->AdvanceSelection(1);
					break;
				case Input::Button::SongSelect_Up:
					m_selectionWheel->AdvanceSelection(-1);
					break;
				case Input::Button::SongSelect_FastDown:
					m_selectionWheel->AdvancePage(1);
					break;
				case Input::Button::SongSelect_FastUp:
					m_selectionWheel->AdvancePage(-1);
					break;
				case Input::Button::SongSelect_Easier:
					m_selectionWheel->AdvanceDifficultySelection(-1);
					break;
				case Input::Button::SongSelect_Harder:
					m_selectionWheel->AdvanceDifficultySelection(1);
					break;
				case Input::Button::SongSelect_Collections:
					if (m_hasCollDiag)
						m_collDiag.Open(GetCurrentSelectedChart());
					break;
				case Input::Button::SongSelect_Random:
					m_selectionWheel->SelectRandom();
					break;
				case Input::Button::SongSelect_ReloadSongs:
					m_mapDatabase->StartSearching();
					OnSearchTermChanged(m_searchInput->input);
					break;
				case Input::Button::SongSelect_Demo:
					StartDemo();
					break;
				case Input::Button::ReloadSkin:
					m_selectionWheel->ReloadScript();
					m_filterSelection->ReloadScript();
					g_application->ReloadScript("songselect/background", m_lua);
					break;
				case Input::Button::SongSelect_OpenEditor:
					OpenEditor();
					break;
				case Input::Button::SongSelect_OpenDirectory:
					Path::ShowInFileBrowser(m_selectionWheel->GetSelection()->path);
					break;
				case Input::Button::SongSelect_OpenSearch:
					m_searchInput->SetActive(!m_searchInput->active);
					break;
				case Input::Button::SongSelect_CloseSearch:
					if (m_searchInput->active) m_searchInput->SetActive(false);
					break;
				case Input::Button::SongSelect_StartPractice:
					m_settDiag.onPressPractice.Call();
					break;
			}
		}
	}

	void m_OnButtonReleased(Input::Button buttonCode, int32 delta)
	{
		if (m_multiplayer && m_multiplayer->GetChatOverlay()->IsOpen())
			return;
		if (m_suspended || m_collDiag.IsActive() || m_settDiag.IsActive())
			return;

		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Keyboard && m_searchInput->active)
			return;

		m_timeSinceButtonReleased[buttonCode] = 0;

		switch (buttonCode)
		{
		case Input::Button::FX_0:
			if (m_timeSinceButtonPressed[Input::Button::FX_0] < m_timeSinceButtonPressed[Input::Button::FX_1] && !g_input.GetButton(Input::Button::FX_1) && !m_sortSelection->Active)
			{
				m_filterSelection->Active = !m_filterSelection->Active;
			}
			break;
		case Input::Button::FX_1:
			if (m_timeSinceButtonPressed[Input::Button::FX_1] < m_timeSinceButtonPressed[Input::Button::FX_0] && !g_input.GetButton(Input::Button::FX_0) && !m_filterSelection->Active)
			{
				m_sortSelection->Active = !m_sortSelection->Active;
			}
			break;
			default:
			break;
		}
	}
	void m_OnMouseScroll(int32 steps)
	{
		if (m_suspended || m_collDiag.IsActive() || m_settDiag.IsActive())
			return;

		if (m_multiplayer && m_multiplayer->GetChatOverlay()->IsOpen())
			return;

		if (m_sortSelection->Active)
		{
			m_sortSelection->AdvanceSelection(steps);
		}
		else if (m_filterSelection->Active)
		{
			m_filterSelection->AdvanceSelection(steps);
		}
		else
		{
			m_selectionWheel->AdvanceSelection(steps);
		}
	}
	void OnKeyPressed(SDL_Scancode code, int32 delta) override
	{
		if (m_multiplayer &&
				m_multiplayer->GetChatOverlay()->OnKeyPressedConsume(code))
			return;

		if (m_collDiag.IsActive() || m_settDiag.IsActive())
			return;

		if (!(m_filterSelection->Active || m_sortSelection->Active))
		{
			if (code == SDL_SCANCODE_LSHIFT || code == SDL_SCANCODE_RSHIFT )
			{
				m_shiftDown |= (code == SDL_SCANCODE_LSHIFT? 1 : 2);
			}
			else if (code == SDL_SCANCODE_DELETE)
			{
				m_previewParams = {"", 0, 0};

				m_previewPlayer.FadeTo(Ref<AudioStream>());
				m_previewPlayer.StopCurrent();

				ChartIndex* chart = m_selectionWheel->GetSelectedChart();
				FolderIndex* folder = m_mapDatabase->GetFolder(chart->folderId);

				bool deleteFolder = m_shiftDown !=0 || folder->charts.size() == 1;

				if (deleteFolder)
				{
					bool res = g_gameWindow->ShowYesNoMessage("Delete chart folder?",
						"Are you sure you want to delete " + folder->path + " and all its difficulties\nThis cannot be undone");
					if (!res)
						return;
					Path::DeleteDir(folder->path);
				}
				else
				{
					String name = chart->title + " [" + chart->diff_shortname + "]";
					bool res = g_gameWindow->ShowYesNoMessage("Delete chart difficulty?",
						"Are you sure you want to delete " + name + "\nThis will only delete " + chart->path + "\nThis cannot be undone...");
					if (!res)
						return;
					Path::Delete(chart->path);
				}
				// Seems to have an issue here where it can get stuck in the other thread
				m_mapDatabase->StartSearching();
				OnSearchTermChanged(m_searchInput->input);
				// TODO if last chart in folder then remove whole folder
			}
		}
	}
	void OnKeyReleased(SDL_Scancode code, int32 delta) override
	{
		if (code == SDL_SCANCODE_LSHIFT)
		{
			m_shiftDown &= ~(code == SDL_SCANCODE_LSHIFT? 1 : 2);
		}
	}
	void Tick(float deltaTime) override
	{
		if (m_dbUpdateTimer.Milliseconds() > 500)
		{
			m_mapDatabase->Update();
			m_dbUpdateTimer.Restart();
		}

		// Tick navigation
		if (!IsSuspended())
		{
			TickNavigation(deltaTime);
			m_previewPlayer.Update(deltaTime);
			m_searchInput->Tick();
			m_selectionWheel->SetSearchFieldLua(m_searchInput);
			if (m_collDiag.IsActive())
			{
				m_collDiag.Tick(deltaTime);
			}
			m_settDiag.Tick(deltaTime);
		}
		if (m_multiplayer)
			m_multiplayer->GetChatOverlay()->Tick(deltaTime);
	}

	void Render(float deltaTime) override
	{
		if (m_suspended && m_hasRestored) return;
		lua_getglobal(m_lua, "render");
		lua_pushnumber(m_lua, deltaTime);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			g_application->ScriptError("songselect/background", m_lua);
		}

		m_selectionWheel->Render(deltaTime);
		m_filterSelection->Render(deltaTime);
		m_sortSelection->Render(deltaTime);

		if (m_collDiag.IsActive())
		{
			m_collDiag.Render(deltaTime);
		}
		m_settDiag.Render(deltaTime);

		if (m_multiplayer)
			m_multiplayer->GetChatOverlay()->Render(deltaTime);
	}

	void TickNavigation(float deltaTime)
	{
		// Lock mouse to screen when active
		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice) == InputDevice::Mouse && g_gameWindow->IsActive())
		{
			if (!m_lockMouse)
				m_lockMouse = g_input.LockMouse();
		}
		else
		{
			if (m_lockMouse)
				m_lockMouse.reset();
			g_gameWindow->SetCursorVisible(true);
		}

		for (size_t i = 0; i < (size_t)Input::Button::Length; i++)
		{
			m_timeSinceButtonPressed[(Input::Button)i] += deltaTime;
			m_timeSinceButtonReleased[(Input::Button)i] += deltaTime;
		}

		if (m_settDiag.IsActive())
		{
			return;
		}

		// Song navigation using laser inputs
		/// TODO: Investigate strange behaviour further and clean up.
		float diff_input = g_input.GetInputLaserDir(0) * m_sensMult;
		float song_input = g_input.GetInputLaserDir(1) * m_sensMult;

		m_advanceDiff += diff_input;
		m_advanceSong += song_input;

		int advanceDiffActual = (int)Math::Floor(m_advanceDiff * Math::Sign(m_advanceDiff)) * Math::Sign(m_advanceDiff);

		int advanceSongActual = (int)Math::Floor(m_advanceSong * Math::Sign(m_advanceSong)) * Math::Sign(m_advanceSong);

		if (m_filterSelection->Active)
		{
			if (advanceDiffActual != 0)
				m_filterSelection->AdvanceSelection(advanceDiffActual);
			if (advanceSongActual != 0)
				m_filterSelection->AdvanceSelection(advanceSongActual);
		}
		else if (m_sortSelection->Active)
		{
			if (advanceDiffActual != 0)
				m_sortSelection->AdvanceSelection(advanceDiffActual);
			if (advanceSongActual != 0)
				m_sortSelection->AdvanceSelection(advanceSongActual);
		}
		else
		{
			if (advanceDiffActual != 0)
				m_selectionWheel->AdvanceDifficultySelection(advanceDiffActual);
			if (advanceSongActual != 0)
				m_selectionWheel->AdvanceSelection(advanceSongActual);
		}

		m_advanceDiff -= advanceDiffActual;
		m_advanceSong -= advanceSongActual;
	}

	void OnSuspend() override
	{
		m_lastMapIndex = m_selectionWheel->GetCurrentSongIndex();

		m_suspended = true;
		m_previewPlayer.Pause();
		m_mapDatabase->PauseSearching();
		if (m_lockMouse)
			m_lockMouse.reset();
	}
	void OnRestore() override
	{
		g_application->DiscordPresenceMenu("Song Select");
		m_suspended = false;
		m_hasRestored = true;
		m_transitionedToGame = false;
		m_previewPlayer.Restore();
		m_mapDatabase->Update(); //flush pending db changes before setting lua tables
		m_selectionWheel->ResetLuaTables();
		m_mapDatabase->ResumeSearching();
		if (g_gameConfig.GetBool(GameConfigKeys::AutoResetSettings))
		{
			g_gameConfig.SetEnum<Enum_SpeedMods>(GameConfigKeys::SpeedMod, SpeedMods::XMod);
			g_gameConfig.Set(GameConfigKeys::ModSpeed, g_gameConfig.GetFloat(GameConfigKeys::AutoResetToSpeed));
			m_filterSelection->SetFiltersByIndex(0, 0);
		}
		if (m_lastMapIndex != -1)
		{
			m_selectionWheel->SelectMapByMapIndex(m_lastMapIndex);
		}
		if (ChartIndex* chartIndex = m_selectionWheel->GetSelectedChart())
		{
			m_mapDatabase->UpdateChartOffset(chartIndex);
		}
	}

	void MakeMultiplayer(MultiplayerScreen *multiplayer)
	{
		m_multiplayer = multiplayer;
	}

	virtual ChartIndex* GetCurrentSelectedChart() override
	{
		return m_selectionWheel->GetSelectedChart();
	}

private:
	void StartDemo() {
		ChartIndex* chart = m_mapDatabase->GetRandomChart();
		PlaybackOptions opts;
		Game* game = Game::Create(chart, opts);
		if (!game)
		{
			Log("Failed to start game", Logger::Severity::Error);
			return;
		}
		game->GetScoring().autoplayInfo.autoplay = true;
		game->SetDemoMode(true);
		game->SetSongDB(m_mapDatabase);
		m_suspended = true;

		// Transition to game
		g_transition->TransitionTo(game);
	}

	void OpenEditor() {
		String paramFormat = g_gameConfig.GetString(GameConfigKeys::EditorParamsFormat);
		String path = Path::Normalize(g_gameConfig.GetString(GameConfigKeys::EditorPath));
		String param = Utility::Sprintf(paramFormat.c_str(),
			Utility::Sprintf("\"%s\"", Path::Absolute(GetCurrentSelectedChart()->path)));
		Path::Run(path, param.GetData());
	}

};

SongSelect *SongSelect::Create(MultiplayerScreen *multiplayer)
{
	SongSelect_Impl *impl = new SongSelect_Impl();
	impl->MakeMultiplayer(multiplayer);
	return impl;
}

SongSelect *SongSelect::Create()
{
	SongSelect_Impl *impl = new SongSelect_Impl();
	return impl;
}
