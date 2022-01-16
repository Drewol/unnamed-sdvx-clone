#include "stdafx.h"
#include "ChallengeSelect.hpp"
#include "ChallengeResult.hpp"
#include "TitleScreen.hpp"
#include "Application.hpp"
#include <Shared/Profiling.hpp>
#include "Scoring.hpp"
#include "Input.hpp"
#include "Game.hpp"
#include "TransitionScreen.hpp"
#include "GameConfig.hpp"
#include "GameplaySettingsDialog.hpp"
#include "lua.hpp"
#include <iterator>
#include <mutex>
#include <unordered_set>
#include "SongSort.hpp"
#include "DBUpdateScreen.hpp"
#include "PreviewPlayer.hpp"
#include "SongFilter.hpp"
#include "ItemSelectionWheel.hpp"
#include <Audio/Audio.hpp>
#include "Gauge.hpp"
#include "Search.hpp"


using ChalItemSelectionWheel = ItemSelectionWheel<ChallengeSelectIndex, ChallengeIndex>;
class ChallengeSelectionWheel : public ChalItemSelectionWheel
{
protected:
	friend class ChallengeSelect_Impl;
	float m_scrollAmount = 0.0;
	LuaBindable* m_bindable = nullptr;
public:
	ChallengeSelectionWheel(IApplicationTickable* owner) : ChalItemSelectionWheel(owner)
	{
	}

	bool Init() override
	{
		ChalItemSelectionWheel::Init();
		m_lua = g_application->LoadScript("songselect/chalwheel");
		if (!m_lua)
			return false;

		m_bindable = new LuaBindable(m_lua, "Challenge");
		m_bindable->AddFunction("GetJSON", this, &ChallengeSelectionWheel::LGetCurrentChallengeJSON);
		m_bindable->Push();
		lua_settop(m_lua, 0);

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
		lua_setglobal(m_lua, "chalwheel");
		return true;
	}

	int LGetCurrentChallengeJSON(lua_State* L)
	{
		ChallengeIndex* chal = this->GetSelection();
		if (chal == nullptr)
			TCPSocket::PushJsonValue(L, "{}"_json);
		else
			TCPSocket::PushJsonValue(L, chal->GetSettings());
		return 1;
	}

	void ReloadScript() override
	{
		g_application->ReloadScript("songselect/chalwheel", m_lua);
		m_SetLuaItemIndex();
	}

	void m_SetLuaItemIndex() override
	{
		if (m_lua == nullptr)
			return;
		lua_getglobal(m_lua, "set_index");
		lua_pushinteger(m_lua, (uint64)m_currentlySelectedLuaSortIndex + 1);
		lua_pushnumber(m_lua, m_scrollAmount);
		if (lua_pcall(m_lua, 2, 0, 0) != 0)
		{
			Logf("Lua error on set_index: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on set_index", lua_tostring(m_lua, -1), 0);
			assert(false);
		}
	}

	void Render(float deltaTime) override
	{
		m_lock.lock();
		lua_getglobal(m_lua, "chalwheel");
		lua_pushstring(m_lua, "searchStatus");
		lua_pushstring(m_lua, *m_lastStatus);
		lua_settable(m_lua, -3);
		lua_setglobal(m_lua, "chalwheel");
		m_lock.unlock();

		lua_getglobal(m_lua, "render");
		lua_pushnumber(m_lua, deltaTime);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
			g_application->RemoveTickable(m_owner);
		}
	}

	~ChallengeSelectionWheel()
	{
		if (m_bindable != nullptr)
			delete m_bindable;
		g_gameConfig.Set(GameConfigKeys::LastSelectedChal, m_currentlySelectedItemId);
		if (m_lua)
			g_application->DisposeLua(m_lua);
	}

	void ResetLuaTables()
	{
		const SortType sort = GetSortType();
		if (sort == SortType::SCORE_ASC || sort == SortType::SCORE_DESC)
			m_doSort();

		m_SetAllItems(); //for force calculation
		m_SetCurrentItems(); //for displaying the correct chal
	}

	void SelectItemBySortIndex(uint32 sortIndex) override
	{
		// TODO(itszn) is there a better place to reset this at?
		m_scrollAmount = 0.0;
		ChalItemSelectionWheel::SelectItemBySortIndex(sortIndex);
	}

	void SetSearchFieldLua(Ref<TextInput> search) override
	{
		if (m_lua == nullptr)
			return;
		lua_getglobal(m_lua, "chalwheel");
		//text
		lua_pushstring(m_lua, "searchText");
		lua_pushstring(m_lua, (search->input).c_str());
		lua_settable(m_lua, -3);
		//enabled
		lua_pushstring(m_lua, "searchInputActive");
		lua_pushboolean(m_lua, search->active);
		lua_settable(m_lua, -3);
		//set global
		lua_setglobal(m_lua, "chalwheel");
	}

	// The delegate templater is unhappy if we use the super class function directly
	void OnSearchStatusUpdated(String status) override
	{
		ChalItemSelectionWheel::OnSearchStatusUpdated(status);
	}

	void OnItemsAdded(Vector<ChallengeIndex*> items) override
	{
		ChalItemSelectionWheel::OnItemsAdded(items);
	}

	void OnItemsRemoved(Vector<ChallengeIndex*> items) override
	{
		ChalItemSelectionWheel::OnItemsRemoved(items);
	}

	void OnItemsUpdated(Vector<ChallengeIndex*> items) override
	{
		ChalItemSelectionWheel::OnItemsUpdated(items);
	}

	void OnItemsCleared(Map<int32, ChallengeIndex*> newList) override
	{
		ChalItemSelectionWheel::OnItemsCleared(newList);
	}

private:
	// grab the actual ChallengeIndex from a given selection
	ChallengeIndex* m_getDBEntryFromItemIndex(const ChallengeSelectIndex ind) const {
		return ind.GetChallenge();
	}
	ChallengeIndex* m_getDBEntryFromItemIndex(const ChallengeSelectIndex* ind) const {
		return ind->GetChallenge();
	}

	// Set all songs into lua
	void m_SetAllItems() override
	{
		//set all songs
		m_SetLuaChals("allChallenges", m_items, false);
		lua_getglobal(m_lua, "challenges_changed");
		if (!lua_isfunction(m_lua, -1))
		{
			lua_pop(m_lua, 1);
			return;
		}
		lua_pushboolean(m_lua, true);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on songs_chaged: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error challenges_changed", lua_tostring(m_lua, -1), 0);
		}
	}

	void m_SetCurrentItems() override
	{
		m_SetLuaChals("challenges", m_SourceCollection(), true);

		lua_getglobal(m_lua, "challenges_changed");
		if (!lua_isfunction(m_lua, -1))
		{
			lua_pop(m_lua, 1);
			return;
		}
		lua_pushboolean(m_lua, false);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on songs_chaged: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error challenges_changed", lua_tostring(m_lua, -1), 0);
		}
	}

	void m_SetLuaChals(const char* key, const Map < int32, ChallengeSelectIndex> &collection, bool sorted)
	{
		lua_getglobal(m_lua, "chalwheel");
		lua_pushstring(m_lua, key);
		lua_newtable(m_lua);
		int chalIndex = 0;

		if (sorted)
		{
			// sortVec should only have the current chals in the collection
			for (auto& chalId : m_sortVec)
			{
				// Grab the chal for this sort index
				ChallengeSelectIndex chal = collection.find(chalId)->second;
				m_PushChalToLua(chal, ++chalIndex);
			}
		}
		else {

			for (auto& chal : collection)
			{
				m_PushChalToLua(chal.second, ++chalIndex);
			}
		}
		lua_settable(m_lua, -3);
		lua_setglobal(m_lua, "chalwheel");
	}

	void m_PushChalToLua(ChallengeSelectIndex chalsel, int index)
	{
		lua_pushinteger(m_lua, index);
		lua_newtable(m_lua);
		ChallengeIndex* chal = chalsel.GetChallenge();
		m_PushStringToTable("title", *(chal->title));
		m_PushStringToTable("requirement_text", *(chal->reqText));
		m_PushIntToTable("id", chal->id);
		m_PushIntToTable("topBadge", chal->clearMark);
		m_PushIntToTable("bestScore", chal->bestScore);
		m_PushStringToTable("grade", ToDisplayString(ToGradeMark(chal->bestScore)));
		lua_pushstring(m_lua, "missing_chart");
		lua_pushboolean(m_lua, chal->missingChart);
		lua_settable(m_lua, -3);

		int chartIndex = 0;
		lua_pushstring(m_lua, "charts");
		lua_newtable(m_lua);
		for (auto& diff : chal->charts)
		{

			String folder_path = Path::RemoveLast(diff->path, nullptr);
			lua_pushinteger(m_lua, ++chartIndex);
			lua_newtable(m_lua);
			m_PushStringToTable("title", diff->title.c_str());
			m_PushStringToTable("artist", diff->artist.c_str());
			m_PushStringToTable("bpm", diff->bpm.c_str());
			m_PushStringToTable("jacketPath", Path::Normalize(folder_path + "/" + diff->jacket_path).c_str());
			Logf("Jacket path: %s", Logger::Severity::Info, Path::Normalize(folder_path + "/" + diff->jacket_path).c_str());

			m_PushIntToTable("level", diff->level);
			m_PushIntToTable("difficulty", diff->diff_index);
			m_PushIntToTable("id", diff->id);
			m_PushStringToTable("effector", diff->effector.c_str());
			m_PushStringToTable("illustrator", diff->illustrator.c_str());
			lua_settable(m_lua, -3);
		}
		lua_settable(m_lua, -3);

		lua_settable(m_lua, -3);
	}

	void m_OnItemSelected(ChallengeSelectIndex index) override
	{
		m_currentlySelectedItemId = index.GetChallenge()->id;
	}
};

/*
	Filter selection element
*/
class ChallengeFilterSelection
{
public:
	ChallengeFilterSelection(Ref<ChallengeSelectionWheel> selectionWheel) : m_selectionWheel(selectionWheel)
	{
	}

	bool Init()
	{
		ChallengeFilter *lvFilter = new ChallengeFilter();
		ChallengeFilter* flFilter = new ChallengeFilter();

		AddFilter(lvFilter, FilterType::Level);
		AddFilter(flFilter, FilterType::Folder);
		for (size_t i = 1; i <= 12; i++)
		{
			AddFilter(new ChallengeLevelFilter(i), FilterType::Level);
		}
		CheckedLoad(m_lua = g_application->LoadScript("songselect/filterwheel"));
		if (m_selectingFolders)
			ToggleSelectionMode();
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
	~ChallengeFilterSelection()
	{
		g_gameConfig.Set(GameConfigKeys::LevelFilterChal, m_currentLevelSelection);

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

	void AddFilter(ChallengeFilter *filter, FilterType type)
	{
		if (type == FilterType::Level)
			m_levelFilters.Add(filter);
		else
			m_folderFilters.Add(filter);
	}

	void SelectFilter(ChallengeFilter *filter, FilterType type)
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
			Logf("Lua error on set_selection: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on set_selection", lua_tostring(m_lua, -1), 0);
			assert(false);
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
			Logf("Lua error on set_mode: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on set_mode", lua_tostring(m_lua, -1), 0);
			assert(false);
		}
	}
	void OnSongsChanged()
	{
		UpdateFilters();
	}

	// Check if any new folders or collections should be added and add them
	void UpdateFilters()
	{
		/*
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
		*/

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
				Logf("Lua error on tables_set: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
				g_gameWindow->ShowMessageBox("Lua Error on tables_set", lua_tostring(m_lua, -1), 0);
			}
		}
	}

	Ref<ChallengeSelectionWheel> m_selectionWheel;
	Vector<ChallengeFilter *> m_folderFilters;
	Vector<ChallengeFilter *> m_levelFilters;
	int32 m_currentFolderSelection = 0;
	int32 m_currentLevelSelection = 0;
	bool m_selectingFolders = true;
	ChallengeFilter *m_currentFilters[2] = {nullptr};
	MapDatabase *m_mapDB;
	lua_State *m_lua = nullptr;
	std::unordered_set<std::string> m_folders;
	std::unordered_set<std::string> m_collections;
};


/*
	Sorting selection element
*/
class ChallengeSortSelection
{
public:
	bool Active = false;
	bool Initialized = false;

	ChallengeSortSelection(Ref<ChallengeSelectionWheel> selectionWheel) : m_selectionWheel(selectionWheel) {}
	~ChallengeSortSelection()
	{
		g_gameConfig.Set(GameConfigKeys::LastSortChal, m_selection);
		for (ChallengeSort *s : m_sorts)
		{
			delete s;
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
			m_sorts.Add(new ChallengeTitleSort("Title ^", false));
			m_sorts.Add(new ChallengeTitleSort("Title v", true));
			m_sorts.Add(new ChallengeScoreSort("Score ^", false));
			m_sorts.Add(new ChallengeScoreSort("Score v", true));
			m_sorts.Add(new ChallengeDateSort("Date ^", false));
			m_sorts.Add(new ChallengeDateSort("Date v", true));
			m_sorts.Add(new ChallengeClearMarkSort("Badge ^", false));
			m_sorts.Add(new ChallengeClearMarkSort("Badge v", true));
		}

		CheckedLoad(m_lua = g_application->LoadScript("songselect/sortwheel"));
		m_SetLuaTable();

		Initialized = true;
		m_selection = g_gameConfig.GetInt(GameConfigKeys::LastSortChal);
		AdvanceSelection(0);
		return true;
	}

	void SetSelection(ChallengeSort *s)
	{
		m_selection = std::find(m_sorts.begin(), m_sorts.end(), s) - m_sorts.begin();
		m_selectionWheel->SetSort(s);

		lua_getglobal(m_lua, "set_selection");
		lua_pushnumber(m_lua, m_selection + 1);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on set_selection: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on set_selection", lua_tostring(m_lua, -1), 0);
			assert(false);
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
				Logf("Lua error on tables_set: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
				g_gameWindow->ShowMessageBox("Lua Error on tables_set", lua_tostring(m_lua, -1), 0);
			}
		}
	}

	Ref<ChallengeSelectionWheel> m_selectionWheel;
	Vector<ChallengeSort *> m_sorts;
	int m_selection = 0;
	lua_State *m_lua = nullptr;
};


class ChallengeSelect_Impl : public ChallengeSelect
{
private:
	Timer m_dbUpdateTimer;
	MapDatabase* m_mapDatabase = nullptr;

	// Map selection wheel
	Ref<ChallengeSelectionWheel> m_selectionWheel;
	Ref<ChallengeSortSelection> m_sortSelection;
	Ref<ChallengeFilterSelection> m_filterSelection;
	Ref<TextInput> m_searchInput;

	float m_advanceChal = 0.0f;
	float m_advanceScroll = 0.0f;
	float m_sensMult = 1.0f;
	MouseLockHandle m_lockMouse;
	bool m_suspended = true;
	bool m_hasRestored = false;
	bool m_previewLoaded = true;
	bool m_showScores = false;



	Map<Input::Button, float> m_timeSinceButtonPressed;
	Map<Input::Button, float> m_timeSinceButtonReleased;
	lua_State* m_lua = nullptr;

	GameplaySettingsDialog m_settDiag;

	// Select sound
	Sample m_selectSound;

	bool m_transitionedToGame = false;
	int32 m_lastChalIndex = -1;

	DBUpdateScreen* m_dbUpdateScreen = nullptr;

	ChallengeManager m_manager;
public:
	ChallengeSelect_Impl() : m_settDiag() {}

	bool AsyncLoad() override
	{
		m_selectSound = g_audio->CreateSample("audio/menu_click.wav");
		m_selectionWheel = Ref<ChallengeSelectionWheel>(new ChallengeSelectionWheel(this));
		m_filterSelection = Ref<ChallengeFilterSelection>(new ChallengeFilterSelection(m_selectionWheel));
		m_mapDatabase = new MapDatabase(true);

		// Add database update hook before triggering potential update
		m_mapDatabase->OnDatabaseUpdateStarted.Add(this, &ChallengeSelect_Impl::m_onDatabaseUpdateStart);
		m_mapDatabase->OnDatabaseUpdateDone.Add(this, &ChallengeSelect_Impl::m_onDatabaseUpdateDone);
		m_mapDatabase->OnDatabaseUpdateProgress.Add(this, &ChallengeSelect_Impl::m_onDatabaseUpdateProgress);
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

	bool AsyncFinalize() override
	{
		CheckedLoad(m_lua = g_application->LoadScript("songselect/background"));

		lua_settop(m_lua, 0);

		g_input.OnButtonPressed.Add(this, &ChallengeSelect_Impl::m_OnButtonPressed);
		g_input.OnButtonReleased.Add(this, &ChallengeSelect_Impl::m_OnButtonReleased);
		g_gameWindow->OnMouseScroll.Add(this, &ChallengeSelect_Impl::m_OnMouseScroll);

		if (!m_selectionWheel->Init())
			return false;
		if (!m_filterSelection->Init())
			return false;
		m_filterSelection->SetMapDB(m_mapDatabase);

		m_mapDatabase->OnChallengesAdded.Add(m_selectionWheel.get(), &ChallengeSelectionWheel::OnItemsAdded);
		m_mapDatabase->OnChallengesUpdated.Add(m_selectionWheel.get(), &ChallengeSelectionWheel::OnItemsUpdated);
		m_mapDatabase->OnChallengesCleared.Add(m_selectionWheel.get(), &ChallengeSelectionWheel::OnItemsCleared);
		m_mapDatabase->OnChallengesRemoved.Add(m_selectionWheel.get(), &ChallengeSelectionWheel::OnItemsRemoved);
		m_mapDatabase->OnSearchStatusUpdated.Add(m_selectionWheel.get(), &ChallengeSelectionWheel::OnSearchStatusUpdated);
		//m_selectionWheel->OnItemsChanged.Add(m_filterSelection.get(), &FilterSelection::OnSongsChanged);
		m_mapDatabase->StartSearching();

		//m_filterSelection->SetFiltersByIndex(g_gameConfig.GetInt(GameConfigKeys::LevelFilterChal), g_gameConfig.GetInt(GameConfigKeys::FolderFilter));
		m_filterSelection->SetFiltersByIndex(g_gameConfig.GetInt(GameConfigKeys::LevelFilterChal), 0);

		m_selectionWheel->SetSort(new ChallengeSort("Title ^", false));
		m_sortSelection = Ref<ChallengeSortSelection>(new ChallengeSortSelection(m_selectionWheel));
		if (!m_sortSelection->Init())
			return false;

		m_selectionWheel->SelectItemByItemId(g_gameConfig.GetInt(GameConfigKeys::LastSelectedChal));

		m_searchInput = Ref<TextInput>(new TextInput());
		m_searchInput->OnTextChanged.Add(this, &ChallengeSelect_Impl::OnSearchTermChanged);

		g_audio->SetGlobalVolume(g_gameConfig.GetFloat(GameConfigKeys::MasterVolume));

		m_sensMult = g_gameConfig.GetFloat(GameConfigKeys::SongSelSensMult);

		if (!m_settDiag.Init())
			return false;

		return true;
	}

	bool Init() override
	{
		return true;
	}

	~ChallengeSelect_Impl()
	{
		// Clear callbacks
		if (m_mapDatabase)
		{
			m_mapDatabase->OnChallengesCleared.Clear();
			delete m_mapDatabase;
		}
		g_input.OnButtonPressed.RemoveAll(this);
		g_input.OnButtonReleased.RemoveAll(this);
		g_gameWindow->OnMouseScroll.RemoveAll(this);

		if (m_lua)
			g_application->DisposeLua(m_lua);
	}

	void OnSearchTermChanged(const String &search)
	{
		//m_filterSelection->AdvanceSelection(0);
		if (search.empty())
		{
			m_filterSelection->AdvanceSelection(0);
		}
		else
		{
			Map<int32, ChallengeIndex *> filter = m_mapDatabase->FindChallenges(search);
			m_selectionWheel->SetFilter(filter);
		}
	}

	void m_OnButtonPressed(Input::Button buttonCode, int32 delta)
	{
		//if (m_multiplayer && m_multiplayer->GetChatOverlay()->IsOpen())
		if (m_suspended || m_settDiag.IsActive())
			return;

		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Keyboard && m_searchInput->active)
			return;

		m_timeSinceButtonPressed[buttonCode] = 0;

		if (buttonCode == Input::Button::BT_S && !m_filterSelection->Active && !m_sortSelection->Active && !IsSuspended() && !m_transitionedToGame)
		{
			ChallengeIndex *folder = m_selectionWheel->GetSelection();
			if (folder)
			{
				// TODO start the chal logic
				ChallengeIndex* chal = GetCurrentSelectedChallenge();
				if (m_manager.StartChallenge(this, chal))
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

		if (m_filterSelection->Active)
		{
			switch (buttonCode) {
				case Input::Button::SongSelect_Down:
					m_filterSelection->AdvanceSelection(1);
					break;
				case Input::Button::SongSelect_Up:
					m_filterSelection->AdvanceSelection(-1);
					break;
			}
		}
		else if (m_sortSelection->Active)
		{
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
				case Input::Button::ScrollLeft:
					m_selectionWheel->m_scrollAmount -= 1.0;
					m_selectionWheel->m_SetLuaItemIndex();
					break;
				case Input::Button::ScrollRight:
					m_selectionWheel->m_scrollAmount += 1.0;
					m_selectionWheel->m_SetLuaItemIndex();
					break;
				case Input::Button::SongSelect_Random:
					m_selectionWheel->SelectRandom();
					break;
				case Input::Button::SongSelect_ReloadSongs:
					m_mapDatabase->StartSearching();
					OnSearchTermChanged(m_searchInput->input);
					break;
				case Input::Button::ReloadSkin:
					m_selectionWheel->ReloadScript();
					m_filterSelection->ReloadScript();
					g_application->ReloadScript("songselect/background", m_lua);
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
			}
		}
	}

	void m_OnButtonReleased(Input::Button buttonCode, int32 delta)
	{
		//if (m_multiplayer && m_multiplayer->GetChatOverlay()->IsOpen())
		if (m_suspended || m_settDiag.IsActive())
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
		if (m_suspended || m_settDiag.IsActive())
			return;

		//if (m_multiplayer && m_multiplayer->GetChatOverlay()->IsOpen())

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
		//if (m_multiplayer && m_multiplayer->GetChatOverlay()->OnKeyPressedConsume(code))

		if (m_settDiag.IsActive())
			return;

		if (m_filterSelection->Active)
		{
		}
		else if (m_sortSelection->Active)
		{
		}
		else
		{
			if (code == SDL_SCANCODE_DELETE)
			{
				ChallengeIndex* chal = m_selectionWheel->GetSelection();
				String name = chal->title;

				bool res = g_gameWindow->ShowYesNoMessage("Delete challenge?", "Are you sure you want to delete " + name + "\nThis will only delete "+chal->path+"\nThis cannot be undone...");
				if (!res)
					return;
				Path::Delete(chal->path);
				m_mapDatabase->StartSearching();
				OnSearchTermChanged(m_searchInput->input);
			}
		}
	}

	void OnKeyReleased(SDL_Scancode code, int32 delta) override
	{
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
			m_searchInput->Tick();
			m_selectionWheel->SetSearchFieldLua(m_searchInput);
			m_settDiag.Tick(deltaTime);
		}
		//if (m_multiplayer)
		//	m_multiplayer->GetChatOverlay()->Tick(deltaTime);
	}

	void Render(float deltaTime) override
	{
		if (m_suspended && m_hasRestored) return;

		if (m_manager.RunningChallenge())
			return;

		lua_getglobal(m_lua, "render");
		lua_pushnumber(m_lua, deltaTime);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			g_application->ScriptError("songselect/background", m_lua);
		}

		m_selectionWheel->Render(deltaTime);
		m_filterSelection->Render(deltaTime);
		m_sortSelection->Render(deltaTime);

		m_settDiag.Render(deltaTime);

		//if (m_multiplayer)
		//	m_multiplayer->GetChatOverlay()->Render(deltaTime);
	}

	void TickNavigation(float deltaTime)
	{
		if (!m_manager.ReturnToSelect())
		{
			return;
		}

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
		float scroll_input = g_input.GetInputLaserDir(0) * m_sensMult;
		float song_input = g_input.GetInputLaserDir(1) * m_sensMult;

		m_advanceScroll += scroll_input;
		m_advanceChal += song_input;

		int advanceScrollActual = (int)Math::Floor(m_advanceScroll * Math::Sign(m_advanceScroll)) * Math::Sign(m_advanceScroll);
		int advanceChalActual = (int)Math::Floor(m_advanceChal * Math::Sign(m_advanceChal)) * Math::Sign(m_advanceChal);


		if (m_filterSelection->Active)
		{
			if (advanceScrollActual != 0)
				m_filterSelection->AdvanceSelection(advanceScrollActual);
			if (advanceChalActual != 0)
				m_filterSelection->AdvanceSelection(advanceChalActual);
		}
		else if (m_sortSelection->Active)
		{
			if (advanceScrollActual != 0)
				m_sortSelection->AdvanceSelection(advanceScrollActual);
			if (advanceChalActual != 0)
				m_sortSelection->AdvanceSelection(advanceChalActual);
		}
		else
		{
			if (m_advanceScroll != 0)
			{
				m_selectionWheel->m_scrollAmount += m_advanceScroll;
				m_selectionWheel->m_SetLuaItemIndex();
				advanceScrollActual = 0;
				m_advanceScroll = 0;
			}
			if (advanceChalActual != 0)
				m_selectionWheel->AdvanceSelection(advanceChalActual);
		}

		m_advanceScroll -= advanceScrollActual;
		m_advanceChal -= advanceChalActual;
	}

	void OnSuspend() override
	{
		m_lastChalIndex = m_selectionWheel->GetCurrentItemIndex();

		m_suspended = true;
		//m_previewPlayer.Pause();
		m_mapDatabase->PauseSearching();
		if (m_lockMouse)
			m_lockMouse.reset();
	}

	void OnRestore() override
	{
		// NOTE: we can't trigger the next chart here bc you can't add tickables on restore
		g_application->DiscordPresenceMenu("Challenge Select");
		m_suspended = false;
		m_hasRestored = true;
		m_transitionedToGame = false;
		//m_previewPlayer.Restore();
		m_selectionWheel->ResetLuaTables();
		//TODO if the manager is going to trigger in the next tick we probably should not do this
		//     we could add a delegate for finishing the charts and then use that to restart searching
		m_mapDatabase->ResumeSearching();
		if (g_gameConfig.GetBool(GameConfigKeys::AutoResetSettings))
		{
			g_gameConfig.SetEnum<Enum_SpeedMods>(GameConfigKeys::SpeedMod, SpeedMods::XMod);
			g_gameConfig.Set(GameConfigKeys::ModSpeed, g_gameConfig.GetFloat(GameConfigKeys::AutoResetToSpeed));
			m_filterSelection->SetFiltersByIndex(0, 0);
		}
		if (m_lastChalIndex != -1)
		{
			m_selectionWheel->SelectItemByItemIndex(m_lastChalIndex);
		}

	}

	ChallengeIndex* GetCurrentSelectedChallenge() override
	{
		return m_selectionWheel->GetSelection();
	}

	MapDatabase* GetMapDatabase() override
	{
		return m_mapDatabase;
	}

};

ChallengeSelect* ChallengeSelect::Create()
{
	ChallengeSelect_Impl* impl = new ChallengeSelect_Impl();
	return impl;
}

ChallengeOption<bool> ChallengeManager::m_getOptionAsBool(
	nlohmann::json reqs, String name)
{
	if (!reqs.contains(name))
		return ChallengeOption<bool>::IgnoreOption();

	nlohmann::json entry = reqs[name];
	bool val;
	if (entry.is_null())
	{
		return ChallengeOption<bool>::DisableOption();
	}
	if (!entry.is_boolean())
	{
		Logf("Skipping setting '%s': not a boolean", Logger::Severity::Warning, *name);
		return ChallengeOption<bool>::IgnoreOption();
	}

	entry.get_to(val);
	return ChallengeOption<bool>(val);
}

ChallengeOption<float> ChallengeManager::m_getOptionAsFloat(
	nlohmann::json reqs, String name, float min, float max)
{
	if (!reqs.contains(name))
		return ChallengeOption<float>::IgnoreOption();

	nlohmann::json entry = reqs[name];
	float val;
	if (entry.is_null())
	{
		return ChallengeOption<float>::DisableOption();
	}
	if (entry.is_number_float())
	{
		entry.get_to(val);
	}
	if (!entry.is_number_float() || isnan(val))
	{
		Logf("Skipping setting '%s': not a number", Logger::Severity::Warning, *name);
		return ChallengeOption<float>::IgnoreOption();
	}
	if (val < min || val > max)
	{
		Logf("Skipping setting '%s': must be between [%f and %f]", Logger::Severity::Warning, *name, min, max);
		return ChallengeOption<float>::IgnoreOption();
	}

	return ChallengeOption<float>(val);
}

ChallengeOption<uint32> ChallengeManager::m_getOptionAsPositiveInteger(
	nlohmann::json reqs, String name, int64 min, int64 max)
{
	if (!reqs.contains(name))
		return ChallengeOption<uint32>::IgnoreOption();

	nlohmann::json entry = reqs[name];
	int32 val;
	if (entry.is_null())
	{
		return ChallengeOption<uint32>::DisableOption();

	}
	if (entry.is_string())
	{
		// Get the intergral from the string
		String str;
		entry.get_to(str);
		// XXX should we actually like validate the string?
		val = atoi(*str);
	}
	else if (entry.is_number_integer())
	{
		entry.get_to(val);
	}
	else
	{
		Logf("Skipping setting '%s': not a integer or string", Logger::Severity::Warning, *name);
		return ChallengeOption<uint32>::IgnoreOption();
	}
	if (val < min || val > max)
	{
		Logf("Skipping setting '%s': must be between [%u and %u]", Logger::Severity::Warning, *name, min, max);
		return ChallengeOption<uint32>::IgnoreOption();
	}

	return ChallengeOption<uint32>(val);
}

ChallengeRequirements ChallengeManager::m_processReqs(nlohmann::json req)
{
	ChallengeRequirements out;
	out.clear =          m_getOptionAsBool(req,            "clear");
	out.min_percentage = m_getOptionAsPositiveInteger(req, "min_percentage", 0,   200);
	out.min_gauge =      m_getOptionAsFloat(req,           "min_gauge",      0.0, 1.0);
	out.max_errors =     m_getOptionAsPositiveInteger(req, "max_errors");
	out.max_nears =      m_getOptionAsPositiveInteger(req, "max_nears");
	out.min_crits =      m_getOptionAsPositiveInteger(req, "min_crits");
	out.min_chain =      m_getOptionAsPositiveInteger(req, "min_chain");
	return out;
}

ChallengeOptions ChallengeManager::m_processOptions(nlohmann::json j)
{
	ChallengeOptions out;
	out.mirror =       m_getOptionAsBool(j,            "mirror");
	out.excessive =    m_getOptionAsBool(j,            "excessive_gauge");
	out.permissive  =    m_getOptionAsBool(j,          "permissive_gauge");
	out.blastive =    m_getOptionAsBool(j,             "blastive_gauge");
	out.gauge_level = m_getOptionAsFloat(j,            "gauge_level",    0.0, 10.0);
	out.ars       =    m_getOptionAsBool(j,            "ars");
	out.gauge_carry_over =    m_getOptionAsBool(j,	   "gauge_carry_over");
	out.min_modspeed = m_getOptionAsPositiveInteger(j, "min_modspeed");
	out.max_modspeed = m_getOptionAsPositiveInteger(j, "max_modspeed");
	out.allow_cmod =   m_getOptionAsBool(j,            "allow_cmod");
	out.allow_excessive =   m_getOptionAsBool(j,       "allow_excessive");
	out.allow_permissive =   m_getOptionAsBool(j,      "allow_blastive");
	out.allow_blastive =   m_getOptionAsBool(j,        "allow_permissive");
	out.allow_ars =    m_getOptionAsBool(j,            "allow_ars");
	out.hidden_min =   m_getOptionAsFloat(j,           "hidden_min",     0.0, 1.0);
	out.sudden_min =   m_getOptionAsFloat(j,           "sudden_min",     0.0, 1.0);
	out.crit_judge =   m_getOptionAsPositiveInteger(j, "crit_judgement", 0,   46);
	out.near_judge =   m_getOptionAsPositiveInteger(j, "near_judgement", 0,   92);
	out.hold_judge =   m_getOptionAsPositiveInteger(j, "hold_judgement", 0,   138);

	// These reqs are checked at the end so we keep em as options
	// TODO only do this on the global one? (overrides don't work for these)
	out.average_percentage = m_getOptionAsPositiveInteger(j, "min_average_percentage", 0, 200);
	out.average_gauge =      m_getOptionAsFloat(j,           "min_average_gauge",      0, 1.0);
	out.average_errors =     m_getOptionAsPositiveInteger(j, "max_average_errors");
	out.max_overall_errors = m_getOptionAsPositiveInteger(j, "max_overall_errors");
	out.average_nears =      m_getOptionAsPositiveInteger(j, "max_average_nears");
	out.max_overall_nears =  m_getOptionAsPositiveInteger(j, "max_overall_nears");
	out.average_crits =      m_getOptionAsPositiveInteger(j, "min_average_crits");
	out.min_overall_crits =  m_getOptionAsPositiveInteger(j, "min_overall_crits");

	out.use_sdvx_complete_percentage = m_getOptionAsBool(j, "use_sdvx_complete_percentage");
	return out;
}

bool ChallengeManager::StartChallenge(ChallengeSelect* sel, ChallengeIndex* chal)
{
	assert(!m_running);

	if (chal->missingChart)
		return false;

	m_challengeSelect = sel;

	// Force reload from file in case it was changed
	chal->ReloadSettings();

	// Check if there are valid settings
	nlohmann::json settings = chal->GetSettings();

	// Check if the json loaded correctly
	if (settings.is_null() || settings.is_discarded())
		return false;

	m_chal = chal;
	m_running = true;
	m_finishedCurrentChart = false;
	m_failedEarly = false;
	m_seenResults = false;
	m_scores.clear();
	m_reqs.clear();
	m_opts.clear();
	m_results.clear();
	m_chartIndex = 0;
	m_chartsPlayed = 0;

	m_totalNears = 0;
	m_totalErrors = 0;
	m_totalCrits = 0;
	m_totalScore = 0;
	m_totalPercentage = 0;
	m_totalGauge = 0.0;
	m_lastGauges.clear();

	m_globalReqs.Reset();
	m_globalOpts.Reset();
	if (settings.contains("global"))
	{
		m_globalReqs = m_processReqs(settings["global"]);
		m_globalOpts = m_processOptions(settings["global"]);
	}


	if (settings.contains("overrides"))
	{
		nlohmann::json overrides = settings["overrides"];

		if (m_chal->charts.size() < overrides.size())
		{
			Log("Note: more overrides than charts", Logger::Severity::Warning);
		}
		for (unsigned int i = 0; i < overrides.size() && i < m_chal->charts.size(); i++)
		{
			m_reqs.push_back(m_processReqs(overrides[i]));
			m_opts.push_back(m_processOptions(overrides[i]));
		}
	}
	for (unsigned int i=m_reqs.size(); i<m_chal->charts.size(); i++)
	{
		m_reqs.push_back(ChallengeRequirements());
		m_opts.push_back(ChallengeOptions());
	}

	return m_setupNextChart();
}

bool ChallengeManager::m_finishedAllCharts(bool passed)
{
	assert(m_chartsPlayed > 0);

	OverallChallengeResult res;

	if (!passed)
	{
		res.failString = "Not All Charts Passed Challenge Goals";
	}

	uint32 numCharts = m_chal->totalNumCharts;

	res.averageScore = m_totalScore / numCharts;

	res.averagePercent = static_cast<uint32>(m_totalPercentage / numCharts);
	if (passed && res.averagePercent < m_globalOpts.average_percentage.Get(0))
	{
		passed = false;
		res.failString = Utility::Sprintf("Average Percentage of %u%% < Min of %u%%",
			res.averagePercent, m_globalOpts.average_percentage.Get(0));
	}
	res.averageGauge = m_totalGauge / numCharts;
	if (passed && res.averageGauge < m_globalOpts.average_gauge.Get(0.0))
	{
		res.failString = Utility::Sprintf("Average Guage of %d%% < Min of %d%%",
			(int)(res.averageGauge * 100), (int)(m_globalOpts.average_gauge.Get(0) * 100));
		passed = false;
	}

	res.overallErrors = m_totalErrors;
	if (passed && res.overallErrors > m_globalOpts.max_overall_errors.Get(INT_MAX))
	{
		res.failString = Utility::Sprintf("Total of %u Error > Max of %u",
			res.overallErrors, m_globalOpts.max_overall_errors.Get(INT_MAX));
		passed = false;
	}
	res.averageErrors = m_totalErrors / numCharts;
	if (passed && res.averageErrors > m_globalOpts.average_errors.Get(INT_MAX))
	{
		res.failString = Utility::Sprintf("Average of %u Error > Max of %u",
			res.averageErrors, m_globalOpts.average_errors.Get(INT_MAX));
		passed = false;
	}

	res.overallNears = m_totalNears;
	if (passed && res.overallNears > m_globalOpts.max_overall_nears.Get(INT_MAX))
	{
		res.failString = Utility::Sprintf("Total of %u Good > Max of %u",
			res.overallNears, m_globalOpts.max_overall_nears.Get(INT_MAX));
		passed = false;
	}
	res.averageNears = m_totalNears / numCharts;
	if (passed && res.averageNears > m_globalOpts.average_nears.Get(INT_MAX))
	{
		res.failString = Utility::Sprintf("Average of %u Good > Max of %u",
			res.averageNears, m_globalOpts.average_nears.Get(INT_MAX));
		passed = false;
	}

	res.overallCrits = m_totalCrits;
	if (passed && res.overallCrits < m_globalOpts.min_overall_crits.Get(0))
	{
		res.failString = Utility::Sprintf("Total of %u Crit < Min of %u",
			res.overallCrits, m_globalOpts.min_overall_crits.Get(INT_MAX));
		passed = false;
	}
	res.averageCrits = m_totalCrits / numCharts;
	if (passed && res.averageCrits < m_globalOpts.average_crits.Get(0))
	{
		res.failString = Utility::Sprintf("Average of %u Crit < Min of %u",
			res.averageNears, m_globalOpts.average_crits.Get(INT_MAX));
		passed = false;
	}

	res.passed = passed;

	m_overallResults = res;

	ClearMark clearMark = ClearMark::Played;
	if (res.passed)
	{
		clearMark = ClearMark::NormalClear;
		if (res.overallErrors == 0)
			clearMark = ClearMark::FullCombo;
		if (res.overallErrors == 0 && res.overallNears == 0)
			clearMark = ClearMark::Perfect;
	}

	res.clearMark = clearMark;

	int32 bestClear = static_cast<int>(clearMark);

	bool goodScore = false;

	if (bestClear < m_chal->clearMark)
		bestClear = m_chal->clearMark;
	else
		goodScore = true;

	uint32 bestScore = res.averageScore;

	if (m_chal->bestScore > 0 && bestScore < (uint32)m_chal->bestScore)
		bestScore = m_chal->bestScore;
	else
		goodScore = true;

	m_challengeSelect->GetMapDatabase()->UpdateChallengeResult(m_chal, bestClear, bestScore);

	m_overallResults.goodScore = goodScore;

	ChallengeResultScreen* resScreen = ChallengeResultScreen::Create(this);
	g_transition->TransitionTo(resScreen);

	m_seenResults = true;
	return false;
}

// Returns if it is all done or not
bool ChallengeManager::ReturnToSelect()
{
	if (!m_running)
		return true;

	if (!m_finishedCurrentChart)
		return false;

	if (m_seenResults)
	{
		// We all all done here
		m_running = false;
		m_chal = nullptr;
		m_currentChart = nullptr;
		return true;
	}

	m_chartsPlayed++;

	if (!m_passedCurrentChart || m_failedEarly)
	{
		return m_finishedAllCharts(m_passedCurrentChart);
	}

	m_chartIndex++;
	if (m_chartIndex == m_chal->charts.size())
	{
		return m_finishedAllCharts(true);
	}

	return m_setupNextChart();
}

bool ChallengeManager::m_setupNextChart()
{
	assert(m_running);

	m_passedCurrentChart = false;
	m_finishedCurrentChart = false;
	m_currentChart = m_chal->charts[m_chartIndex];
	m_currentOptions = m_globalOpts.Merge(m_opts[m_chartIndex]);

	if (m_currentOptions.min_modspeed.Get(0) >
		m_currentOptions.max_modspeed.Get(INT_MAX))
	{
		Log("Skipping setting 'max_modspeed': must be more than 'min_modspeed'", Logger::Severity::Warning);
		m_currentOptions.max_modspeed = ChallengeOption<uint32>::IgnoreOption();
	}

	PlaybackOptions opts;

	// By default use gauge settings from user
	GaugeTypes gaugeType = g_gameConfig.GetEnum<Enum_GaugeTypes>(GameConfigKeys::GaugeType);
	if (gaugeType == GaugeTypes::Hard && m_currentOptions.allow_excessive.Get(true))
		opts.gaugeType = GaugeType::Hard;
	if (gaugeType == GaugeTypes::Permissive && m_currentOptions.allow_permissive.Get(false))
		opts.gaugeType = GaugeType::Permissive;
	if (gaugeType == GaugeTypes::Blastive && m_currentOptions.allow_permissive.Get(false))
	{
		opts.gaugeType = GaugeType::Blastive;
		opts.gaugeLevel = (float)g_gameConfig.GetInt(GameConfigKeys::BlastiveLevel) / 2.0f;
	}

	opts.backupGauge = g_gameConfig.GetBool(GameConfigKeys::BackupGauge) && m_currentOptions.allow_ars.Get(true);

	// Check if we have overrides
	if (m_currentOptions.blastive.Get(false))
	{
		opts.gaugeType = GaugeType::Blastive;
		opts.gaugeLevel = m_currentOptions.gauge_level.Get(0.5);
	}
	else if (m_currentOptions.permissive.Get(false))
	{
		opts.gaugeType = GaugeType::Permissive;
	}
	else if (m_currentOptions.excessive.Get(false) || m_currentOptions.ars.Get(false))
	{
		opts.gaugeType = GaugeType::Hard;
	}
	if (m_currentOptions.ars.Get(false))
		opts.backupGauge = true;

	// atm there are not modifiers for mirror mode
	opts.mirror = m_currentOptions.mirror.Get(false);

	Game* game = Game::Create(this, m_currentChart, opts);
	if (!game)
	{
		Log("Failed to start game", Logger::Severity::Error);
		return false;
	}

	if (m_currentOptions.gauge_carry_over.Get(false) && m_lastGauges.size() > 0)
	{
		auto setGauge = [=](void *screen) {
			if (screen == nullptr)
				return;
			game->SetAllGaugeValues(this->m_lastGauges);
		};
		auto handle = g_transition->OnLoadingComplete.AddLambda(std::move(setGauge));
		g_transition->RemoveOnComplete(handle);
	}

	g_transition->TransitionTo(game);
	return true;
}

void ChallengeManager::ReportScore(Game* game, ClearMark clearMark)
{
	assert(m_running);

	if (clearMark == ClearMark::NotPlayed)
	{

	}

	m_finishedCurrentChart = true;
	const Scoring& scoring = game->GetScoring();

	ChallengeRequirements req = m_globalReqs.Merge(m_reqs[m_chartIndex]);
	ChallengeResult res;

	uint32 finalScore = scoring.CalculateCurrentScore();
	res.score = finalScore;
	res.opts = game->GetPlaybackOptions();
	float percentage = std::max((finalScore - 8000000.0f) / 10000.0f, 0.0f);

	if (m_globalOpts.use_sdvx_complete_percentage.Get(false))
	{
		// In skill analyzer mode any clear is 100%
		if (percentage < 100.0 && clearMark >= ClearMark::NormalClear)
		{
			percentage = 100.0;
		}
		else if (clearMark < ClearMark::NormalClear)
		{
			bool canFail = game->GetPlaybackOptions().gaugeType == GaugeType::Hard;
			if (canFail)
			{
				// If we failed part way though we can use our distance as our percentage
				// we can use the max possible score at crash to tell how far we got
				percentage = game->GetScoring().currentMaxScore / 100000.0f;
			}
			else
			{
				// In the case where we failed effective rate, make a PUC be 100%
				percentage = finalScore / 100000.0f;
			}
		}
	}
	res.percent = percentage;


	res.crits = scoring.GetPerfects();
	m_totalCrits += res.crits;
	res.nears = scoring.GetGoods();
	m_totalNears += res.nears;
	res.errors = scoring.GetMisses();
	m_totalErrors += res.errors;

	m_totalScore += finalScore;
	m_totalPercentage += percentage;
	m_lastGauges.clear();
	scoring.GetAllGaugeValues(m_lastGauges);
	m_totalGauge += scoring.GetTopGauge()->GetValue();

	res.badge = clearMark;
	if (req.clear.Get(false))
	{
		if (clearMark >= ClearMark::NormalClear)
			req.clear.MarkPassed();
		else if (res.failString == "")
			res.failString = "Chart was not cleared";
	}
	else
	{
		req.clear.MarkPassed();
	}

	if (req.min_percentage.HasValue())
	{
		if (static_cast<uint32>(percentage) >= *req.min_percentage)
			req.min_percentage.MarkPassed();
		else if (res.failString == "")
			res.failString = Utility::Sprintf("Percentage of %u%% < Min of %u%%",
				static_cast<uint32>(percentage), *req.min_percentage);
	}

	res.gauge = scoring.GetTopGauge()->GetValue();
	if (req.min_gauge.HasValue())
	{
		if (res.gauge >= *req.min_gauge)
			req.min_gauge.MarkPassed();
		else if (res.failString == "")
			res.failString = Utility::Sprintf("Gauge of %d%% < Min of %d%%",
				(int)(res.gauge * 100), (int)(*req.min_gauge * 100));
	}

	if (req.max_errors.HasValue())
	{
		if (res.errors <= *req.max_errors)
			req.max_errors.MarkPassed();
		else if (res.failString == "")
			res.failString = Utility::Sprintf("%u Errors > Max of %u",
				res.errors, *req.max_errors);
	}

	if (req.max_nears.HasValue())
	{
		if (res.nears <= *req.max_nears)
			req.max_nears.MarkPassed();
		else if (res.failString == "")
			res.failString = Utility::Sprintf("%u Nears > Max of %u",
				res.nears, *req.max_nears);
	}

	res.maxCombo = scoring.maxComboCounter;
	if (req.min_chain.HasValue())
	{
		if (res.maxCombo >= *req.min_chain)
			req.min_chain.MarkPassed();
		else if (res.failString == "")
			res.failString = Utility::Sprintf("%u Chain < Min of %u",
				res.maxCombo, *req.min_chain);
	}

	if (req.min_crits.HasValue())
	{
		if (res.crits >= *req.min_crits)
			req.min_crits.MarkPassed();
		else if (res.failString == "")
			res.failString = Utility::Sprintf("%u Crits < Min of %u",
				res.crits, *req.min_crits);
	}

	res.passed = req.Passed();
	m_passedCurrentChart = res.passed;

	// Now check if we crossed any global failing thresholds
	if (m_totalErrors > m_globalOpts.max_overall_errors.Get(INT_MAX))
		m_failedEarly = true;
	if (m_totalNears > m_globalOpts.max_overall_nears.Get(INT_MAX))
		m_failedEarly = true;

	m_results.push_back(res);
}
