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

#include <functional>

class TitleScreen_Impl : public TitleScreen, public Scriptable
{
private:
	Ref<CommonGUIStyle> m_guiStyle;
	Ref<Canvas> m_canvas;

	void MousePressed(MouseButton button)
	{
		if (IsSuspended())
			return;
		lua_getglobal(L, "mouse_pressed");
		lua_pushnumber(L, (int32)button);
		if (lua_pcall(L, 1, 0, 0) != 0)
		{
			Logf("Lua error on mouse_pressed: %s", Logger::Error, lua_tostring(L, -1));
			g_gameWindow->ShowMessageBox("Lua Error on mouse_pressed", lua_tostring(L, -1), 0);
			assert(false);
		}
		lua_settop(L, 0);
	}

	void SongSelect() { g_application->AddTickable(SongSelect::Create()); }
	int lSongSelect(lua_State* L)
	{
		SongSelect();
		return 0;
	}

	void Exit() { g_application->Shutdown(); }
	int lExit(lua_State* L)
	{
		Exit();
		return 0;
	}

	void Settings() { g_application->AddTickable(SettingsScreen::Create()); }
	int lSettings(lua_State* L)
	{
		Settings();
		return 0;
	}

public:
	bool Init()
	{
		m_guiStyle = g_commonGUIStyle;
		CheckedLoad(g_application->LoadScript("titlescreen", this));
		g_gameWindow->OnMousePressed.Add(this, &TitleScreen_Impl::MousePressed);
		return true;
	}

	virtual void m_InitScriptState()
	{
#define BIND(Type, Member) (std::bind(& Type :: Member, this, std::placeholders::_1))
		m_RegisterMemberFunction("SongSelect", BIND(TitleScreen_Impl, lSongSelect));
		m_RegisterMemberFunction("Settings", BIND(TitleScreen_Impl, lSettings));
		m_RegisterMemberFunction("Exit", BIND(TitleScreen_Impl, lExit));

		m_CreateGlobalObject("menu");
	};

	~TitleScreen_Impl()
	{
	}

	virtual void Render(float deltaTime)
	{
		if (IsSuspended())
			return;

		lua_getglobal(L, "render");
		lua_pushnumber(L, deltaTime);
		if (lua_pcall(L, 1, 0, 0) != 0)
		{
			Logf("Lua error: %s", Logger::Error, lua_tostring(L, -1));
			g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(L, -1), 0);
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
		g_application->ReloadScript("titlescreen", L);
		g_application->DiscordPresenceMenu("Title Screen");
	}


};

TitleScreen* TitleScreen::Create()
{
	TitleScreen_Impl* impl = new TitleScreen_Impl();
	return impl;
}