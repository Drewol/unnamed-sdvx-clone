#include "stdafx.h"
#include "Test.hpp"
#include "Application.hpp"
#include <Shared/Profiling.hpp>
#include "Scoring.hpp"
#include <Audio/Audio.hpp>
#include "Track.hpp"
#include "Camera.hpp"
#include "Background.hpp"
#include "Shared/Jobs.hpp"
#include "ScoreScreen.hpp"
#include "Shared/Enum.hpp"

class Test_Impl : public Test
{
private:
	WString m_currentText;
	Ref<Gamepad> m_gamepad;
	Vector<String> m_textSettings;

public:
	static void StaticFunc(int32 arg)
	{
	}
	static int32 StaticFunc1(int32 arg)
	{
		return arg * 2;
	}
	static int32 StaticFunc2(int32 arg)
	{
		return arg * 2;
	}
	bool Init() override
	{
		m_gamepad = g_gameWindow->OpenGamepad(0);
		return true;
	}
	~Test_Impl()
	{
	}
	void OnKeyPressed(SDL_Scancode code, int32 delta) override
	{
		if(code == SDL_SCANCODE_TAB)
		{
			//m_settings->SetShow(!m_settings->IsShown());
		}
	}
	void OnKeyReleased(SDL_Scancode code, int32 delta) override
	{
	}
	void Render(float deltaTime) override
	{
	}
	void Tick(float deltaTime) override
	{
	}
};

Test* Test::Create()
{
	Test_Impl* impl = new Test_Impl();
	return impl;
}
