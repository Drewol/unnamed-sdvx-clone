#pragma once
#include "SongSort.hpp"
#include "SongFilter.hpp"
#include <Beatmap/MapDatabase.hpp>

class TextInput
{
public:
	String input;
	String composition;
	uint32 backspaceCount;
	bool active = false;
	Delegate<const String &> OnTextChanged;

	~TextInput()
	{
		g_gameWindow->OnTextInput.RemoveAll(this);
		g_gameWindow->OnTextComposition.RemoveAll(this);
		g_gameWindow->OnKeyRepeat.RemoveAll(this);
		g_gameWindow->OnKeyPressed.RemoveAll(this);
	}

	void OnTextInput(const String &wstr)
	{
		input += wstr;
		OnTextChanged.Call(input);
	}
	void OnTextComposition(const Graphics::TextComposition &comp)
	{
		composition = comp.composition;
	}
	void OnKeyRepeat(SDL_Scancode key)
	{
		if (key == SDL_SCANCODE_BACKSPACE)
		{
			if (input.empty())
				backspaceCount++; // Send backspace
			else
			{
				auto it = input.end(); // Modify input string instead
				--it;
				while ((*it & 0b11000000) == 0b10000000)
				{
					input.erase(it);
					--it;
				}
				input.erase(it);
				OnTextChanged.Call(input);
			}
		}
	}
	void OnKeyPressed(SDL_Scancode code)
	{
		SDL_Keycode key = SDL_GetKeyFromScancode(code);
		if (key == SDLK_v)
		{
			if (g_gameWindow->GetModifierKeys() == ModifierKeys::Ctrl)
			{
				if (g_gameWindow->GetTextComposition().composition.empty())
				{
					// Paste clipboard text into input buffer
					input += g_gameWindow->GetClipboard();
				}
			}
		}
	}
	void SetActive(bool state)
	{
		active = state;
		if (state)
		{
			SDL_StartTextInput();
			g_gameWindow->OnTextInput.Add(this, &TextInput::OnTextInput);
			g_gameWindow->OnTextComposition.Add(this, &TextInput::OnTextComposition);
			g_gameWindow->OnKeyRepeat.Add(this, &TextInput::OnKeyRepeat);
			g_gameWindow->OnKeyPressed.Add(this, &TextInput::OnKeyPressed);
		}
		else
		{
			SDL_StopTextInput();
			g_gameWindow->OnTextInput.RemoveAll(this);
			g_gameWindow->OnTextComposition.RemoveAll(this);
			g_gameWindow->OnKeyRepeat.RemoveAll(this);
			g_gameWindow->OnKeyPressed.RemoveAll(this);
		}
	}
	void Reset()
	{
		backspaceCount = 0;
		input.clear();
	}
	void Tick()
	{
	}
};

// ItemIndex represents the current item selected
// DBIndex represents the datastructure the db has for that item
template<class ItemSelectIndex, class DBIndex>
class ItemSelectionWheel
{
protected:
	Map<int32, ItemSelectIndex> m_items;
	Map<int32, ItemSelectIndex> m_itemFilter;
	Vector<uint32> m_sortVec;
	Vector<uint32> m_randomVec;

	bool m_filterSet = false;
	IApplicationTickable *m_owner;

	// Currently selected sort index
	uint32 m_selectedSortIndex = 0;

	// Currently selected selection ID
	int32 m_currentlySelectedItemId = 0;

	// current lua item index
	uint32 m_currentlySelectedLuaSortIndex = 0;

	// Index of the last item we had before a change
	int32 m_lastItemIndex = -1;

	// Style to use for everything song select related
	lua_State *m_lua = nullptr;
	String m_lastStatus = "";
	std::mutex m_lock;

	ItemSort<ItemSelectIndex> *m_currentSort = nullptr;

	String luaScript = "";

public:
	Delegate<> OnItemsChanged;

	ItemSelectionWheel(IApplicationTickable *owner) : m_owner(owner)
	{
	}


	virtual bool Init()
	{
		return true;
	}

	virtual void ReloadScript() = 0;

	// Called when the render tick happens
	virtual void Render(float deltaTime) = 0;

	~ItemSelectionWheel() {}


	uint32 GetCurrentItemIndex() {
		if (m_sortVec.empty())
			return -1;
		return m_sortVec[m_selectedSortIndex];
	}

	virtual void OnItemsAdded(Vector<DBIndex*> items)
	{
		bool hadItems = m_items.size() != 0;
		for (auto i : items)
		{
			ItemSelectIndex index(i);
			m_items.Add(index.id, index);

			// Add only if we are not filtering (otherwise the filter will add)
			if (!m_filterSet)
				m_sortVec.push_back(index.id);
		}

		if (!m_filterSet)
		{
			m_doSort();
			// Try to go back to selected song in new sort
			if (hadItems)
				SelectLastItemIndex(true);
			else
				SelectItemBySortIndex(0);
			m_SetCurrentItems();
		}

		// Clear the current queue of random charts
		m_randomVec.clear();

		// Filter will take care of sorting and setting lua
		OnItemsChanged.Call();
	}

	virtual void OnItemsRemoved(Vector<DBIndex*> items)
	{
		for (auto i : items)
		{
			ItemSelectIndex index(i);
			m_items.erase(index.id);

			// Check if the map was in the sort set
			int32 foundSortIndex = m_getSortIndexFromItemIndex(index.id);
			if (foundSortIndex != -1)
				m_sortVec.erase(m_sortVec.begin() + foundSortIndex);
		}

		if (!m_filterSet)
		{
			// Try to go back to selected song in new sort
			SelectLastItemIndex(true);
			m_SetCurrentItems();
		}

		// Clear the current queue of random charts
		m_randomVec.clear();

		// Filter will take care of sorting and setting lua
		OnItemsChanged.Call();
	}

	virtual void OnItemsUpdated(Vector<DBIndex *> items)
	{
		// TODO what does this actually do?
		for (auto i : items)
		{
			ItemSelectIndex index(i);
			assert(m_items.Contains(index.id));
			ItemSelectIndex* item = &m_items.at(index.id);
			*item = index;
		}

		// Clear the current queue of random charts
		m_randomVec.clear();

		OnItemsChanged.Call();
	}

	virtual void OnItemsCleared(Map<int32, DBIndex *> newList)
	{

		m_itemFilter.clear();
		m_items.clear();
		m_sortVec.clear();
		for (auto i : newList)
		{
			ItemSelectIndex index(i.second);
			m_items.Add(index.id, index);
			m_sortVec.push_back(index.id);
		}

		if (m_items.size() == 0)
			return;

		//set all items
		m_SetAllItems();

		// Clear the current queue of random charts
		m_randomVec.clear();

		// Filter will take care of sorting and setting lua
		OnItemsChanged.Call();
	}

	virtual void OnSearchStatusUpdated(String status)
	{
		m_lock.lock();
		m_lastStatus = status;
		m_lock.unlock();
	}

	void SelectRandom()
	{
		if (m_SourceCollection().empty())
			return;

		// If the randomized vector of charts has not been initialized or has
		// been exhausted, we copy the current m_sortVec
		if (m_randomVec.empty()) {
			m_randomVec = m_sortVec;
		}

		uint32 itemIndex;
		if (m_randomVec.size() == 1) {
			itemIndex = m_randomVec.back();
		} else {
			uint32 selection = Random::IntRange(0, (int32)m_randomVec.size() - 1);
			itemIndex = m_randomVec.at(selection);
			m_randomVec[selection] = m_randomVec.back();
		}
		m_randomVec.pop_back();

		SelectItemByItemIndex(itemIndex);
	}

	void SelectItemByItemId(uint32 id)
	{
		for (const auto &it : m_SourceCollection())
		{
			if (m_getDBEntryFromItemIndex((const ItemSelectIndex)it.second)->id == (int32)id)
			{
				SelectItemByItemIndex(it.first);
				break;
			}
		}
	}

	virtual void SelectItemBySortIndex(uint32 sortIndex)
	{
		uint32 vecLen = m_sortVec.size();
		if (vecLen == 0)
			return;
		sortIndex = sortIndex % vecLen;

		m_selectedSortIndex = sortIndex;

		uint32 itemIndex = m_sortVec[sortIndex];

		auto &srcCollection = m_SourceCollection();
		auto it = srcCollection.find(itemIndex);
		if (it != srcCollection.end())
		{
			m_OnItemSelected(it->second);

			//set index in lua
			m_currentlySelectedLuaSortIndex = sortIndex;
			m_SetLuaItemIndex();
		}
		else
		{
			Logf("Could not find item for sort index %u -> %u", Logger::Severity::Warning, sortIndex, itemIndex);
		}

		m_lastItemIndex = itemIndex;
	}

	// TODO this has a seperate behavior for normal song select
	virtual int32 SelectLastItemIndex(bool isFiltered)
	{
		if (m_lastItemIndex == -1)
			return -1;

		int32 res = SelectItemByItemIndex(m_lastItemIndex);

		if (res == -1)
		{
			Logf("Couldn't find original item %u after item set change", Logger::Severity::Info, m_lastItemIndex);
			SelectItemBySortIndex(0);
		}

		return res;
	}

	int32 SelectItemByItemIndex(int32 itemIndex)
	{
		if (itemIndex < 0)
			return -1;
		int32 foundSortIndex = m_getSortIndexFromItemIndex(itemIndex);
		if (foundSortIndex != -1)
			SelectItemBySortIndex(foundSortIndex);
		return foundSortIndex;
	}

	void AdvanceSelection(int32 offset)
	{
		uint32 vecLen = m_sortVec.size();
		if (vecLen == 0)
			return;

		int32 newIndex = m_selectedSortIndex + offset;
		if (newIndex < 0) // Rolled under
		{
			newIndex += vecLen;
		}
		if ((uint32)newIndex >= vecLen) // Rolled over
		{
			newIndex -= vecLen;
		}

		SelectItemBySortIndex(newIndex);
	}

	virtual void AdvancePage(int32 direction)
	{
		lua_getglobal(m_lua, "get_page_size");
		if (lua_isfunction(m_lua, -1))
		{
			if (lua_pcall(m_lua, 0, 1, 0) != 0)
			{
				Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
				g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
			}
			int ret = luaL_checkinteger(m_lua, -1);
			lua_settop(m_lua, 0);
			AdvanceSelection(ret * direction);
		}
		else
		{
			lua_pop(m_lua, 1);
			AdvanceSelection(5 * direction);
		}
	}

	SortType GetSortType() const
	{
		if (m_currentSort)
			return m_currentSort->GetType();
		return SortType::TITLE_ASC;
	}

	void SetSort(ItemSort<ItemSelectIndex> *sort)
	{
		if (sort == m_currentSort)
			return;

		m_currentSort = sort;

		OnSearchStatusUpdated("Sorting by " + m_currentSort->GetName());
		m_doSort();

		// When resorting, jump back to the top
		SelectItemBySortIndex(0);
		m_SetCurrentItems();
	}

	// Set display filter to a set of items
	void SetFilter(Map<int32, DBIndex*> filter)
	{
		m_itemFilter.clear();
		for (auto i : filter)
		{
			ItemSelectIndex index(i.second);
			m_itemFilter.Add(index.id, index);
		}
		m_filterSet = true;

		// Add the filtered maps into the sort vec then sort
		m_sortVec.clear();
		for (auto &it : m_itemFilter)
		{
			m_sortVec.push_back(it.first);
		}
		m_doSort();

		// Clear the current queue of random charts
		m_randomVec.clear();

		// Try to go back to selected song in new sort
		SelectLastItemIndex(true);

		m_SetCurrentItems();
	}

	void SetFilter(Filter<ItemSelectIndex> *filter[2])
	{
		bool isFiltered = false;
		m_itemFilter = m_items;
		for (size_t i = 0; i < 2; i++)
		{
			if (!filter[i])
				continue;
			m_itemFilter = filter[i]->GetFiltered(m_itemFilter);
			if (!filter[i]->IsAll())
				isFiltered = true;
		}
		m_filterSet = isFiltered;

		// Add the filtered maps into the sort vec then sort
		m_sortVec.clear();
		for (auto &it : m_itemFilter)
		{
			m_sortVec.push_back(it.first);
		}
		m_doSort();

		// Clear the current queue of random charts
		m_randomVec.clear();
		
		// Try to go back to selected song in new sort
		SelectLastItemIndex(isFiltered);

		m_SetCurrentItems();
	}

	void ClearFilter()
	{
		if (!m_filterSet)
			return;

		m_filterSet = false;

		// Reset sort vec to all maps and then sort
		m_sortVec.clear();
		for (auto &it : m_items)
		{
			m_sortVec.push_back(it.first);
		}
		m_doSort();

		// Clear the current queue of random charts
		m_randomVec.clear();
		
		// Try to go back to selected song in new sort
		SelectLastItemIndex(true);

		m_SetCurrentItems();
	}

	DBIndex *GetSelection() const
	{
		ItemSelectIndex const *item = m_SourceCollection().Find(
			m_getCurrentlySelectedItemIndex());
		if (item)
			return m_getDBEntryFromItemIndex(item);

		return nullptr;
	}

	friend class TextInput;
	virtual void SetSearchFieldLua(Ref<TextInput> search) = 0;


protected:
	virtual DBIndex* m_getDBEntryFromItemIndex(const ItemSelectIndex*) const = 0;
	virtual DBIndex* m_getDBEntryFromItemIndex(const ItemSelectIndex) const = 0;

	void m_doSort()
	{
		if (m_currentSort == nullptr)
		{
			Log("No sorting set", Logger::Severity::Warning);
			return;
		}
		Logf("Sorting with %s", Logger::Severity::Info, m_currentSort->GetName().c_str());
		m_currentSort->SortInplace(m_sortVec, m_SourceCollection());
	}

	int32 m_getSortIndexFromItemIndex(uint32 itemId) const
	{
		if (m_sortVec.size() == 0)
			return -1;

		const auto &it = std::find(m_sortVec.begin(), m_sortVec.end(), itemId);
		if (it == m_sortVec.end())
			return -1;
		return std::distance(m_sortVec.begin(), it);
	}

	int32 m_getCurrentlySelectedItemIndex() const
	{
		uint32 vecLen = m_sortVec.size();
		if (vecLen == 0)
			return -1;
		if (m_selectedSortIndex >= vecLen)
			return -1;
		return m_sortVec[m_selectedSortIndex];
	}

	const Map<int32, ItemSelectIndex> &m_SourceCollection() const
	{
		return m_filterSet ? m_itemFilter : m_items;
	}

	void m_PushStringToTable(const char *name, const char *data)
	{
		lua_pushstring(m_lua, name);
		lua_pushstring(m_lua, data);
		lua_settable(m_lua, -3);
	}
	void m_PushFloatToTable(const char *name, float data)
	{
		lua_pushstring(m_lua, name);
		lua_pushnumber(m_lua, data);
		lua_settable(m_lua, -3);
	}
	void m_PushIntToTable(const char *name, int data)
	{
		lua_pushstring(m_lua, name);
		lua_pushinteger(m_lua, data);
		lua_settable(m_lua, -3);
	}

	// Lua Updates
	virtual void m_SetLuaItemIndex() {
		if (m_lua == nullptr)
			return;
		lua_getglobal(m_lua, "set_index");
		lua_pushinteger(m_lua, (uint64)m_currentlySelectedLuaSortIndex + 1);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on set_index: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on set_index", lua_tostring(m_lua, -1), 0);
			assert(false);
		}
	}

	virtual void m_SetAllItems() = 0;

	virtual void m_SetCurrentItems() = 0;

	virtual void m_OnItemSelected(ItemSelectIndex index) = 0;
};