#include "stdafx.h"
#include "TitleScreen.hpp"
#include "Application.hpp"
#include "TransitionScreen.hpp"
#include "SettingsScreen.hpp"
#include <Shared/Profiling.hpp>
#include "Scoring.hpp"
#include "SongSelect.hpp"
#include <Audio/Audio.hpp>
#include "Track.hpp"
#include "Camera.hpp"
#include "Background.hpp"
#include "Shared/Jobs.hpp"
#include "ScoreScreen.hpp"
#include "Shared/Enum.hpp"
#include "lua.hpp"
#include "Shared/LuaBindable.hpp"
#include "DownloadScreen.hpp"
#include "MultiplayerScreen.hpp"
#include "ChallengeSelect.hpp"

class TitleScreen_Impl : public TitleScreen
{
private:
	lua_State* m_lua = nullptr;
	LuaBindable* m_luaBinds = nullptr;

	void Exit()
	{
		g_application->Shutdown();
	}

	int lExit(lua_State* L)
	{
		Exit();
		return 0;
	}

	void Start()
	{
		g_transition->TransitionTo(SongSelect::Create());
	}

	int lStart(lua_State* L)
	{
		Start();
		return 0;
	}

	int lDownloads(lua_State* L)
	{
		g_application->AddTickable(new DownloadScreen());
		return 0;
	}

	int lMultiplayer(lua_State* L)
	{
		g_transition->TransitionTo(new MultiplayerScreen());
		return 0;
	}

	int lChallengeSelect(lua_State* L)
	{
		g_transition->TransitionTo(ChallengeSelect::Create());
		return 0;
	}

	int lUpdate(lua_State* L)
	{
		g_application->RunUpdater();
		return 0;
	}

	void Settings()
	{
		g_application->AddTickable(SettingsScreen::Create());
	}

	int lSettings(lua_State* L)
	{
		Settings();
		return 0;
	}

	void MousePressed(MouseButton button)
	{
		if (IsSuspended())
			return;
		lua_getglobal(m_lua, "mouse_pressed");
		lua_pushnumber(m_lua, (int32)button);
		if (lua_pcall(m_lua, 1, 1, 0) != 0)
		{
			g_application->ScriptError("titlescreen", m_lua);
		}
		int ret = luaL_checkinteger(m_lua, 1);
		lua_settop(m_lua, 0);
		switch (ret)
		{
		case 1:
			Start();
			break;
		case 2:
			Settings();
			break;
		case 3:
			Exit();
			break;
		default:
			break;
		}
	}

	void m_OnButtonPressed(Input::Button buttonCode)
	{
		if (IsSuspended())
			return;

		lua_getglobal(m_lua, "button_pressed");
		if (lua_isfunction(m_lua, -1))
		{
			lua_pushnumber(m_lua, (int32)buttonCode);
			if (lua_pcall(m_lua, 1, 0, 0) != 0)
			{
				g_application->ScriptError("titlescreen", m_lua);
			}
		}
		lua_settop(m_lua, 0);
	}

public:
	bool Init()
	{
		m_lua = g_application->LoadScript("titlescreen");
		if (m_lua == nullptr)
		{
			Settings();
			return false;
		}
		m_luaBinds = new LuaBindable(m_lua, "Menu");
		m_luaBinds->AddFunction("Exit", this, &TitleScreen_Impl::lExit);
		m_luaBinds->AddFunction("Settings", this, &TitleScreen_Impl::lSettings);
		m_luaBinds->AddFunction("Start", this, &TitleScreen_Impl::lStart);
		m_luaBinds->AddFunction("DLScreen", this, &TitleScreen_Impl::lDownloads);
		m_luaBinds->AddFunction("Update", this, &TitleScreen_Impl::lUpdate);

		m_luaBinds->AddFunction("Multiplayer", this, &TitleScreen_Impl::lMultiplayer);
		m_luaBinds->AddFunction("Challenges", this, &TitleScreen_Impl::lChallengeSelect);

		m_luaBinds->Push();
		lua_settop(m_lua, 0);
		g_gameWindow->OnMousePressed.Add(this, &TitleScreen_Impl::MousePressed);
		g_input.OnButtonPressed.Add(this, &TitleScreen_Impl::m_OnButtonPressed);
		return true;
	}
	
	TitleScreen_Impl()
	{
		g_gameWindow->OnMousePressed.RemoveAll(this);
	}

	~TitleScreen_Impl()
	{
		g_gameWindow->OnMousePressed.RemoveAll(this);
		g_input.OnButtonPressed.RemoveAll(this);
		if (m_lua)
		{
			g_application->DisposeLua(m_lua);
			m_lua = nullptr;
		}
		if (m_luaBinds)
		{
			delete m_luaBinds;
			m_luaBinds = nullptr;
		}
	}

	virtual void Render(float deltaTime)
	{
		if (IsSuspended())
			return;

		lua_getglobal(m_lua, "render");
		lua_pushnumber(m_lua, deltaTime);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			if (!g_application->ScriptError("titlescreen", m_lua)) {
				Settings(); //Go to settings since there are no scripts there and you can change skin from there
				g_application->RemoveTickable(this);
			}
		}
	}
	virtual void OnSuspend()
	{
	}
	virtual void OnRestore()
	{
		g_gameWindow->SetCursorVisible(true);
		g_application->DiscordPresenceMenu("Title Screen");
	}


};

TitleScreen* TitleScreen::Create()
{
	TitleScreen_Impl* impl = new TitleScreen_Impl();
	return impl;
}