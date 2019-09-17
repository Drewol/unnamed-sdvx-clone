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
#include <SDL2/SDL.h>
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
#define FULL_FONT_TEXTURE_HEIGHT 32768 //needed to load all CJK glyphs

class SettingsScreen_Impl : public SettingsScreen
{
private:
	nk_context* m_nctx;
	nk_font_atlas m_nfonts;

	const char* m_speedMods[3] = { "XMod", "MMod", "CMod" };
	const char* m_laserModes[3] = { "Keyboard", "Mouse", "Controller" };
	const char* m_buttonModes[2] = { "Keyboard", "Controller" };
	Vector<const char*> m_aaModes = { "Off", "2x MSAA", "4x MSAA", "8x MSAA", "16x MSAA" };
	Vector<String> m_gamePads;
	Vector<String> m_skins;

	Vector<GameConfigKeys> m_keyboardKeys = {
		GameConfigKeys::Key_BTS,
		GameConfigKeys::Key_BT0,
		GameConfigKeys::Key_BT1,
		GameConfigKeys::Key_BT2,
		GameConfigKeys::Key_BT3,
		GameConfigKeys::Key_FX0,
		GameConfigKeys::Key_FX1
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
		GameConfigKeys::Controller_FX1
	};

	Vector<GameConfigKeys> m_controllerLaserKeys = {
		GameConfigKeys::Controller_Laser0Axis,
		GameConfigKeys::Controller_Laser1Axis,

	};

	Texture m_whiteTex;


	int m_selectedGamepad = 0;
	float m_laserColors[2] = { 0.25f, 0.75f };
	String m_controllerButtonNames[7];
	String m_controllerLaserNames[2];
	struct nk_vec2 m_comboBoxSize = nk_vec2(1050, 250);
	float m_buttonheight = 30;
	char m_songsPath[1024];
	int m_pathlen = 0;
	char m_multiplayerHost[1024];
	int m_multiplayerHostLen = 0;
	char m_multiplayerPassword[1024];
	int m_multiplayerPasswordLen = 0;
	char m_multiplayerUsername[1024];
	int m_multiplayerUsernameLen = 0;

	std::queue<SDL_Event> eventQueue;

	void UpdateNuklearInput(SDL_Event evt)
	{
		eventQueue.push(evt);
	}

	//TODO: Use argument instead of many functions if possible.



	void SetKey_BTA()
	{
		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Controller)
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_BT0, true, m_selectedGamepad));
		else
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Key_BT0));
	}
	void SetKey_BTB()
	{
		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Controller)
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_BT1, true, m_selectedGamepad));
		else
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Key_BT1));
	}
	void SetKey_BTC()
	{
		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Controller)
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_BT2, true, m_selectedGamepad));
		else
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Key_BT2));
	}
	void SetKey_BTD()
	{
		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Controller)
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_BT3, true, m_selectedGamepad));
		else
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Key_BT3));
	}
	void SetKey_FXL()
	{
		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Controller)
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_FX0, true, m_selectedGamepad));
		else
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Key_FX0));
	}
	void SetKey_FXR()
	{
		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Controller)
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_FX1, true, m_selectedGamepad));
		else
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Key_FX1));
	}
	void SetKey_ST()
	{
		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Controller)
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_BTS, true, m_selectedGamepad));
		else
			g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Key_BTS));
	}

	void SetLL()
	{
		int lasermode = (int)g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice);
		g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_Laser0Axis, lasermode == 2, m_selectedGamepad));
	}
	void SetRL()
	{
		int lasermode = (int)g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice);
		g_application->AddTickable(ButtonBindingScreen::Create(GameConfigKeys::Controller_Laser1Axis, lasermode == 2, m_selectedGamepad));
	}

	void CalibrateSens()
	{
		LaserSensCalibrationScreen* sensScreen = LaserSensCalibrationScreen::Create();
		sensScreen->SensSet.Add(this, &SettingsScreen_Impl::SetSens);
		g_application->AddTickable(sensScreen);
	}

	void SetSens(float sens)
	{
		switch (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice))
		{
		case InputDevice::Controller:
			g_gameConfig.Set(GameConfigKeys::Controller_Sensitivity, sens);
			break;
		case InputDevice::Mouse:
			g_gameConfig.Set(GameConfigKeys::Mouse_Sensitivity, sens);
			break;
		case InputDevice::Keyboard:
		default:
			g_gameConfig.Set(GameConfigKeys::Key_Sensitivity, sens);
			break;
		}
	}

	void Exit()
	{
		g_gameWindow->OnAnyEvent.RemoveAll(this);

		g_gameConfig.Set(GameConfigKeys::Laser0Color, m_laserColors[0]);
		g_gameConfig.Set(GameConfigKeys::Laser1Color, m_laserColors[1]);

		String songsPath = String(m_songsPath, m_pathlen);
		songsPath.TrimBack('\n');
		songsPath.TrimBack(' ');
		g_gameConfig.Set(GameConfigKeys::SongFolder, songsPath);

		String multiplayerHost = String(m_multiplayerHost, m_multiplayerHostLen);
		multiplayerHost.TrimBack('\n');
		multiplayerHost.TrimBack(' ');
		g_gameConfig.Set(GameConfigKeys::MultiplayerHost, multiplayerHost);

		String multiplayerPassword = String(m_multiplayerPassword, m_multiplayerPasswordLen);
		multiplayerPassword.TrimBack('\n');
		multiplayerPassword.TrimBack(' ');
		g_gameConfig.Set(GameConfigKeys::MultiplayerPassword, multiplayerPassword);

		String multiplayerUsername = String(m_multiplayerUsername, m_multiplayerUsernameLen);
		multiplayerUsername.TrimBack('\n');
		multiplayerUsername.TrimBack(' ');
		g_gameConfig.Set(GameConfigKeys::MultiplayerUsername, multiplayerUsername);

		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Mouse)
		{
			g_gameConfig.SetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice, InputDevice::Keyboard);
		}


		g_input.Cleanup();
		g_input.Init(*g_gameWindow);
		g_application->RemoveTickable(this);
	}

	float FloatSetting(GameConfigKeys key, String label, float min, float max, float step = 0.01)
	{
		float value = g_gameConfig.GetFloat(key);
		nk_labelf(m_nctx, nk_text_alignment::NK_TEXT_LEFT, *label, value);
		nk_slider_float(m_nctx, min, &value, max, step);
		g_gameConfig.Set(key, value);
		return value;
	}

	float PercentSetting(GameConfigKeys key, String label)
	{
		float value = g_gameConfig.GetFloat(key);
		nk_labelf(m_nctx, nk_text_alignment::NK_TEXT_LEFT, *label, value * 100);
		nk_slider_float(m_nctx, 0, &value, 1, 0.005);
		g_gameConfig.Set(key, value);
		return value;
	}

	bool ToggleSetting(GameConfigKeys key, String label)
	{
		int value = g_gameConfig.GetBool(key) ? 0 : 1;
		nk_checkbox_label(m_nctx, *label, &value);
		g_gameConfig.Set(key, value == 0);
		return value;
	}

	template<typename EnumClass>
	typename EnumClass::EnumType EnumSetting(GameConfigKeys key, String label)
	{
		
		EnumStringMap<typename EnumClass::EnumType> nameMap = EnumClass::GetMap();
		Vector<const char*> names;
		int value = (int)g_gameConfig.GetEnum<EnumClass>(key);

		for (auto it = nameMap.begin(); it != nameMap.end(); it++)
		{
			names.Add(*(*it).second);
		}

		nk_label(m_nctx, *label, nk_text_alignment::NK_TEXT_LEFT);
		nk_combobox(m_nctx, names.data(), names.size(), &value, m_buttonheight, m_comboBoxSize);
		g_gameConfig.SetEnum<EnumClass>(key, nameMap.FromString(names[value]));
		return nameMap.FromString(names[value]);
	}

	int SelectionSetting(GameConfigKeys key, Vector<const char*> options, String label)
	{
		int value = g_gameConfig.GetInt(key) % options.size();
		nk_label(m_nctx, *label, nk_text_alignment::NK_TEXT_LEFT);
		nk_combobox(m_nctx, options.data(), options.size(), &value, m_buttonheight, m_comboBoxSize);
		g_gameConfig.Set(key, value);
		return value;
	}

	String StringSelectionSetting(GameConfigKeys key, Vector<String> options, String label)
	{
		String value = g_gameConfig.GetString(key);
		int selection;
		auto stringSearch = std::find(options.begin(), options.end(), value);
		if (stringSearch != options.end())
			selection = stringSearch - options.begin();
		else
			selection = 0;

		Vector<const char*> displayData;
		for (String& s : options)
		{
			displayData.Add(*s);
		}

		nk_label(m_nctx, *label, nk_text_alignment::NK_TEXT_LEFT);
		nk_combobox(m_nctx, displayData.data(), options.size(), &selection, m_buttonheight, m_comboBoxSize);
		value = options[selection];
		g_gameConfig.Set(key, value);
		return value;
	}

	int IntSetting(GameConfigKeys key, String label, int min, int max, int step = 1, int perpixel = 1)
	{
		int value = g_gameConfig.GetInt(key);
		value = nk_propertyi(m_nctx, *label, min, value, max, step, perpixel);
		g_gameConfig.Set(key, value);
		return value;
	}


public:
	~SettingsScreen_Impl()
	{
		nk_sdl_shutdown();
		g_application->ApplySettings();
	}


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
			struct nk_font *fallback = nk_font_atlas_add_from_file(atlas, Path::Normalize( Path::Absolute("fonts/settings/NotoSans-Regular.ttf")).c_str(), 24, 0);

			struct nk_font_config cfg_kr = nk_font_config(24);
			cfg_kr.merge_mode = nk_true;
			cfg_kr.range = nk_font_korean_glyph_ranges();

			NK_STORAGE const nk_rune jp_ranges[] = {
				0x0020, 0x00FF,
				0x3000, 0x303f,
				0x3040, 0x309f,
				0x30a0, 0x30ff,
				0x4e00, 0x9faf,
				0xff00, 0xffef,
				0
			};
			struct nk_font_config cfg_jp = nk_font_config(24);
			cfg_jp.merge_mode = nk_true;
			cfg_jp.range = jp_ranges;

			NK_STORAGE const nk_rune cjk_ranges[] = {
				0x0020, 0x00FF,
				0x3000, 0x30FF,
				0x3131, 0x3163,
				0xAC00, 0xD79D,
				0x31F0, 0x31FF,
				0xFF00, 0xFFEF,
				0x4e00, 0x9FAF,
				0
			};

			struct nk_font_config cfg_cjk = nk_font_config(24);
			cfg_cjk.merge_mode = nk_true;
			cfg_cjk.range = cjk_ranges;


			//nk_font_atlas_add_from_file(atlas, Path::Normalize("fonts/settings/NanumBarunGothic.ttf").c_str(), 24, &cfg_kr);
			//nk_font_atlas_add_from_file(atlas, Path::Normalize("fonts/settings/mplus-1m-medium.ttf").c_str(), 24, &cfg_jp);
			int maxSize;
			glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
			Logf("System max texture size: %d", Logger::Info, maxSize);
			if (maxSize >= FULL_FONT_TEXTURE_HEIGHT)
			{
				nk_font_atlas_add_from_file(atlas, Path::Normalize(Path::Absolute("fonts/settings/DroidSansFallback.ttf")).c_str(), 24, &cfg_cjk);
			}
			
			nk_sdl_font_stash_end();
			nk_font_atlas_cleanup(atlas);
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


		m_laserColors[0] = g_gameConfig.GetFloat(GameConfigKeys::Laser0Color);
		m_laserColors[1] = g_gameConfig.GetFloat(GameConfigKeys::Laser1Color);

		String songspath = g_gameConfig.GetString(GameConfigKeys::SongFolder);
		strcpy(m_songsPath, songspath.c_str());
		m_pathlen = songspath.length();

		String multiplayerHost = g_gameConfig.GetString(GameConfigKeys::MultiplayerHost);
		strcpy(m_multiplayerHost, multiplayerHost.c_str());
		m_multiplayerHostLen = multiplayerHost.length();

		String multiplayerPassword = g_gameConfig.GetString(GameConfigKeys::MultiplayerPassword);
		strcpy(m_multiplayerPassword, multiplayerPassword.c_str());
		m_multiplayerPasswordLen = multiplayerPassword.length();

		String multiplayerUsername = g_gameConfig.GetString(GameConfigKeys::MultiplayerUsername);
		strcpy(m_multiplayerUsername, multiplayerUsername.c_str());
		m_multiplayerUsernameLen = multiplayerUsername.length();

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

		for (size_t i = 0; i < 7; i++)
		{
			if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Controller)
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
			if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice) == InputDevice::Controller)
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


		Vector<const char*> pads;

		for (size_t i = 0; i < m_gamePads.size(); i++)
		{
			pads.Add(m_gamePads[i].GetData());
		}

		nk_color lcol = nk_hsv_f(m_laserColors[0] / 360, 1, 1);
		nk_color rcol = nk_hsv_f(m_laserColors[1] / 360, 1, 1);

		float w = Math::Min(g_resolution.y / 1.4, g_resolution.x - 5.0);
		float x = g_resolution.x / 2 - w / 2;
		m_comboBoxSize = nk_vec2(w - 30, 250);

		if (nk_begin(m_nctx, "Settings", nk_rect(x, 0, w, g_resolution.y), 0))
		{

			nk_layout_row_dynamic(m_nctx, m_buttonheight, 3);
			if (nk_button_label(m_nctx, m_controllerLaserNames[0].c_str())) SetLL();
			if (nk_button_label(m_nctx, m_controllerButtonNames[0].c_str())) SetKey_ST();
			if (nk_button_label(m_nctx, m_controllerLaserNames[1].c_str())) SetRL();

			nk_layout_row_dynamic(m_nctx, m_buttonheight, 4);
			if (nk_button_label(m_nctx, m_controllerButtonNames[1].c_str())) SetKey_BTA();
			if (nk_button_label(m_nctx, m_controllerButtonNames[2].c_str())) SetKey_BTB();
			if (nk_button_label(m_nctx, m_controllerButtonNames[3].c_str())) SetKey_BTC();
			if (nk_button_label(m_nctx, m_controllerButtonNames[4].c_str())) SetKey_BTD();
			nk_layout_row_dynamic(m_nctx, m_buttonheight, 2);
			if (nk_button_label(m_nctx, m_controllerButtonNames[5].c_str())) SetKey_FXL();
			if (nk_button_label(m_nctx, m_controllerButtonNames[6].c_str())) SetKey_FXR();

			nk_layout_row_dynamic(m_nctx, m_buttonheight, 1);
			if (nk_button_label(m_nctx, "Calibrate Laser Sensitivity")) CalibrateSens();

			GameConfigKeys laserSensKey;
			switch (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice))
			{
			case InputDevice::Controller:
				laserSensKey = GameConfigKeys::Controller_Sensitivity;
				break;
			case InputDevice::Mouse:
				laserSensKey = GameConfigKeys::Mouse_Sensitivity;
				break;
			case InputDevice::Keyboard:
			default:
				laserSensKey = GameConfigKeys::Key_Sensitivity;
				break;
			}

			FloatSetting(laserSensKey, "Laser sensitivity (%f):", 0, 20, 0.001);
			EnumSetting<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice, "Button input mode:");
			EnumSetting<Enum_InputDevice>(GameConfigKeys::LaserInputDevice, "Laser input mode:");

			if (m_gamePads.size() > 0)
			{
				m_selectedGamepad = SelectionSetting(GameConfigKeys::Controller_DeviceID, pads, "Selected Controller:");
			}

			IntSetting(GameConfigKeys::GlobalOffset, "Global Offset:", -1000, 1000);
			IntSetting(GameConfigKeys::InputOffset, "Input Offset:", -1000, 1000);
			IntSetting(GameConfigKeys::InputBounceGuard, "Button Bounce Guard:", 0, 100);
			ToggleSetting(GameConfigKeys::SkipScore, "Skip score screen on manual exit");


			EnumSetting<Enum_SpeedMods>(GameConfigKeys::SpeedMod, "Speed mod:");

			FloatSetting(GameConfigKeys::HiSpeed, "HiSpeed (%f):", 0.25, 20, 0.05);
			FloatSetting(GameConfigKeys::ModSpeed, "ModSpeed (%f):", 50, 1500, 0.5);
			PercentSetting(GameConfigKeys::MasterVolume, "Master Volume (%.1f%%):");

			nk_layout_row_dynamic(m_nctx, 30, 1);
			ToggleSetting(GameConfigKeys::WindowedFullscreen, "Use windowed fullscreen");
			ToggleSetting(GameConfigKeys::ForcePortrait, "Force portrait rendering (don't use if already in portrait)");
			ToggleSetting(GameConfigKeys::VSync, "VSync");
			ToggleSetting(GameConfigKeys::ShowFps, "Show FPS");

			SelectionSetting(GameConfigKeys::AntiAliasing, m_aaModes, "Anti aliasing (requires restart):");

			if (m_skins.size() > 0)
			{
				StringSelectionSetting(GameConfigKeys::Skin, m_skins, "Selected Skin:");
			}

			nk_label(m_nctx, "Laser colors:", nk_text_alignment::NK_TEXT_LEFT);
			nk_layout_row_dynamic(m_nctx, 30, 2);
			if (nk_button_color(m_nctx, lcol))	m_laserColors[1] = fmodf(m_laserColors[0] + 180, 360);
			if (nk_button_color(m_nctx, rcol))	m_laserColors[0] = fmodf(m_laserColors[1] + 180, 360);
			nk_slider_float(m_nctx, 0, m_laserColors, 360, 0.1);
			nk_slider_float(m_nctx, 0, m_laserColors+1, 360, 0.1);

			nk_layout_row_dynamic(m_nctx, 30, 1);

			EnumSetting<Enum_AutoScoreScreenshotSettings>(GameConfigKeys::AutoScoreScreenshot, "Automatically capture score screenshots:");

#ifdef _WIN32
			ToggleSetting(GameConfigKeys::WASAPI_Exclusive, "WASAPI Exclusive Mode (requires restart)");
#endif // _WIN32
			ToggleSetting(GameConfigKeys::CheckForUpdates, "Check for updates on startup");
			ToggleSetting(GameConfigKeys::OnlyRelease, "Only check for new release versions");

			nk_label(m_nctx, "Songs folder path:", nk_text_alignment::NK_TEXT_LEFT);
			nk_edit_string(m_nctx, NK_EDIT_FIELD, m_songsPath, &m_pathlen, 1024, nk_filter_default);

			nk_label(m_nctx, "Multiplayer Server:", nk_text_alignment::NK_TEXT_LEFT);
			nk_edit_string(m_nctx, NK_EDIT_FIELD, m_multiplayerHost, &m_multiplayerHostLen, 1024, nk_filter_default);

			nk_label(m_nctx, "Multiplayer Server Username:", nk_text_alignment::NK_TEXT_LEFT);
			nk_edit_string(m_nctx, NK_EDIT_FIELD, m_multiplayerUsername, &m_multiplayerUsernameLen, 1024, nk_filter_default);

			nk_label(m_nctx, "Multiplayer Server Password:", nk_text_alignment::NK_TEXT_LEFT);
			nk_edit_string(m_nctx, NK_EDIT_FIELD, m_multiplayerPassword, &m_multiplayerPasswordLen, 1024, nk_filter_default);

			nk_spacing(m_nctx, 1);
			if (nk_button_label(m_nctx, "Skin Settings"))
			{
				g_application->AddTickable(new SkinSettingsScreen(g_gameConfig.GetString(GameConfigKeys::Skin), m_nctx));
			}
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
		if (m_state)
		{
			float sens = 6.0 / (m_delta / m_currentSetting);
			
			g_application->FastText("Turn left knob one revolution clockwise", g_resolution.x / 2, g_resolution.y / 2, 40, NVGalign::NVG_ALIGN_CENTER | NVGalign::NVG_ALIGN_MIDDLE);
			g_application->FastText("then press start.", g_resolution.x / 2, g_resolution.y / 2 + 45, 40, NVGalign::NVG_ALIGN_CENTER | NVGalign::NVG_ALIGN_MIDDLE);
			g_application->FastText(Utility::Sprintf("Current Sens: %.2f", sens), g_resolution.x / 2, g_resolution.y / 2 + 90, 40, NVGalign::NVG_ALIGN_CENTER | NVGalign::NVG_ALIGN_MIDDLE);

		}
		else
		{
			m_delta = 0;
			g_application->FastText("Press start twice", g_resolution.x / 2, g_resolution.y / 2, 40, NVGalign::NVG_ALIGN_CENTER | NVGalign::NVG_ALIGN_MIDDLE);
		}
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




void SkinSettingsScreen::StringSelectionSetting(String key, String label, SkinSetting& setting)
{
	float w = Math::Min(g_resolution.y / 1.4, g_resolution.x - 5.0);

	String value = m_skinConfig->GetString(key);
	int selection;
	String* options = setting.selectionSetting.options;
	auto stringSearch = std::find(options, options + setting.selectionSetting.numOptions, value);
	if (stringSearch != options + setting.selectionSetting.numOptions)
		selection = (stringSearch - options);
	else
		selection = 0;

	Vector<const char*> displayData;
	for (size_t i = 0; i < setting.selectionSetting.numOptions; i++)
	{
		displayData.Add(*setting.selectionSetting.options[i]);
	}

	nk_label(m_nctx, *label, nk_text_alignment::NK_TEXT_LEFT);
	nk_combobox(m_nctx, displayData.data(), setting.selectionSetting.numOptions, &selection, 30, nk_vec2(w - 30, 250));
	value = options[selection];
	m_skinConfig->Set(key, value);
}

void SkinSettingsScreen::Exit()
{
	g_application->RemoveTickable(this);
}

void SkinSettingsScreen::IntSetting(String key, String label, int min, int max, int step, int perpixel)
{
	int value = m_skinConfig->GetInt(key);
	value = nk_propertyi(m_nctx, *label, min, value, max, step, perpixel);
	m_skinConfig->Set(key, value);
}

float SkinSettingsScreen::FloatSetting(String key, String label, float min, float max, float step)
{
	float value = m_skinConfig->GetFloat(key);
	nk_labelf(m_nctx, nk_text_alignment::NK_TEXT_LEFT, *label, value);
	nk_slider_float(m_nctx, min, &value, max, step);
	m_skinConfig->Set(key, value);
	return value;
}

float SkinSettingsScreen::PercentSetting(String key, String label)
{
	float value = m_skinConfig->GetFloat(key);
	nk_labelf(m_nctx, nk_text_alignment::NK_TEXT_LEFT, *label, value * 100);
	nk_slider_float(m_nctx, 0, &value, 1, 0.005);
	m_skinConfig->Set(key, value);
	return value;
}

void SkinSettingsScreen::TextSetting(String key, String label)
{
	String value = m_skinConfig->GetString(key);
	char display[1024];
	strcpy(display, value.c_str());
	int len = value.length();
	nk_label(m_nctx, *label, nk_text_alignment::NK_TEXT_LEFT);
	nk_edit_string(m_nctx, NK_EDIT_FIELD, display, &len, 1024, nk_filter_default);
	value = String(display, len);
	m_skinConfig->Set(key, value);
}

void SkinSettingsScreen::ColorSetting(String key, String label)
{
	float w = Math::Min(g_resolution.y / 1.4, g_resolution.x - 5.0) - 100;
	Color value = m_skinConfig->GetColor(key);
	nk_label(m_nctx, *label, nk_text_alignment::NK_TEXT_LEFT);
	float r, g, b, a;
	r = value.x;
	g = value.y;
	b = value.z;
	a = value.w;
	nk_colorf nkCol = { r,g,b,a };
	if (nk_combo_begin_color(m_nctx, nk_rgb_cf(nkCol), nk_vec2(200, 400))) {
		enum color_mode { COL_RGB, COL_HSV };
		nk_layout_row_dynamic(m_nctx, 120, 1);
		nkCol = nk_color_picker(m_nctx, nkCol, NK_RGBA);

		nk_layout_row_dynamic(m_nctx, 25, 2);
		m_hsvMap[key] = nk_option_label(m_nctx, "RGB", m_hsvMap[key] ? 1 : 0) == 1;
		m_hsvMap[key] = nk_option_label(m_nctx, "HSV", m_hsvMap[key] ? 0 : 1) == 0;

		nk_layout_row_dynamic(m_nctx, 25, 1);
		if (!m_hsvMap[key]) {
			nkCol.r = nk_propertyf(m_nctx, "#R:", 0, nkCol.r, 1.0f, 0.01f, 0.005f);
			nkCol.g = nk_propertyf(m_nctx, "#G:", 0, nkCol.g, 1.0f, 0.01f, 0.005f);
			nkCol.b = nk_propertyf(m_nctx, "#B:", 0, nkCol.b, 1.0f, 0.01f, 0.005f);
			nkCol.a = nk_propertyf(m_nctx, "#A:", 0, nkCol.a, 1.0f, 0.01f, 0.005f);
		}
		else {
			float hsva[4];
			nk_colorf_hsva_fv(hsva, nkCol);
			hsva[0] = nk_propertyf(m_nctx, "#H:", 0, hsva[0], 1.0f, 0.01f, 0.05f);
			hsva[1] = nk_propertyf(m_nctx, "#S:", 0, hsva[1], 1.0f, 0.01f, 0.05f);
			hsva[2] = nk_propertyf(m_nctx, "#V:", 0, hsva[2], 1.0f, 0.01f, 0.05f);
			hsva[3] = nk_propertyf(m_nctx, "#A:", 0, hsva[3], 1.0f, 0.01f, 0.05f);
			nkCol = nk_hsva_colorfv(hsva);
		}
		nk_combo_end(m_nctx);
	}
	m_skinConfig->Set(key, Color(nkCol.r, nkCol.g, nkCol.b, nkCol.a));


	nk_layout_row_dynamic(m_nctx, 30, 1);

}

bool SkinSettingsScreen::ToggleSetting(String key, String label)
{
	int value = m_skinConfig->GetBool(key) ? 0 : 1;
	nk_checkbox_label(m_nctx, *label, &value);
	m_skinConfig->Set(key, value == 0);
	return value;
}

SkinSettingsScreen::SkinSettingsScreen(String skin, nk_context* ctx)
{
	m_nctx = ctx;
	m_skin = skin;
	if (skin == g_application->GetCurrentSkin())
	{
		m_skinConfig = g_skinConfig;
	}
	else
	{
		m_skinConfig = new SkinConfig(skin);
	}
}

SkinSettingsScreen::~SkinSettingsScreen()
{
	if (m_skinConfig != g_skinConfig && m_skinConfig)
	{
		delete m_skinConfig;
		m_skinConfig = nullptr;
	}
}

void SkinSettingsScreen::Tick(float deltatime)
{

}

void SkinSettingsScreen::Render(float deltaTime)
{
	float w = Math::Min(g_resolution.y / 1.4, g_resolution.x - 5.0);
	float x = g_resolution.x / 2 - w / 2;
	if (nk_begin(m_nctx, *Utility::Sprintf("%s Settings", m_skin), nk_rect(x, 0, w, g_resolution.y), 0))
	{
		nk_layout_row_dynamic(m_nctx, 30, 1);
		nk_labelf(m_nctx, nk_text_alignment::NK_TEXT_CENTERED, "%s Skin Settings", *m_skin);
		nk_labelf(m_nctx, nk_text_alignment::NK_TEXT_CENTERED, "_______________________");
		for (auto s : m_skinConfig->GetSettings())
		{
			if (s.type == SkinSetting::Type::Boolean)
			{
				ToggleSetting(s.key, s.label);
			}
			else if (s.type == SkinSetting::Type::Selection)
			{
				StringSelectionSetting(s.key, s.label, s);
			}
			else if (s.type == SkinSetting::Type::Float)
			{
				FloatSetting(s.key, s.label + " (%.2f):", s.floatSetting.min, s.floatSetting.max);
			}
			else if (s.type == SkinSetting::Type::Integer)
			{
				IntSetting(s.key, s.label, s.intSetting.min, s.intSetting.max);
			}
			else if (s.type == SkinSetting::Type::Label)
			{
				nk_label(m_nctx, *s.key, nk_text_alignment::NK_TEXT_LEFT);
			}
			else if (s.type == SkinSetting::Type::Separator)
			{
				nk_labelf(m_nctx, nk_text_alignment::NK_TEXT_CENTERED, "_______________________");
			}
			else if (s.type == SkinSetting::Type::Text)
			{
				TextSetting(s.key, s.label);
			}
			else if (s.type == SkinSetting::Type::Color)
			{
				ColorSetting(s.key, s.label);
			}
		}
		if (nk_button_label(m_nctx, "Exit")) Exit();
		nk_end(m_nctx);
	}
	nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
}
