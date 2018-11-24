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
#include "HealthGauge.hpp"
#include "Shared/Jobs.hpp"
#include "ScoreScreen.hpp"
#include "Shared/Enum.hpp"
#include "Scriptable.hpp"

class TitleScreen_Impl : public TitleScreen, public Scriptable
{
private:
	Ref<CommonGUIStyle> m_guiStyle;
	Ref<Canvas> m_canvas;
	lua_State* m_lua = nullptr;


	void Exit()
	{
		g_application->Shutdown();
	}

	void Start()
	{
		g_application->AddTickable(SongSelect::Create());
	}

	void Settings()
	{
		g_application->AddTickable(SettingsScreen::Create());
	}

	void MousePressed(MouseButton button)
	{
		if (IsSuspended())
			return;
		lua_getglobal(m_lua, "mouse_pressed");
		lua_pushnumber(m_lua, (int32)button);
		if (lua_pcall(m_lua, 1, 1, 0) != 0)
		{
			Logf("Lua error on mouse_pressed: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on mouse_pressed", lua_tostring(m_lua, -1), 0);
			assert(false);
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

public:
	bool Init()
	{
		m_guiStyle = g_commonGUIStyle;
		CheckedLoad(m_lua = g_application->LoadScript("titlescreen"));
		g_gameWindow->OnMousePressed.Add(this, &TitleScreen_Impl::MousePressed);
		return true;
	}

	~TitleScreen_Impl()
	{
	}

	virtual void InitScriptState(lua_State* L)
	{
	}

	virtual void Render(float deltaTime)
	{
		if (IsSuspended())
			return;

		lua_getglobal(m_lua, "render");
		lua_pushnumber(m_lua, deltaTime);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
			assert(false);
		}
	}
	virtual void OnSuspend()
	{
	}
	virtual void OnRestore()
	{
		g_gameWindow->SetCursorVisible(true);
		g_application->ReloadSkin();
		g_application->ReloadScript("titlescreen", m_lua);
		g_application->DiscordPresenceMenu("Title Screen");
	}


};

TitleScreen* TitleScreen::Create()
{
	TitleScreen_Impl* impl = new TitleScreen_Impl();
	return impl;
}