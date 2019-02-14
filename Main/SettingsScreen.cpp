#include "stdafx.h"
#include "SettingsScreen.hpp"
#include "Application.hpp"
#include <Shared/Profiling.hpp>
#include "GameConfig.hpp"
#include "Scoring.hpp"
#include <Audio/Audio.hpp>
#include "Track.hpp"
#include "Camera.hpp"
#include "Background.hpp"
#include "HealthGauge.hpp"
#include "Shared/Jobs.hpp"
#include "ScoreScreen.hpp"
#include "Shared/Enum.hpp"
#include "Input.hpp"
#include <queue>
#ifdef _WIN32
#include "SDL_keyboard.h"
#include <SDL.h>
#else
#include "SDL2/SDL_keyboard.h"
#include <SDL2/SDL.h>
#endif
#include "nanovg.h"


#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_SDL_GL3_IMPLEMENTATION
#include "../third_party/nuklear/nuklear.h"
#include "nuklear/nuklear_sdl_gl3.h"

#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024

class SettingsScreen_Impl : public SettingsScreen
{
private:
	nk_context* m_nctx;
	nk_font_atlas m_nfonts;

	const char* m_speedMods[3] = { "XMod", "MMod", "CMod" };
	const char* m_laserModes[3] = { "Keyboard", "Mouse", "Controller" };
	const char* m_buttonModes[2] = { "Keyboard", "Controller" };
	const char* m_aaModes[5] = { "Off", "2x MSAA", "4x MSAA", "8x MSAA", "16x MSAA" };
	Vector<String> m_gamePads;
	Vector<String> m_skins;

	Vector<GameConfigKeys> m_keyboardKeys = {
		GameConfigKeys::Key_BTS,
		GameConfigKeys::Key_BT0,
		GameConfigKeys::Key_BT1,
		GameConfigKeys::Key_BT2,
		GameConfigKeys::Key_BT3,
		GameConfigKeys::Key_FX0,
		GameConfigKeys::Key_FX1,
		GameConfigKeys::Key_BTB
	};

	Vector<GameConfigKeys> m_keyboardLaserKeys = {
		GameConfigKeys::Key_Laser0Neg,
		GameConfigKeys::Key_Laser0Pos,
		GameConfigKeys::Key_Laser1Neg,
		GameConfigKeys::Key_Laser1Pos,
	};

	Vector<GameConfigKeys> m_controllerKeys = {
		GameConfigKeys::Controller_BTS,
		GameConfigKeys::Controller_BT0,
		GameConfigKeys::Controller_BT1,
		GameConfigKeys::Controller_BT2,
		GameConfigKeys::Controller_BT3,
		GameConfigKeys::Controller_FX0,
		GameConfigKeys::Controller_FX1,
		GameConfigKeys::Controller_BTB
	};

	Vector<GameConfigKeys> m_controllerLaserKeys = {
		GameConfigKeys::Controller_Laser0Axis,
		GameConfigKeys::Controller_Laser1Axis,

	};

	//Button* m_buttonButtons[7];
	//Button* m_laserButtons[2];
	//Panel* m_laserColorPanels[2];

	Texture m_whiteTex;

	int m_speedMod = 0;
	int m_laserMode = 0;
	int m_buttonMode = 0;
	int m_selectedGamepad = 0;
	int m_selectedSkin = 0;
	int m_globalOffset = 0;
	int m_inputOffset = 0;
	int m_antialiasing = 0;
	int m_bounceGuard = 0;
	float m_modSpeed = 400.f;
	float m_hispeed = 1.f;
	float m_laserSens = 1.0f;
	float m_masterVolume = 1.0f;
	float m_laserColors[2] = { 0.25f, 0.75f };
	String m_controllerButtonNames[8];
	String m_controllerLaserNames[2];
	char m_songsPath[1024];
	int m_pathlen = 0;
	int m_wasapi = 1;
	int m_update = 1;
	int m_windowFull = 0;

	std::queue<SDL_Event> eventQueue;

	void UpdateNuklearInput(SDL_Event evt)
	{
		eventQueue.push(evt);
	}

	//TODO: Use argument instead of many functions if possible.
	void SetKey_BTA()
	{
		if (m_buttonMode == 1)
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_BT0, true, m_selectedGamepad));
		else
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Key_BT0));
	}
	void SetKey_BTB()
	{
		if (m_buttonMode == 1)
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_BT1, true, m_selectedGamepad));
		else
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Key_BT1));
	}
	void SetKey_BTC()
	{
		if (m_buttonMode == 1)
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_BT2, true, m_selectedGamepad));
		else
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Key_BT2));
	}
	void SetKey_BTD()
	{
		if (m_buttonMode == 1)
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_BT3, true, m_selectedGamepad));
		else
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Key_BT3));
	}
	void SetKey_FXL()
	{
		if (m_buttonMode == 1)
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_FX0, true, m_selectedGamepad));
		else
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Key_FX0));
	}
	void SetKey_FXR()
	{
		if (m_buttonMode == 1)
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_FX1, true, m_selectedGamepad));
		else
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Key_FX1));
	}
	void SetKey_ST()
	{
		if (m_buttonMode == 1)
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_BTS, true, m_selectedGamepad));
		else
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Key_BTS));
	}

	void SetLL()
	{
		g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_Laser0Axis, m_laserMode == 2, m_selectedGamepad));
	}
	void SetRL()
	{
		g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_Laser1Axis, m_laserMode == 2, m_selectedGamepad));
	}

	void SetKey_BK()
	{
		if (m_buttonMode == 1)
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_BTB, true, m_selectedGamepad));
		else
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Key_BTB));
	}

	void CalibrateSens()
	{
		LaserSensCalibrationScreen* sensScreen = LaserSensCalibrationScreen::Create();
		sensScreen->SensSet.Add(this, &SettingsScreen_Impl::SetSens);
		g_application->AddTickable(sensScreen);
	}

	void SetSens(float sens)
	{
		m_laserSens = sens;
	}

	void Exit()
	{
		g_gameWindow->OnAnyEvent.RemoveAll(this);

		Map<String, InputDevice> inputModeMap = {
			{ "Keyboard", InputDevice::Keyboard },
			{ "Mouse", InputDevice::Mouse },
			{ "Controller", InputDevice::Controller },
		};
		
		g_gameConfig.SetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice, inputModeMap[m_buttonModes[m_buttonMode]]);
		g_gameConfig.SetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice, inputModeMap[m_laserModes[m_laserMode]]);

		g_gameConfig.Set(GameConfigKeys::HiSpeed, m_hispeed);
		g_gameConfig.Set(GameConfigKeys::AntiAliasing, m_antialiasing);
		g_gameConfig.Set(GameConfigKeys::ModSpeed, m_modSpeed);
		g_gameConfig.Set(GameConfigKeys::MasterVolume, m_masterVolume);
		g_gameConfig.Set(GameConfigKeys::Laser0Color, m_laserColors[0]);
		g_gameConfig.Set(GameConfigKeys::Laser1Color, m_laserColors[1]);
		g_gameConfig.Set(GameConfigKeys::Controller_DeviceID, m_selectedGamepad);
		g_gameConfig.Set(GameConfigKeys::GlobalOffset, m_globalOffset);
		g_gameConfig.Set(GameConfigKeys::WASAPI_Exclusive, m_wasapi == 0);
		g_gameConfig.Set(GameConfigKeys::CheckForUpdates, m_update == 0);
		g_gameConfig.Set(GameConfigKeys::WindowedFullscreen, m_windowFull == 0);
		g_gameConfig.Set(GameConfigKeys::InputOffset, m_inputOffset);
		g_gameConfig.Set(GameConfigKeys::InputBounceGuard, m_bounceGuard);

		String songsPath = String(m_songsPath, m_pathlen);
		songsPath.TrimBack('\n');
		songsPath.TrimBack(' ');
		g_gameConfig.Set(GameConfigKeys::SongFolder, songsPath);

		if(m_skins.size() > 0)
			g_gameConfig.Set(GameConfigKeys::Skin, m_skins[m_selectedSkin]);

		switch (inputModeMap[m_laserModes[m_laserMode]])
		{
		case InputDevice::Controller:
			g_gameConfig.Set(GameConfigKeys::Controller_Sensitivity, m_laserSens);
			break;
		case InputDevice::Mouse:
			g_gameConfig.Set(GameConfigKeys::Mouse_Sensitivity, m_laserSens);
			break;
		case InputDevice::Keyboard:
		default:
			g_gameConfig.Set(GameConfigKeys::Key_Sensitivity, m_laserSens);
			break;
		}

		if (m_speedMod == 2)
		{
			g_gameConfig.Set(GameConfigKeys::UseCMod, true);
			g_gameConfig.Set(GameConfigKeys::UseMMod, false);
		}
		else if (m_speedMod == 1)
		{
			g_gameConfig.Set(GameConfigKeys::UseCMod, false);
			g_gameConfig.Set(GameConfigKeys::UseMMod, true);
		}
		else
		{
			g_gameConfig.Set(GameConfigKeys::UseCMod, false);
			g_gameConfig.Set(GameConfigKeys::UseMMod, false);
		}

		g_input.Cleanup();
		g_input.Init(*g_gameWindow);
		g_application->RemoveTickable(this);
	}

public:

	//TODO: Controller support and the rest of the options and better layout
	bool Init()
	{
		m_gamePads = g_gameWindow->GetGamepadDeviceNames();	
		m_skins = Path::GetSubDirs("./skins/");

		m_nctx = nk_sdl_init((SDL_Window*)g_gameWindow->Handle());
		g_gameWindow->OnAnyEvent.Add(this, &SettingsScreen_Impl::UpdateNuklearInput);
		{
			struct nk_font_atlas *atlas;
			nk_sdl_font_stash_begin(&atlas);
			struct nk_font *fallback = nk_font_atlas_add_from_file(atlas, Path::Normalize("fonts/fallback_truetype.ttf").c_str(), 24, 0);

			nk_sdl_font_stash_end();
			//nk_style_load_all_cursors(m_nctx, atlas->cursors);
			nk_style_set_font(m_nctx, &fallback->handle);
		}
		m_nctx->style.text.color = nk_rgb(255, 255, 255);
		m_nctx->style.button.border_color = nk_rgb(0, 128, 255);
		m_nctx->style.button.padding = nk_vec2(5,5);
		m_nctx->style.button.rounding = 0;
		m_nctx->style.window.fixed_background = nk_style_item_color(nk_rgb(40, 40, 40));
		m_nctx->style.slider.bar_normal = nk_rgb(20, 20, 20);
		m_nctx->style.slider.bar_hover = nk_rgb(20, 20, 20);
		m_nctx->style.slider.bar_active = nk_rgb(20, 20, 20);



		if (g_gameConfig.GetBool(GameConfigKeys::UseCMod))
			m_speedMod = 2;
		else if (g_gameConfig.GetBool(GameConfigKeys::UseMMod))
			m_speedMod = 1;

		switch (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice))
		{
		case InputDevice::Controller:
			m_buttonMode = 1;
			break;
		case InputDevice::Keyboard:
		default:
			m_buttonMode = 0;
			break;
		}

		switch (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice))
		{
		case InputDevice::Controller:
			m_laserMode = 2;
			m_laserSens = g_gameConfig.GetFloat(GameConfigKeys::Controller_Sensitivity);
			break;
		case InputDevice::Mouse:
			m_laserMode = 1;
			m_laserSens = g_gameConfig.GetFloat(GameConfigKeys::Mouse_Sensitivity);
			break;
		case InputDevice::Keyboard:
		default:
			m_laserMode = 0;
			m_laserSens = g_gameConfig.GetFloat(GameConfigKeys::Key_Sensitivity);
			break;
		}

		m_antialiasing = g_gameConfig.GetInt(GameConfigKeys::AntiAliasing);

		m_modSpeed = g_gameConfig.GetFloat(GameConfigKeys::ModSpeed);
		m_hispeed = g_gameConfig.GetFloat(GameConfigKeys::HiSpeed);
		m_masterVolume = g_gameConfig.GetFloat(GameConfigKeys::MasterVolume);
		m_laserColors[0] = g_gameConfig.GetFloat(GameConfigKeys::Laser0Color);
		m_laserColors[1] = g_gameConfig.GetFloat(GameConfigKeys::Laser1Color);

		m_globalOffset = g_gameConfig.GetInt(GameConfigKeys::GlobalOffset);
		m_inputOffset = g_gameConfig.GetInt(GameConfigKeys::InputOffset);
		m_bounceGuard = g_gameConfig.GetInt(GameConfigKeys::InputBounceGuard);
		m_wasapi = g_gameConfig.GetBool(GameConfigKeys::WASAPI_Exclusive) ? 0 : 1;
		m_update = g_gameConfig.GetBool(GameConfigKeys::CheckForUpdates) ? 0 : 1;
		m_windowFull = g_gameConfig.GetBool(GameConfigKeys::WindowedFullscreen) ? 0 : 1;

		String songspath = g_gameConfig.GetString(GameConfigKeys::SongFolder);
		strcpy(m_songsPath, songspath.c_str());
		m_pathlen = songspath.length();

		m_selectedGamepad = g_gameConfig.GetInt(GameConfigKeys::Controller_DeviceID);
		if (m_selectedGamepad >= m_gamePads.size())
		{
			m_selectedGamepad = 0;
		}
		auto skinSearch = std::find(m_skins.begin(), m_skins.end(), g_gameConfig.GetString(GameConfigKeys::Skin));
		if (skinSearch == m_skins.end())
			m_selectedSkin = 0;
		else
			m_selectedSkin = skinSearch - m_skins.begin();
		return true;
	}

	void Tick(float deltatime)
	{

		nk_input_begin(m_nctx);
		while (!eventQueue.empty())
		{
			nk_sdl_handle_event(&eventQueue.front());
			eventQueue.pop();
		}
		nk_input_end(m_nctx);

		for (size_t i = 0; i < 8; i++)
		{
			if (m_buttonMode == 1)
			{
				m_controllerButtonNames[i] = Utility::Sprintf("%d", g_gameConfig.GetInt(m_controllerKeys[i]));
			}
			else
			{
				m_controllerButtonNames[i] = SDL_GetKeyName(g_gameConfig.GetInt(m_keyboardKeys[i]));
			}
		}
		for (size_t i = 0; i < 2; i++)
		{
			if (m_laserMode == 2)
			{
				m_controllerLaserNames[i] = Utility::Sprintf("%d", g_gameConfig.GetInt(m_controllerLaserKeys[i]));
			}
			else
			{
				m_controllerLaserNames[i] = Utility::ConvertToUTF8(Utility::WSprintf( //wstring->string because regular Sprintf messes up(?????)
					L"%ls / %ls",
					Utility::ConvertToWString(SDL_GetKeyName(g_gameConfig.GetInt(m_keyboardLaserKeys[i * 2]))),
					Utility::ConvertToWString(SDL_GetKeyName(g_gameConfig.GetInt(m_keyboardLaserKeys[i * 2 + 1])))
				));
			}
		}
	}

	void Render(float deltatime)
	{
		if (IsSuspended())
			return;

		float buttonheight = 30;

		Vector<const char*> pads;
		Vector<const char*> skins;

		for (size_t i = 0; i < m_gamePads.size(); i++)
		{
			pads.Add(m_gamePads[i].GetData());
		}

		for (size_t i = 0; i < m_skins.size(); i++)
		{
			skins.Add(m_skins[i].GetData());
		}

		nk_color lcol = nk_hsv_f(m_laserColors[0] / 360, 1, 1);
		nk_color rcol = nk_hsv_f(m_laserColors[1] / 360, 1, 1);

		float w = Math::Min(g_resolution.y / 1.4, g_resolution.x - 5.0);
		float x = g_resolution.x / 2 - w / 2;

		if (nk_begin(m_nctx, "Settings", nk_rect(x, 0, w, g_resolution.y), 0))
		{
			auto comboBoxSize = nk_vec2(w - 30 ,250);

			nk_layout_row_dynamic(m_nctx, buttonheight, 3);
			if (nk_button_label(m_nctx, m_controllerLaserNames[0].c_str())) SetLL();
			if (nk_button_label(m_nctx, m_controllerButtonNames[0].c_str())) SetKey_ST();
			if (nk_button_label(m_nctx, m_controllerLaserNames[1].c_str())) SetRL();

			nk_layout_row_dynamic(m_nctx, buttonheight, 4);
			if (nk_button_label(m_nctx, m_controllerButtonNames[1].c_str())) SetKey_BTA();
			if (nk_button_label(m_nctx, m_controllerButtonNames[2].c_str())) SetKey_BTB();
			if (nk_button_label(m_nctx, m_controllerButtonNames[3].c_str())) SetKey_BTC();
			if (nk_button_label(m_nctx, m_controllerButtonNames[4].c_str())) SetKey_BTD();
			nk_layout_row_dynamic(m_nctx, buttonheight, 2);
			if (nk_button_label(m_nctx, m_controllerButtonNames[5].c_str())) SetKey_FXL();
			if (nk_button_label(m_nctx, m_controllerButtonNames[6].c_str())) SetKey_FXR();

			nk_layout_row_dynamic(m_nctx, buttonheight, 1);
			if (nk_button_label(m_nctx, "Calibrate Laser Sensitivity")) CalibrateSens();

			nk_layout_row_dynamic(m_nctx, buttonheight, 1);
			if (nk_button_label(m_nctx, m_controllerButtonNames[7].c_str())) SetKey_BK();

			nk_labelf(m_nctx, nk_text_alignment::NK_TEXT_LEFT, "Laser sensitivity (%f):", m_laserSens);
			nk_slider_float(m_nctx, 0, &m_laserSens, 20, 0.001);

			nk_label(m_nctx, "Button input mode:", nk_text_alignment::NK_TEXT_LEFT);
			nk_combobox(m_nctx, m_buttonModes, 2, &m_buttonMode, buttonheight, comboBoxSize);

			nk_label(m_nctx, "Laser input mode:", nk_text_alignment::NK_TEXT_LEFT);
			nk_combobox(m_nctx, m_laserModes, 3, &m_laserMode, buttonheight, comboBoxSize);

			if (m_gamePads.size() > 0)
			{
				nk_label(m_nctx, "Selected Controller:", nk_text_alignment::NK_TEXT_LEFT);
				nk_combobox(m_nctx, pads.data(), m_gamePads.size(), &m_selectedGamepad, buttonheight, comboBoxSize);
			}

			m_globalOffset = nk_propertyi(m_nctx, "Global Offset:", -1000, m_globalOffset, 1000, 1, 1);
			m_inputOffset = nk_propertyi(m_nctx, "Input Offset:", -1000, m_inputOffset, 1000, 1, 1);

			nk_label(m_nctx, "Speed mod:", nk_text_alignment::NK_TEXT_LEFT);
			nk_combobox(m_nctx, m_speedMods, 3, &m_speedMod, buttonheight, comboBoxSize);

			nk_labelf(m_nctx, nk_text_alignment::NK_TEXT_LEFT, "HiSpeed (%f):", m_hispeed);
			nk_slider_float(m_nctx, 0.25, &m_hispeed, 20, 0.05);

			nk_labelf(m_nctx, nk_text_alignment::NK_TEXT_LEFT, "ModSpeed (%f):", m_modSpeed);
			nk_slider_float(m_nctx, 50, &m_modSpeed, 1500, 0.5);

			nk_labelf(m_nctx, nk_text_alignment::NK_TEXT_LEFT, "Master Volume (%.1f%%):", m_masterVolume * 100);
			nk_slider_float(m_nctx, 0, &m_masterVolume, 1, 0.005);

			nk_layout_row_dynamic(m_nctx, 30, 1);
			nk_checkbox_label(m_nctx, "Use windowed fullscreen", &m_windowFull);

			nk_label(m_nctx, "Anti aliasing (requires restart):", nk_text_alignment::NK_TEXT_LEFT);
			nk_combobox(m_nctx, m_aaModes, 5, &m_antialiasing, buttonheight, comboBoxSize);

			if (m_skins.size() > 0)
			{
				nk_label(m_nctx, "Selected Skin:", nk_text_alignment::NK_TEXT_LEFT);
				nk_combobox(m_nctx, skins.data(), m_skins.size(), &m_selectedSkin, buttonheight, comboBoxSize);
			}

			nk_label(m_nctx, "Laser colors:", nk_text_alignment::NK_TEXT_LEFT);
			nk_layout_row_dynamic(m_nctx, 30, 2);
			if (nk_button_color(m_nctx, lcol))	m_laserColors[1] = fmodf(m_laserColors[0] + 180, 360);
			if (nk_button_color(m_nctx, rcol))	m_laserColors[0] = fmodf(m_laserColors[1] + 180, 360);
			nk_slider_float(m_nctx, 0, m_laserColors, 360, 0.1);
			nk_slider_float(m_nctx, 0, m_laserColors+1, 360, 0.1);

			nk_layout_row_dynamic(m_nctx, 30, 1);
#ifdef _WIN32
			nk_checkbox_label(m_nctx, "WASAPI Exclusive Mode (requires restart)", &m_wasapi);
#endif // _WIN32
			nk_checkbox_label(m_nctx, "Check for updates on startup", &m_update);

			nk_label(m_nctx, "Songs folder path:", nk_text_alignment::NK_TEXT_LEFT);
			nk_edit_string(m_nctx, NK_EDIT_FIELD, m_songsPath, &m_pathlen, 1024, nk_filter_default);
			nk_spacing(m_nctx, 1);
			if (nk_button_label(m_nctx, "Exit")) Exit();
			nk_end(m_nctx);
		}
		nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
	}

	virtual void OnSuspend()
	{
		//g_rootCanvas->Remove(m_canvas.As<GUIElementBase>());
	}
	virtual void OnRestore()
	{
		g_application->DiscordPresenceMenu("Settings");
		//Canvas::Slot* slot = g_rootCanvas->Add(m_canvas.As<GUIElementBase>());
		//slot->anchor = Anchors::Full;
	}
};

SettingsScreen* SettingsScreen::Create()
{
	SettingsScreen_Impl* impl = new SettingsScreen_Impl();
	return impl;
}

class ButtonBindingScreen_Impl : public ButtonBindingScreen
{
private:
	Ref<Gamepad> m_gamepad;
	//Label* m_prompt;
	GameConfigKeys m_key;
	bool m_isGamepad;
	int m_gamepadIndex;
	bool m_completed = false;
	bool m_knobs = false;
	Vector<float> m_gamepadAxes;

public:
	ButtonBindingScreen_Impl(GameConfigKeys key, bool gamepad, int controllerindex)
	{
		m_key = key;
		m_gamepadIndex = controllerindex;
		m_isGamepad = gamepad;
		m_knobs = (key == GameConfigKeys::Controller_Laser0Axis || key == GameConfigKeys::Controller_Laser1Axis);
			
	}

	bool Init()
	{
		if (m_isGamepad)
		{
			m_gamepad = g_gameWindow->OpenGamepad(m_gamepadIndex);
			if (!m_gamepad)
			{
				Logf("Failed to open gamepad: %s", Logger::Error, m_gamepadIndex);
				false;
			}
			if (m_knobs)
			{
				for (size_t i = 0; i < m_gamepad->NumAxes(); i++)
				{
					m_gamepadAxes.Add(m_gamepad->GetAxis(i));
				}
			}
			else
			{
				m_gamepad->OnButtonPressed.Add(this, &ButtonBindingScreen_Impl::OnButtonPressed);
			}
		}
		return true;
	}

	void Tick(float deltatime)
	{
		if (m_knobs && m_isGamepad)
		{
			for (uint8 i = 0; i < m_gamepad->NumAxes(); i++)
			{
				float delta = fabs(m_gamepad->GetAxis(i) - m_gamepadAxes[i]);
				if (delta > 0.3f)
				{
					g_gameConfig.Set(m_key, i);
					m_completed = true;
					break;
				}

			}
		}

		if (m_completed && m_gamepad)
		{
			m_gamepad->OnButtonPressed.RemoveAll(this);
			m_gamepad.Release();

			g_application->RemoveTickable(this);
		}
		else if (m_completed && !m_knobs)
		{
			g_application->RemoveTickable(this);
		}
	}

	void Render(float deltatime)
	{
		String prompt = "Press Key";

		if (m_knobs)
		{
			prompt = "Press Left Key";
			if (m_completed)
			{
				prompt = "Press Right Key";
			}
		}

		if (m_isGamepad)
		{
			prompt = "Press Button";
			m_gamepad = g_gameWindow->OpenGamepad(m_gamepadIndex);
			if (m_knobs)
			{
				prompt = "Turn Knob";
				for (size_t i = 0; i < m_gamepad->NumAxes(); i++)
				{
					m_gamepadAxes.Add(m_gamepad->GetAxis(i));
				}
			}
		}

		g_application->FastText(prompt, g_resolution.x / 2, g_resolution.y / 2, 40, NVGalign::NVG_ALIGN_CENTER | NVGalign::NVG_ALIGN_MIDDLE);
	}

	void OnButtonPressed(uint8 key)
	{
		if (!m_knobs)
		{
			g_gameConfig.Set(m_key, key);
			m_completed = true;
		}
	}

	virtual void OnKeyPressed(int32 key)
	{
		if (!m_isGamepad && !m_knobs)
		{
			g_gameConfig.Set(m_key, key);
			m_completed = true; // Needs to be set because pressing right alt triggers two keypresses on the same frame.
		}
		else if (!m_isGamepad && m_knobs)
		{
			if (!m_completed)
			{
				switch (m_key)
				{
				case GameConfigKeys::Controller_Laser0Axis:
					g_gameConfig.Set(GameConfigKeys::Key_Laser0Neg, key);
					break;
				case GameConfigKeys::Controller_Laser1Axis:
					g_gameConfig.Set(GameConfigKeys::Key_Laser1Neg, key);
					break;
				default:
					break;
				}
				m_completed = true;
			}
			else
			{
				switch (m_key)
				{
				case GameConfigKeys::Controller_Laser0Axis:
					g_gameConfig.Set(GameConfigKeys::Key_Laser0Pos, key);
					break;
				case GameConfigKeys::Controller_Laser1Axis:
					g_gameConfig.Set(GameConfigKeys::Key_Laser1Pos, key);
					break;
				default:
					break;
				}
				g_application->RemoveTickable(this);
			}
		}
	}

	virtual void OnSuspend()
	{
		//g_rootCanvas->Remove(m_canvas.As<GUIElementBase>());
	}
	virtual void OnRestore()
	{
		//Canvas::Slot* slot = g_rootCanvas->Add(m_canvas.As<GUIElementBase>());
		//slot->anchor = Anchors::Full;
	}
};

ButtonBindingScreen* ButtonBindingScreen::Create(GameConfigKeys key, bool gamepad, int controllerIndex)
{
	ButtonBindingScreen_Impl* impl = new ButtonBindingScreen_Impl(key, gamepad, controllerIndex);
	return impl;
}

class LaserSensCalibrationScreen_Impl : public LaserSensCalibrationScreen
{
private:
	Ref<Gamepad> m_gamepad;
	//Label* m_prompt;
	bool m_state = false;
	float m_delta = 0.f;
	float m_currentSetting = 0.f;
	bool m_firstStart = false;
public:
	LaserSensCalibrationScreen_Impl()
	{

	}

	~LaserSensCalibrationScreen_Impl()
	{
		g_input.OnButtonPressed.RemoveAll(this);
	}

	bool Init()
	{
		g_input.GetInputLaserDir(0); //poll because there might be something idk

		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice) == InputDevice::Controller)
			m_currentSetting = g_gameConfig.GetFloat(GameConfigKeys::Controller_Sensitivity);
		else
			m_currentSetting = g_gameConfig.GetFloat(GameConfigKeys::Mouse_Sensitivity);

		g_input.OnButtonPressed.Add(this, &LaserSensCalibrationScreen_Impl::OnButtonPressed);
		return true;
	}

	void Tick(float deltatime)
	{
		m_delta += g_input.GetInputLaserDir(0);

	}

	void Render(float deltatime)
	{
		String prompt = "Press Start Twice";
		if (m_state)
		{
			float sens = 6.0 / (m_delta / m_currentSetting);
			prompt = Utility::Sprintf("Turn left knob one revolution clockwise \nand then press start.\nCurrent Sens: %.2f", sens);

		}
		else
		{
			m_delta = 0;
		}
		g_application->FastText(prompt, g_resolution.x / 2, g_resolution.y / 2, 40, NVGalign::NVG_ALIGN_CENTER | NVGalign::NVG_ALIGN_MIDDLE);
	}

	void OnButtonPressed(Input::Button button)
	{
		if (button == Input::Button::BT_S)
		{
			if (m_firstStart)
			{
				if (m_state)
				{
					// calc sens and then call delagate
					SensSet.Call(6.0 / (m_delta / m_currentSetting));
					g_application->RemoveTickable(this);
				}
				else
				{
					m_delta = 0;
					m_state = !m_state;
				}
			}
			else
			{
				m_firstStart = true;
			}
		}
		else if (button == Input::Button::BT_B)
			g_application->RemoveTickable(this);
	}

	virtual void OnKeyPressed(int32 key)
	{
		if (key == SDLK_ESCAPE)
			g_application->RemoveTickable(this);
	}

	virtual void OnSuspend()
	{
		//g_rootCanvas->Remove(m_canvas.As<GUIElementBase>());
	}
	virtual void OnRestore()
	{
		//Canvas::Slot* slot = g_rootCanvas->Add(m_canvas.As<GUIElementBase>());
		//slot->anchor = Anchors::Full;
	}
};

LaserSensCalibrationScreen* LaserSensCalibrationScreen::Create()
{
	LaserSensCalibrationScreen* impl = new LaserSensCalibrationScreen_Impl();
	return impl;
}