#include "stdafx.h"
#include "CollectionDialog.hpp"
#include "Shared/LuaBindable.hpp"
#include "Beatmap/MapDatabase.hpp"
#include "lua.hpp"
#include "GameConfig.hpp"

// XXX probably should be moved with the other ones to its own class file?
class TextInputCollectionDialog
{
public:
	String input;
	String composition;
	uint32 backspaceCount;
	bool active = false;
	Delegate<const String&> OnTextChanged;
	Delegate<const String&> OnReturn;
	bool start_taking_input = false;

	~TextInputCollectionDialog()
	{
		g_gameWindow->OnTextInput.RemoveAll(this);
		g_gameWindow->OnTextComposition.RemoveAll(this);
		g_gameWindow->OnKeyRepeat.RemoveAll(this);
		g_gameWindow->OnKeyPressed.RemoveAll(this);
	}

	void OnTextInput(const String& wstr)
	{
		if (!start_taking_input)
			return;
		input += wstr;
		OnTextChanged.Call(input);
	}
	void OnTextComposition(const Graphics::TextComposition& comp)
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
		else if (code == SDL_SCANCODE_RETURN)
		{
			OnReturn.Call(input);
		}
	}
	void SetActive(bool state)
	{
		active = state;
		if (state)
		{
			start_taking_input = false;

			SDL_StartTextInput();
			g_gameWindow->OnTextInput.Add(this, &TextInputCollectionDialog::OnTextInput);
			g_gameWindow->OnTextComposition.Add(this, &TextInputCollectionDialog::OnTextComposition);
			g_gameWindow->OnKeyRepeat.Add(this, &TextInputCollectionDialog::OnKeyRepeat);
			g_gameWindow->OnKeyPressed.Add(this, &TextInputCollectionDialog::OnKeyPressed);
		}
		else
		{
			SDL_StopTextInput();
			g_gameWindow->OnTextInput.RemoveAll(this);
			g_gameWindow->OnTextComposition.RemoveAll(this);
			g_gameWindow->OnKeyRepeat.RemoveAll(this);
			g_gameWindow->OnKeyPressed.RemoveAll(this);
			Reset();
		}
	}
	void Reset()
	{
		backspaceCount = 0;
		input.clear();
	}
	void Tick()
	{
		// Wait until we release the start button
		if (active && !start_taking_input && !g_input.GetButton(Input::Button::BT_S))
		{
			start_taking_input = true;
		}
	}
};



CollectionDialog::~CollectionDialog()
{
	g_input.OnButtonPressed.RemoveAll(this);
	g_gameWindow->OnKeyPressed.RemoveAll(this);
	if (m_lua)
	{
		g_application->DisposeLua(m_lua);
		m_lua = nullptr;
	}
}

bool CollectionDialog::Init(MapDatabase* songdb)
{
	if (!songdb)
	{
		return false;
	}
	m_songdb = songdb;
	m_lua = g_application->LoadScript("collectiondialog");
	if (!m_lua)
	{
		return false;
	}

	{
		lua_newtable(m_lua);

		lua_pushstring(m_lua, "collections");
		lua_newtable(m_lua);
		lua_settable(m_lua, -3);

		lua_setglobal(m_lua, "dialog");
	}

	m_bindable = Ref<LuaBindable>(new LuaBindable(m_lua, "menu"));
	m_bindable->AddFunction("Cancel", this, &CollectionDialog::lCancel);
	m_bindable->AddFunction("Confirm", this, &CollectionDialog::lConfirm);
	m_bindable->AddFunction("ChangeState", this, &CollectionDialog::lChangeState);
	m_bindable->Push();
	lua_settop(m_lua, 0);

	m_sensMult = g_gameConfig.GetFloat(GameConfigKeys::SongSelSensMult);

	g_input.OnButtonPressed.Add(this, &CollectionDialog::m_OnButtonPressed);
	g_gameWindow->OnKeyPressed.Add(this, &CollectionDialog::m_OnKeyPressed);

	m_nameEntry = Ref<TextInputCollectionDialog>(new TextInputCollectionDialog());
	m_nameEntry->OnReturn.Add(this, &CollectionDialog::m_OnEntryReturn);

	m_isInitialized = true;
	return true;
}

void CollectionDialog::Tick(float deltaTime)
{
	//Handle knob navigation
	float input = g_input.GetInputLaserDir(0) + g_input.GetInputLaserDir(1);
	m_knobAdvance += input * m_sensMult;
	if (fabsf(m_knobAdvance) >= 1.0f)
	{
		int advance = fabsf(m_knobAdvance);
		advance *= (int)Math::Sign(m_knobAdvance);
		m_knobAdvance -= advance;
		m_AdvanceSelection(advance);
	}

	m_nameEntry->Tick();
	lua_getglobal(m_lua, "dialog");
	lua_pushstring(m_lua, "newName");
	lua_pushstring(m_lua, *m_nameEntry->input);
	lua_settable(m_lua, -3);
	lua_setglobal(m_lua, "dialog");

	if (m_shouldChangeState) //delay this till here because we can't remove the event handler inside the event
	{
		m_ChangeState();
		m_shouldChangeState = false;
	}
}

void CollectionDialog::Render(float deltaTime)
{
	g_application->ForceRender();
	lua_getglobal(m_lua, "render");
	lua_pushnumber(m_lua, deltaTime);
	if (lua_pcall(m_lua, 1, 1, 0) != 0)
	{
		Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
		g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
		Close();
		m_Finish();
	}
	else
	{
		bool hasClosed = !lua_toboolean(m_lua, lua_gettop(m_lua));
		if (hasClosed)
		{
			m_Finish();
		}
	}
}

void CollectionDialog::Open(const ChartIndex* song)
{
	if (song == nullptr)
		return;
	m_active = true;
	m_currentId = song->folderId;
	m_nameEntry->SetActive(false);
	m_nameEntry->Reset();

	//set lua table
	lua_getglobal(m_lua, "dialog");

	lua_pushstring(m_lua, "title");
	lua_pushstring(m_lua, *song->title);
	lua_settable(m_lua, -3);

	lua_pushstring(m_lua, "artist");
	lua_pushstring(m_lua, *song->artist);
	lua_settable(m_lua, -3);

	lua_pushstring(m_lua, "jacket");
	lua_pushstring(m_lua, *song->jacket_path);
	lua_settable(m_lua, -3);

	m_closing = false;
	lua_pushstring(m_lua, "closing");
	lua_pushboolean(m_lua, m_closing);
	lua_settable(m_lua, -3);

	lua_pushstring(m_lua, "isTextEntry");
	lua_pushboolean(m_lua, m_nameEntry->active);
	lua_settable(m_lua, -3);

	lua_pushstring(m_lua, "collections");
	lua_newtable(m_lua);

	auto existing = m_songdb->GetCollectionsForMap(m_currentId);
	int i = 1;
	for (String name : m_songdb->GetCollections())
	{
		lua_pushinteger(m_lua, i++);
		{
			lua_newtable(m_lua);
			lua_pushstring(m_lua, "name");
			lua_pushstring(m_lua, *name);
			lua_settable(m_lua, -3);

			lua_pushstring(m_lua, "exists");
			lua_pushboolean(m_lua, existing.Contains(name));
			lua_settable(m_lua, -3);
		}
		lua_settable(m_lua, -3);

	}
	lua_settable(m_lua, -3);

	lua_setglobal(m_lua, "dialog");

	lua_getglobal(m_lua, "open");
	if (lua_isfunction(m_lua, -1))
	{
		if (lua_pcall(m_lua, 0, 0, 0) != 0)
		{
			Logf("Lua error on open: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on open", lua_tostring(m_lua, -1), 0);
		}
	}
	lua_settop(m_lua, 0);
}

void CollectionDialog::Close()
{
	lua_getglobal(m_lua, "dialog");
	m_closing = true;
	lua_pushstring(m_lua, "closing");
	lua_pushboolean(m_lua, m_closing);
	lua_settable(m_lua, -3);
	lua_setglobal(m_lua, "dialog");
}

bool CollectionDialog::IsActive()
{
	return m_active;
}

bool CollectionDialog::IsInitialized()
{
	return m_isInitialized;
}

int CollectionDialog::lConfirm(lua_State* L)
{
	if (m_closing)
		return 0;

	String collectionName(luaL_checkstring(L, 2));
	m_songdb->AddOrRemoveToCollection(collectionName, m_currentId);
	OnCompletion.Call();
	Close();
	return 0;
}

int CollectionDialog::lCancel(lua_State* L)
{
	Close();
	return 0;
}

int CollectionDialog::lChangeState(lua_State* L)
{
	if (m_closing)
		return 0;

	m_shouldChangeState = true;
	return 0;
}

void CollectionDialog::m_ChangeState()
{
	m_nameEntry->SetActive(!m_nameEntry->active);

	lua_getglobal(m_lua, "dialog");
	lua_pushstring(m_lua, "isTextEntry");
	lua_pushboolean(m_lua, m_nameEntry->active);
	lua_settable(m_lua, -3);
	lua_setglobal(m_lua, "dialog");
}

void CollectionDialog::m_AdvanceSelection(int steps)
{
	lua_getglobal(m_lua, "advance_selection");
	if (lua_isfunction(m_lua, -1))
	{
		lua_pushnumber(m_lua, (int32)steps);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on advance_selection: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on advance_selection", lua_tostring(m_lua, -1), 0);
		}
	}
	lua_settop(m_lua, 0);
}

void CollectionDialog::m_OnButtonPressed(Input::Button button)
{
	if (!m_active || m_nameEntry->active || m_closing)
		return;

	lua_getglobal(m_lua, "button_pressed");
	if (lua_isfunction(m_lua, -1))
	{
		lua_pushnumber(m_lua, (int32)button);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on button_pressed: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on button_pressed", lua_tostring(m_lua, -1), 0);
		}
	}
	lua_settop(m_lua, 0);
}

void CollectionDialog::m_OnKeyPressed(SDL_Scancode code)
{
	if (!m_active || m_closing)
	{
		return;
	}

	if (m_nameEntry->active)
	{
		if (code == SDL_SCANCODE_ESCAPE)
			m_shouldChangeState = true;
		return;
	}

	if (code == SDL_SCANCODE_DOWN)
	{
		m_AdvanceSelection(1);
	}
	else if (code == SDL_SCANCODE_UP)
	{
		m_AdvanceSelection(-1);
	}
}

void CollectionDialog::m_OnEntryReturn(const String& name)
{
	m_songdb->AddOrRemoveToCollection(name, m_currentId);
	OnCompletion.Call();
	Close();
}

void CollectionDialog::m_Finish()
{
	m_nameEntry->SetActive(false);
	m_active = false;
}
