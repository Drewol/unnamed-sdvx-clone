#include "stdafx.h"
#include "SettingsScreen.hpp"

#include "Application.hpp"
#include "CalibrationScreen.hpp"

#include "SettingsPage.hpp"
#include "SkinConfig.hpp"
#include "TransitionScreen.hpp"

static inline const char* GetKeyNameFromScancodeConfig(int scancode)
{
	return SDL_GetKeyName(SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(scancode)));
}

static const String GetKeyNameFromConfigKey(GameConfigKeys key) {
	// Oh, you like SDVX? Name every possible key bindings for it!
	switch (key) {
		case GameConfigKeys::Controller_BTS:
		case GameConfigKeys::Key_BTS:
		case GameConfigKeys::Key_BTSAlt:
			return "Start";
		case GameConfigKeys::Controller_Back:
		case GameConfigKeys::Key_Back:
		case GameConfigKeys::Key_BackAlt:
			return "Back";

		case GameConfigKeys::Controller_BT0:
		case GameConfigKeys::Key_BT0:
		case GameConfigKeys::Key_BT0Alt:
			return "BT-A";
		case GameConfigKeys::Controller_BT1:
		case GameConfigKeys::Key_BT1:
		case GameConfigKeys::Key_BT1Alt:
			return "BT-B";
		case GameConfigKeys::Controller_BT2:
		case GameConfigKeys::Key_BT2:
		case GameConfigKeys::Key_BT2Alt:
			return "BT-C";
		case GameConfigKeys::Controller_BT3:
		case GameConfigKeys::Key_BT3:
		case GameConfigKeys::Key_BT3Alt:
			return "BT-D";

		case GameConfigKeys::Controller_FX0:
		case GameConfigKeys::Key_FX0:
		case GameConfigKeys::Key_FX0Alt:
			return "FX-L";
		case GameConfigKeys::Controller_FX1:
		case GameConfigKeys::Key_FX1:
		case GameConfigKeys::Key_FX1Alt:
			return "FX-R";

		case GameConfigKeys::Controller_Laser0Axis:
			return "Left Laser";
		case GameConfigKeys::Controller_Laser1Axis:
			return "Right Laser";

		case GameConfigKeys::Key_SongSelect_Up:
			return "Previous song";
		case GameConfigKeys::Key_SongSelect_Down:
			return "Next song";
		case GameConfigKeys::Key_SongSelect_FastUp:
			return "Previous song (jump)";
		case GameConfigKeys::Key_SongSelect_FastDown:
			return "Next song (jump)";
		case GameConfigKeys::Key_SongSelect_Easier:
			return "Difficulty easier";
		case GameConfigKeys::Key_SongSelect_Harder:
			return "Difficulty header";
		case GameConfigKeys::Key_SongSelect_Collections:
			return "Edit song collections";
		case GameConfigKeys::Key_SongSelect_Random:
			return "Random song";
		case GameConfigKeys::Key_SongSelect_ReloadSongs:
			return "Reload song database";
		case GameConfigKeys::Key_SongSelect_Demo:
			return "Start demo mode";
		case GameConfigKeys::Key_SongSelect_OpenEditor:
			return "Open song in editor";
		case GameConfigKeys::Key_SongSelect_OpenDirectory:
			return "Open songs directory";
		case GameConfigKeys::Key_SongSelect_OpenSearch:
			return "Open songs search";
		case GameConfigKeys::Key_SongSelect_CloseSearch:
			return "Close songs search";
		case GameConfigKeys::Key_SongSelect_StartPractice:
			return "Start song in practice mode";

		case GameConfigKeys::Key_Game_Bailout:
			return "End practice mode";
		case GameConfigKeys::Key_Game_Pause:
			return "Pause";
		case GameConfigKeys::Key_Game_SkipIntro:
			return "Skip intro/outro";
		case GameConfigKeys::Key_Game_Advance:
			return "Jump 5s forwards";
		case GameConfigKeys::Key_Game_Restart:
			return "Restart";
		case GameConfigKeys::Key_Game_DebugHUD:
			return "Toggle debug HUD";

		case GameConfigKeys::Key_Result_Continue:
			return "Return to song select";
		case GameConfigKeys::Key_Result_Screenshot:
			return "Save screenshot";

		case GameConfigKeys::Key_ReloadSkin:
			return "Reload skin";
		case GameConfigKeys::Key_ScrollLeft:
			return "Scroll left";
		case GameConfigKeys::Key_ScrollRight:
			return "Scroll right";
		case GameConfigKeys::Key_ScrollUp:
			return "Scroll up";
		case GameConfigKeys::Key_ScrollDown:
			return "Scroll down";

		default:
			return Enum_GameConfigKeys::ToString(key);
	}
}

class SettingsPage_Input : public SettingsPage
{
public:
	SettingsPage_Input(nk_context* nctx) : SettingsPage(nctx, "Input") {}

protected:
	void Load() override
	{
		m_gamePads.clear();

		m_gamePadsStr = g_gameWindow->GetGamepadDeviceNames();
		for (const String& gamePadName : m_gamePadsStr)
		{
			m_gamePads.Add(gamePadName.data());
		}

		//Look for selected gamepad and if none are set use 0
		m_selectedPad = 0;
		const auto deviceId = g_gameConfig.GetBlob<16>(GameConfigKeys::Controller_DeviceID);
		for (size_t i = 0; i < m_gamePads.size(); i++)
		{
			auto id = SDL_JoystickGetDeviceGUID(i);
			if (memcmp(deviceId.data(), id.data, 16) == 0)
			{
				m_selectedPad = i;
				break;
			}
		}


	}

	void Save() override
	{
		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Mouse)
		{
			g_gameConfig.SetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice, InputDevice::Keyboard);
		}

		if (!m_gamePads.empty())
		{
			std::array<uint8, 16> newId;
			memcpy(newId.data(), SDL_JoystickGetDeviceGUID(m_selectedPad).data, 16);
			g_gameConfig.SetBlob<16>(GameConfigKeys::Controller_DeviceID, newId);
		}
	}

	Vector<String> m_gamePadsStr;
	Vector<const char*> m_gamePads;
	int m_selectedPad;

	String m_controllerButtonNames[8];
	String m_controllerLaserNames[2];

	const Vector<GameConfigKeys>* m_activeBTKeys = &m_keyboardKeys;
	const Vector<GameConfigKeys>* m_activeLaserKeys = &m_keyboardLaserKeys;
	bool m_useBTGamepad = false;
	bool m_useLaserGamepad = false;
	bool m_altBinds = false;

	const Vector<GameConfigKeys>* m_activeSongSelectKeys = &m_songSelectKeys;
	const Vector<GameConfigKeys>* m_activeGameKeys = &m_gameKeys;
	const Vector<GameConfigKeys>* m_activeResultKeys = &m_resultKeys;
	const Vector<GameConfigKeys>* m_activeMiscKeys = &m_miscKeys;

	const Vector<GameConfigKeys> m_keyboardKeys = {
		GameConfigKeys::Key_BTS,
		GameConfigKeys::Key_BT0,
		GameConfigKeys::Key_BT1,
		GameConfigKeys::Key_BT2,
		GameConfigKeys::Key_BT3,
		GameConfigKeys::Key_FX0,
		GameConfigKeys::Key_FX1,
		GameConfigKeys::Key_Back
	};
	const Vector<GameConfigKeys> m_altKeyboardKeys = {
		GameConfigKeys::Key_BTSAlt,
		GameConfigKeys::Key_BT0Alt,
		GameConfigKeys::Key_BT1Alt,
		GameConfigKeys::Key_BT2Alt,
		GameConfigKeys::Key_BT3Alt,
		GameConfigKeys::Key_FX0Alt,
		GameConfigKeys::Key_FX1Alt,
		GameConfigKeys::Key_BackAlt
	};
	const Vector<GameConfigKeys> m_songSelectKeys = {
		GameConfigKeys::Key_SongSelect_Up,
		GameConfigKeys::Key_SongSelect_Down,
		GameConfigKeys::Key_SongSelect_FastUp,
		GameConfigKeys::Key_SongSelect_FastDown,
		GameConfigKeys::Key_SongSelect_Easier,
		GameConfigKeys::Key_SongSelect_Harder,
		GameConfigKeys::Key_SongSelect_Collections,
		GameConfigKeys::Key_SongSelect_Random,
		GameConfigKeys::Key_SongSelect_ReloadSongs,
		GameConfigKeys::Key_SongSelect_Demo,
		GameConfigKeys::Key_SongSelect_OpenEditor,
		GameConfigKeys::Key_SongSelect_OpenDirectory,
		GameConfigKeys::Key_SongSelect_OpenSearch,
		GameConfigKeys::Key_SongSelect_CloseSearch,
		GameConfigKeys::Key_SongSelect_StartPractice
	};
	const Vector<GameConfigKeys> m_gameKeys = {
		GameConfigKeys::Key_Game_Bailout,
		GameConfigKeys::Key_Game_Pause,
		GameConfigKeys::Key_Game_SkipIntro,
		GameConfigKeys::Key_Game_Advance,
		GameConfigKeys::Key_Game_Restart,
		GameConfigKeys::Key_Game_DebugHUD
	};
	const Vector<GameConfigKeys> m_resultKeys = {
		GameConfigKeys::Key_Result_Continue,
		GameConfigKeys::Key_Result_Screenshot
	};
	const Vector<GameConfigKeys> m_miscKeys = {
		GameConfigKeys::Key_ReloadSkin,
		GameConfigKeys::Key_ScrollLeft,
		GameConfigKeys::Key_ScrollRight,
		GameConfigKeys::Key_ScrollUp,
		GameConfigKeys::Key_ScrollDown,
	};

	const Vector<GameConfigKeys> m_keyboardLaserKeys = {
		GameConfigKeys::Key_Laser0Neg,
		GameConfigKeys::Key_Laser0Pos,
		GameConfigKeys::Key_Laser1Neg,
		GameConfigKeys::Key_Laser1Pos,
	};
	const Vector<GameConfigKeys> m_altKeyboardLaserKeys = {
		GameConfigKeys::Key_Laser0NegAlt,
		GameConfigKeys::Key_Laser0PosAlt,
		GameConfigKeys::Key_Laser1NegAlt,
		GameConfigKeys::Key_Laser1PosAlt,
	};

	const Vector<GameConfigKeys> m_controllerKeys = {
		GameConfigKeys::Controller_BTS,
		GameConfigKeys::Controller_BT0,
		GameConfigKeys::Controller_BT1,
		GameConfigKeys::Controller_BT2,
		GameConfigKeys::Controller_BT3,
		GameConfigKeys::Controller_FX0,
		GameConfigKeys::Controller_FX1,
		GameConfigKeys::Controller_Back
	};
	const Vector<GameConfigKeys> m_controllerLaserKeys = {
		GameConfigKeys::Controller_Laser0Axis,
		GameConfigKeys::Controller_Laser1Axis,
	};

	void RenderContents() override
	{
		UpdateInputKeyBindingStatus();
		UpdateControllerInputNames();

		SectionHeader("Key Bindings");

		RenderKeyBindings();

		SectionHeader("Input Device");

		if (m_gamePads.size() > 0)
		{
			int newPad = SelectionInput(m_selectedPad, m_gamePads, "Controller to use:");
			if (newPad != m_selectedPad)
			{
				m_selectedPad = newPad;
				std::array<uint8, 16> newId;
				memcpy(newId.data(), SDL_JoystickGetDeviceGUID(m_selectedPad).data, 16);
				g_gameConfig.SetBlob<16>(GameConfigKeys::Controller_DeviceID, newId);
			}
		}

		LayoutRowDynamic(2, m_lineHeight * 6);

		if (nk_group_begin(m_nctx, "Button Input", NK_WINDOW_NO_SCROLLBAR))
		{
			LayoutRowDynamic(1);
			EnumSetting<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice, "Button input mode:");
			IntSetting(GameConfigKeys::InputBounceGuard, "Bounce guard:", 0, 100);
			EnumSetting<Enum_ButtonComboModeSettings>(GameConfigKeys::UseBackCombo, "Use 3xBT+Start for Back:");

			nk_group_end(m_nctx);
		}
		if (nk_group_begin(m_nctx, "Laser Input", NK_WINDOW_NO_SCROLLBAR))
		{
			LayoutRowDynamic(1);
			EnumSetting<Enum_InputDevice>(GameConfigKeys::LaserInputDevice, "Laser input mode:");

			if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice) == InputDevice::Mouse)
			{
				const bool mouseAxisFlipped = g_gameConfig.GetInt(GameConfigKeys::Mouse_Laser0Axis) != 0;
				if (mouseAxisFlipped != ToggleInput(mouseAxisFlipped, "Flip mouse axes"))
				{
					g_gameConfig.Set(GameConfigKeys::Mouse_Laser0Axis, mouseAxisFlipped ? 0 : 1);
					g_gameConfig.Set(GameConfigKeys::Mouse_Laser1Axis, mouseAxisFlipped ? 1 : 0);
				}
			}

			EnumSetting<Enum_LaserAxisOption>(GameConfigKeys::InvertLaserInput, "Invert directions:");
			nk_group_end(m_nctx);
		}

		SectionHeader("Laser Sensitivity");
		RenderLaserSensitivitySettings();

		SectionHeader("In-Game Key Inputs");

		EnumSetting<Enum_AbortMethod>(GameConfigKeys::RestartPlayMethod, "Restart with F5:");
		if (g_gameConfig.GetEnum<Enum_AbortMethod>(GameConfigKeys::RestartPlayMethod) == AbortMethod::Hold)
		{
			IntSetting(GameConfigKeys::RestartPlayHoldDuration, "Restart hold duration (ms):", 250, 10000, 250);
		}

		EnumSetting<Enum_AbortMethod>(GameConfigKeys::ExitPlayMethod, "Exit gameplay with Back:");
		if (g_gameConfig.GetEnum<Enum_AbortMethod>(GameConfigKeys::ExitPlayMethod) == AbortMethod::Hold)
		{
			IntSetting(GameConfigKeys::ExitPlayHoldDuration, "Restart hold duration (ms):", 250, 10000, 250);
		}

		ToggleSetting(GameConfigKeys::DisableNonButtonInputsDuringPlay, "Disable non-buttons during gameplay");
		ToggleSetting(GameConfigKeys::PracticeSetupNavEnabled, "Enable navigation inputs for the practice setup");

		SectionHeader("UI Controls");
		RenderUIControlsSettings();
	}

private:
	inline void RenderKeyBindingsLaserButton(int index)
	{
		const float laserHue = g_gameConfig.GetFloat(index == 0 ? GameConfigKeys::Laser0Color : GameConfigKeys::Laser1Color) / 360;

		m_nctx->style.button.normal = nk_style_item_color(nk_hsv_f(laserHue, 1.0f, 1.0f));
		m_nctx->style.button.hover = nk_style_item_color(nk_hsv_f(laserHue, 0.5f, 1.0f));
		m_nctx->style.button.text_normal = nk_rgb(0, 0, 0);
		m_nctx->style.button.text_hover = nk_rgb(0, 0, 0);
		m_nctx->style.button.border_color = nk_hsv_f(laserHue, 0.5f, 0.5f);

		if (nk_button_label(m_nctx, m_controllerLaserNames[index].data()))
		{
			OpenLaserBind(index == 0 ? GameConfigKeys::Controller_Laser0Axis : GameConfigKeys::Controller_Laser1Axis);
		}
	}

	inline void RenderKeyBindings()
	{
		const float DESIRED_BT_HEIGHT = m_lineHeight * 3.0f;
		const float DESIRED_BT_SPACING = m_lineHeight * 0.25f;

		const float DESIRED_CONTROLLER_WIDTH = DESIRED_BT_HEIGHT*4 + DESIRED_BT_SPACING*3;

		const float CONTROLLER_WIDTH = Math::Min(DESIRED_CONTROLLER_WIDTH, m_pageInnerWidth * 0.8f);
		const float BT_SPACING = CONTROLLER_WIDTH / (DESIRED_CONTROLLER_WIDTH / DESIRED_BT_SPACING);

		const float BT_HEIGHT = (CONTROLLER_WIDTH - BT_SPACING * 3)/ 4;
		const float FX_HEIGHT = BT_HEIGHT / 2;
		const float LASER_HEIGHT = BT_HEIGHT / 2;

		const float CONTROLLER_HEIGHT = LASER_HEIGHT + BT_SPACING * 2 + BT_HEIGHT + BT_SPACING + FX_HEIGHT + m_lineHeight * 5;
		const float CONTROLLER_MARGIN = (m_pageInnerWidth - CONTROLLER_WIDTH) / 2;

		// Center align the whole thing
		const float sizes[] = { CONTROLLER_MARGIN, CONTROLLER_WIDTH, CONTROLLER_MARGIN };
		nk_layout_row(m_nctx, NK_STATIC, CONTROLLER_HEIGHT, 3, sizes);

		Label("");

		if (nk_group_begin(m_nctx, "Key Bindings Main", NK_WINDOW_NO_SCROLLBAR))
		{
			// Lasers and start
			nk_style_push_float(m_nctx, &m_nctx->style.window.spacing.x, BT_SPACING * 2);
			nk_style_push_float(m_nctx, &m_nctx->style.window.spacing.y, BT_SPACING * 2);

			nk_style_push_style_item(m_nctx, &m_nctx->style.button.normal, nk_style_item_color(nk_rgb(0, 0, 0)));
			nk_style_push_style_item(m_nctx, &m_nctx->style.button.hover, nk_style_item_color(nk_rgb(0, 0, 0)));
			nk_style_push_color(m_nctx, &m_nctx->style.button.border_color, nk_rgb(0, 0, 0));
			nk_style_push_color(m_nctx, &m_nctx->style.button.text_normal , nk_rgb(0, 0, 0));
			nk_style_push_color(m_nctx, &m_nctx->style.button.text_hover, nk_rgb(0, 0, 0));

			nk_style_push_float(m_nctx, &m_nctx->style.button.rounding, LASER_HEIGHT / 4);

			LayoutRowDynamic(3, LASER_HEIGHT);

			RenderKeyBindingsLaserButton(0);

			m_nctx->style.button.normal = nk_style_item_color(nk_rgb(0, 111, 232));
			m_nctx->style.button.hover = nk_style_item_color(nk_rgb(111, 180, 255));
			m_nctx->style.button.text_normal = nk_rgb(255, 255, 255);
			m_nctx->style.button.text_hover = nk_rgb(255, 255, 255);
			m_nctx->style.button.border_color = nk_rgb(0, 67, 140);

			m_nctx->style.button.rounding = 0;
			if (nk_button_label(m_nctx, m_controllerButtonNames[0].c_str())) OpenButtonBind((*m_activeBTKeys)[0]);

			m_nctx->style.button.rounding = LASER_HEIGHT / 4;

			RenderKeyBindingsLaserButton(1);

			m_nctx->style.button.rounding = 2;

			// BT
			m_nctx->style.button.normal = nk_style_item_color(nk_rgb(224, 224, 224));
			m_nctx->style.button.hover = nk_style_item_color(nk_rgb(255, 255, 255));
			m_nctx->style.button.text_normal = nk_rgb(0, 0, 0);
			m_nctx->style.button.text_hover = nk_rgb(0, 0, 0);
			m_nctx->style.button.border_color = nk_rgb(128, 128, 128);

			LayoutRowDynamic(4, BT_HEIGHT);
			if (nk_button_label(m_nctx, m_controllerButtonNames[1].c_str())) OpenButtonBind((*m_activeBTKeys)[1]);
			if (nk_button_label(m_nctx, m_controllerButtonNames[2].c_str())) OpenButtonBind((*m_activeBTKeys)[2]);
			if (nk_button_label(m_nctx, m_controllerButtonNames[3].c_str())) OpenButtonBind((*m_activeBTKeys)[3]);
			if (nk_button_label(m_nctx, m_controllerButtonNames[4].c_str())) OpenButtonBind((*m_activeBTKeys)[4]);

			// FX
			m_nctx->style.button.normal = nk_style_item_color(nk_rgb(255, 160, 0));
			m_nctx->style.button.hover = nk_style_item_color(nk_rgb(255, 219, 157));
			m_nctx->style.button.text_normal = nk_rgb(128, 80, 0);
			m_nctx->style.button.text_hover = nk_rgb(128, 80, 0);
			m_nctx->style.button.border_color = nk_rgb(128, 80, 0);

			LayoutRowDynamic(2, FX_HEIGHT);
			if (nk_button_label(m_nctx, m_controllerButtonNames[5].c_str())) OpenButtonBind((*m_activeBTKeys)[5]);
			if (nk_button_label(m_nctx, m_controllerButtonNames[6].c_str())) OpenButtonBind((*m_activeBTKeys)[6]);

			nk_style_pop_float(m_nctx);

			nk_style_pop_color(m_nctx);
			nk_style_pop_color(m_nctx);
			nk_style_pop_color(m_nctx);

			nk_style_pop_style_item(m_nctx);
			nk_style_pop_style_item(m_nctx);

			nk_style_pop_float(m_nctx);
			nk_style_pop_float(m_nctx);

			LayoutRowDynamic(1);
			Label("Back:");
			if (nk_button_label(m_nctx, m_controllerButtonNames[7].c_str())) OpenButtonBind((*m_activeBTKeys)[7]);

			if (!m_useBTGamepad)
			{
				Separator(m_lineHeight * 0.5f);
				LayoutRowDynamic(2);

				if (nk_option_label(m_nctx, "Primary", m_altBinds ? 0 : 1) > 0) m_altBinds = false;
				if (nk_option_label(m_nctx, "Alternate", m_altBinds ? 1 : 0) > 0) m_altBinds = true;
			}

			nk_group_end(m_nctx);
		}

		Label("");
	}

	inline void RenderLaserSensitivitySettings()
	{
		GameConfigKeys laserSensKey;
		switch (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice))
		{
		case InputDevice::Controller:
			laserSensKey = GameConfigKeys::Controller_Sensitivity;
			break;
		case InputDevice::Mouse:
			laserSensKey = GameConfigKeys::Mouse_Sensitivity;
			Label(Utility::Sprintf("Estimated PPR: %.0f", Input::EstimatePprFromSens(g_gameConfig.GetFloat(laserSensKey))));
			break;
		case InputDevice::Keyboard:
		default:
			laserSensKey = GameConfigKeys::Key_Sensitivity;
			break;
		}

		FloatSetting(laserSensKey, "Laser sensitivity (%g):", 0, 20, 0.001f);
		FloatSetting(GameConfigKeys::SongSelSensMult, "Song select sensitivity multiplier", 0.0f, 20.0f, 0.1f);

		if (nk_button_label(m_nctx, "Calibrate Laser Sensitivity")) OpenCalibrateSensitivity();
	}

	inline void RenderUIControlsSettings()
	{
		_RenderSingleUIControlsSettings(0, "Song select (and downloads)", m_songSelectKeys, m_activeSongSelectKeys);
		_RenderSingleUIControlsSettings(1, "In-game", m_gameKeys, m_activeGameKeys);
		_RenderSingleUIControlsSettings(2, "Results", m_resultKeys, m_activeResultKeys);
		_RenderSingleUIControlsSettings(3, "Misc", m_miscKeys, m_activeMiscKeys);
	}
	inline void _RenderSingleUIControlsSettings(int index, const char* name, Vector<GameConfigKeys> keys, const Vector<GameConfigKeys>* activeKeys)
	{
		if (nk_tree_push_id(m_nctx, NK_TREE_NODE, name, NK_MINIMIZED, index))
		{
			LayoutRowDynamic(2, m_lineHeight);
			for (size_t i = 0; i < keys.size(); i++)
			{
				Label(GetKeyNameFromConfigKey(keys[i]) + ":");
				if (nk_button_label(m_nctx, GetKeyNameFromScancodeConfig(g_gameConfig.GetInt((*activeKeys)[i]))))
					OpenButtonBind((*activeKeys)[i]);
			}
			nk_tree_pop(m_nctx);
		}
	}

	inline void OpenLaserBind(GameConfigKeys axis)
	{
		const InputDevice laserInputDevice = g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice);
		OpenButtonBind(axis, laserInputDevice == InputDevice::Controller);
	}

	inline void OpenButtonBind(GameConfigKeys key)
	{
		OpenButtonBind(key, m_useBTGamepad);
	}
	inline void OpenButtonBind(GameConfigKeys key, bool gamepad)
	{
		g_application->AddTickable(ButtonBindingScreen::Create(key, gamepad, m_selectedPad, m_altBinds));
	}

	inline void OpenCalibrateSensitivity()
	{
		LaserSensCalibrationScreen* sensScreen = LaserSensCalibrationScreen::Create();
		sensScreen->SensSet.Add(this, &SettingsPage_Input::SetSensitivity);
		g_application->AddTickable(sensScreen);
	}
	inline void SetSensitivity(float sens)
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

	inline void UpdateInputKeyBindingStatus()
	{
		m_useBTGamepad = g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Controller;
		m_useLaserGamepad = g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice) == InputDevice::Controller;

		if (m_useBTGamepad) m_activeBTKeys = &m_controllerKeys;
		else if (m_altBinds) m_activeBTKeys = &m_altKeyboardKeys;
		else m_activeBTKeys = &m_keyboardKeys;

		if (m_altBinds) m_activeLaserKeys = &m_altKeyboardLaserKeys;
		else m_activeLaserKeys = &m_keyboardLaserKeys;
	}
	inline void UpdateControllerInputNames()
	{
		for (size_t i = 0; i < 8; i++)
		{
			if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Controller)
			{
				m_controllerButtonNames[i] = Utility::Sprintf("%d", g_gameConfig.GetInt(m_controllerKeys[i]));
			}
			else
			{
				m_controllerButtonNames[i] = GetKeyNameFromScancodeConfig(g_gameConfig.GetInt((*m_activeBTKeys)[i]));
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
				m_controllerLaserNames[i] = Utility::ConvertToUTF8(Utility::WSprintf( // wstring->string because regular Sprintf messes up(?????)
					L"%ls / %ls",
					Utility::ConvertToWString(GetKeyNameFromScancodeConfig(g_gameConfig.GetInt((*m_activeLaserKeys)[i * 2]))),
					Utility::ConvertToWString(GetKeyNameFromScancodeConfig(g_gameConfig.GetInt((*m_activeLaserKeys)[i * 2 + 1])))
				));
			}
		}
	}
};

class SettingsPage_Game : public SettingsPage
{
public:
	SettingsPage_Game(nk_context* nctx) : SettingsPage(nctx, "Game") {}

protected:
	void Load() override
	{
		m_hitWindow = HitWindow::FromConfig();

		m_songsPath.Load();
	}

	void Save() override
	{
		m_hitWindow.Validate();
		m_hitWindow.SaveConfig();

		m_songsPath.Save();
	}

	HitWindow m_hitWindow = HitWindow::NORMAL;

	GameConfigTextData m_songsPath = { GameConfigKeys::SongFolder };

protected:
	void RenderContents() override
	{
		SectionHeader("Offsets");

		LayoutRowDynamic(2);
		IntSetting(GameConfigKeys::GlobalOffset, "Global offset", -1000, 1000);
		IntSetting(GameConfigKeys::InputOffset, "Button Input offset", -1000, 1000);
		IntSetting(GameConfigKeys::LaserOffset, "Laser Input offset", -1000, 1000);

		LayoutRowDynamic(1);
		if (nk_button_label(m_nctx, "Calibrate Offsets")) {
			CalibrationScreen* cscreen = new CalibrationScreen(m_nctx);
			g_transition->TransitionTo(cscreen);
			return;
		}

		SectionHeader("Speed Mod");
		EnumSetting<Enum_SpeedMods>(GameConfigKeys::SpeedMod, "Speed mod type:");
		FloatSetting(GameConfigKeys::HiSpeed, "HiSpeed", 0.25f, 20, 0.05f);
		FloatSetting(GameConfigKeys::ModSpeed, "ModSpeed", 50, 1500, 0.5f);
		ToggleSetting(GameConfigKeys::AutoSaveSpeed, "Save hi-speed changes during gameplay");

		SectionHeader("Timing Window");
		RenderTimingWindowSettings();

		SectionHeader("Before Playing");
		IntSetting(GameConfigKeys::LeadInTime, "Lead-in time (ms)", 250, 10000, 250);
		IntSetting(GameConfigKeys::PracticeLeadInTime, "(for practice mode)", 250, 10000, 250);

		ToggleSetting(GameConfigKeys::AutoComputeSongOffset, "Before first-time play, compute and set the song offset automatically");

		SectionHeader("After Playing");
		ToggleSetting(GameConfigKeys::SkipScore, "Skip score screen on manual exit");
		EnumSetting<Enum_AutoScoreScreenshotSettings>(GameConfigKeys::AutoScoreScreenshot, "Automatically capture score screenshots:");

		EnumSetting<Enum_SongOffsetUpdateMethod>(GameConfigKeys::UpdateSongOffsetAfterFirstPlay, "Based on hit stats, update song offset for first:");
		EnumSetting<Enum_SongOffsetUpdateMethod>(GameConfigKeys::UpdateSongOffsetAfterEveryPlay, "After having updated first time, update song offset for every:");

		SectionHeader("Songs");
		Label("Songs folder path:");
		m_songsPath.Render(m_nctx);

		ToggleSetting(GameConfigKeys::TransferScoresOnChartUpdate, "When a chart is modified, do not reset the scores for the chart");
	}

private:
	inline void RenderTimingWindowSettings()
	{
		LayoutRowDynamic(4);

		const int hitWindowPerfect = IntInput(m_hitWindow.perfect, "Crit", 0, HitWindow::NORMAL.perfect);
		if (hitWindowPerfect != m_hitWindow.perfect)
		{
			m_hitWindow.perfect = hitWindowPerfect;
			if (m_hitWindow.good < m_hitWindow.perfect)
				m_hitWindow.good = m_hitWindow.perfect;
			if (m_hitWindow.hold < m_hitWindow.perfect)
				m_hitWindow.hold = m_hitWindow.perfect;
		}

		const int hitWindowGood = IntInput(m_hitWindow.good, "Near", 0, HitWindow::NORMAL.good);
		if (hitWindowGood != m_hitWindow.good)
		{
			m_hitWindow.good = hitWindowGood;
			if (m_hitWindow.good < m_hitWindow.perfect)
				m_hitWindow.perfect = m_hitWindow.good;
			if (m_hitWindow.hold < m_hitWindow.good)
				m_hitWindow.hold = m_hitWindow.good;
		}

		const int hitWindowHold = IntInput(m_hitWindow.hold, "Hold", 0, HitWindow::NORMAL.hold);
		if (hitWindowHold != m_hitWindow.hold)
		{
			m_hitWindow.hold = hitWindowHold;
			if (m_hitWindow.hold < m_hitWindow.perfect)
				m_hitWindow.perfect = m_hitWindow.hold;
			if (m_hitWindow.hold < m_hitWindow.good)
				m_hitWindow.good = m_hitWindow.hold;
		}

		const int hitWindowSlam = IntInput(m_hitWindow.slam, "Slam", 0, HitWindow::NORMAL.slam);
		if (hitWindowSlam != m_hitWindow.slam)
			m_hitWindow.slam = hitWindowSlam;

		LayoutRowDynamic(2);

		if (nk_button_label(m_nctx, "Set to NORMAL (default)"))
		{
			m_hitWindow = HitWindow::NORMAL;
		}

		if (nk_button_label(m_nctx, "Set to HARD"))
		{
			m_hitWindow = HitWindow::HARD;
		}
	}
};

class SettingsPage_Visual : public SettingsPage
{
public:
	SettingsPage_Visual(nk_context* nctx) : SettingsPage(nctx, "Visual") {}

protected:
	void Load() override
	{
		m_laserColors[0] = g_gameConfig.GetFloat(GameConfigKeys::Laser0Color);
		m_laserColors[1] = g_gameConfig.GetFloat(GameConfigKeys::Laser1Color);
	}

	void Save() override
	{
		g_gameConfig.Set(GameConfigKeys::Laser0Color, m_laserColors[0]);
		g_gameConfig.Set(GameConfigKeys::Laser1Color, m_laserColors[1]);

		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Mouse)
		{
			g_gameConfig.SetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice, InputDevice::Keyboard);
		}
	}

	std::array<float, 2> m_laserColors = { 200.0f, 330.0f };

	void RenderContents() override
	{
		SectionHeader("Hidden / Sudden");
		ToggleSetting(GameConfigKeys::EnableHiddenSudden, "Enable Hidden/Sudden");
		ToggleSetting(GameConfigKeys::ShowCover, "Show track cover");

		LayoutRowDynamic(2, m_lineHeight * 4);

		if (nk_group_begin(m_nctx, "Hidden", NK_WINDOW_NO_SCROLLBAR))
		{
			LayoutRowDynamic(1);
			Label("Hidden", nk_text_alignment::NK_TEXT_CENTERED);
			FloatSetting(GameConfigKeys::HiddenCutoff, "Cutoff", 0.0f, 1.0f);
			FloatSetting(GameConfigKeys::HiddenFade, "Fade", 0.0f, 1.0f);
			nk_group_end(m_nctx);
		}

		if (nk_group_begin(m_nctx, "Sudden", NK_WINDOW_NO_SCROLLBAR))
		{
			LayoutRowDynamic(1);
			Label("Sudden", nk_text_alignment::NK_TEXT_CENTERED);
			FloatSetting(GameConfigKeys::SuddenCutoff, "Cutoff", 0.0f, 1.0f);
			FloatSetting(GameConfigKeys::SuddenFade, "Fade", 0.0f, 1.0f);
			nk_group_end(m_nctx);
		}

		SectionHeader("Lasers");
		RenderLaserColorSetting();

		SectionHeader("Game Elements");

		ToggleSetting(GameConfigKeys::DisableBackgrounds, "Disable song backgrounds");
		ToggleSetting(GameConfigKeys::DelayedHitEffects, "Delayed fade button hit effects");
		FloatSetting(GameConfigKeys::DistantButtonScale, "Distant button scale", 1.0f, 5.0f);

		SectionHeader("Game UI");

		//TODO: Move these somewhere else?
		ToggleSetting(GameConfigKeys::FastGUI, "Use Lightweight GUI (no skin)");
		ToggleSetting(GameConfigKeys::SkinDevMode, "Skin Development Mode");

		EnumSetting<Enum_ScoreDisplayModes>(GameConfigKeys::ScoreDisplayMode, "In-game score display is:");
		ToggleSetting(GameConfigKeys::DisplayPracticeInfoInGame, "Show practice-mode info during gameplay");
	}

private:
	const std::array<float, 4> m_laserColorPalette = { 330.f, 60.f, 100.f, 200.f };
	bool m_laserColorPaletteVisible = false;

	void RenderLaserColorSetting()
	{
		const nk_color leftColor = nk_hsv_f(m_laserColors[0] / 360, 1, 1);
		const nk_color rightColor = nk_hsv_f(m_laserColors[1] / 360, 1, 1);

		const int lcolInt = static_cast<int>(m_laserColors[0]);
		const int rcolInt = static_cast<int>(m_laserColors[1]);

		LayoutRowDynamic(1);
		Label("Laser colors:");

		LayoutRowDynamic(2);

		// Color
		if (nk_button_color(m_nctx, leftColor)) m_laserColorPaletteVisible = !m_laserColorPaletteVisible;
		if (nk_button_color(m_nctx, rightColor)) m_laserColorPaletteVisible = !m_laserColorPaletteVisible;

		// Palette
		if (m_laserColorPaletteVisible)
		{
			LayoutRowDynamic(2 * static_cast<int>(m_laserColorPalette.size()));

			RenderLaserColorPalette(m_laserColors.data());
			RenderLaserColorPalette(m_laserColors.data() + 1);

			LayoutRowDynamic(2);
		}

		// Text
		{
			const int lcolIntNew = IntInput(lcolInt, "Left hue", 0, 360);
			if (lcolIntNew != lcolInt) m_laserColors[0] = static_cast<float>(lcolIntNew);

			const int rcolIntNew = IntInput(rcolInt, "Right hue", 0, 360);
			if (rcolIntNew != rcolInt) m_laserColors[1] = static_cast<float>(rcolIntNew);
		}

		// Slider
		{
			nk_slider_float(m_nctx, 0, m_laserColors.data(), 360, 0.1f);
			nk_slider_float(m_nctx, 0, m_laserColors.data() + 1, 360, 0.1f);
		}
	}

	void RenderLaserColorPalette(float* laserColor)
	{
		for (const float paletteHue : m_laserColorPalette)
		{
			const nk_color paletteColor = nk_hsv_f(paletteHue / 360, 1, 1);
			if (nk_button_color(m_nctx, paletteColor))
			{
				*laserColor = paletteHue;
			}
		}
	}
};

class SettingsPage_System : public SettingsPage
{
public:
	SettingsPage_System(nk_context* nctx) : SettingsPage(nctx, "System") {}

protected:
	void Load() override
	{
		m_channels = { "release", "master", "develop" };
		String channel = g_gameConfig.GetString(GameConfigKeys::UpdateChannel);

		if (!m_channels.Contains(channel))
		{
			m_channels.insert(m_channels.begin(), channel);
		}
	}

	void Save() override
	{
		if (g_gameConfig.GetBool(GameConfigKeys::CheckForUpdates))
		{
			g_application->CheckForUpdate();
		}
	}

	const Vector<const char*> m_aaModes = { "Off", "2x MSAA", "4x MSAA", "8x MSAA", "16x MSAA" };
	Vector<String> m_channels;

protected:
	void RenderContents() override
	{
		bool applySettings = false;
		auto SetApply = [&applySettings](bool b) {
			if (b) applySettings = true;
		};

		SectionHeader("Audio");

		PercentSetting(GameConfigKeys::MasterVolume, "Master volume (%.1f%%):");
		PercentSetting(GameConfigKeys::SlamVolume, "Slam/Clap volume (%.1f%%):");
		ToggleSetting(GameConfigKeys::MuteUnfocused, "Mute the game when unfocused");
#ifdef _WIN32
		ToggleSetting(GameConfigKeys::WASAPI_Exclusive, "WASAPI exclusive mode (requires restart)");
#endif // _WIN32
		ToggleSetting(GameConfigKeys::PrerenderEffects, "Pre-render song effects (experimental)");

		SectionHeader("Render");
		SetApply(ToggleSetting(GameConfigKeys::ResponsiveInputs, "Responsive Inputs (CPU intensive)"));
		SetApply(ToggleSetting(GameConfigKeys::Fullscreen, "Fullscreen"));
		SetApply(ToggleSetting(GameConfigKeys::WindowedFullscreen, "Use windowed fullscreen"));

		ToggleSetting(GameConfigKeys::AdjustWindowPositionOnStartup, "Adjust window positions to be in-bound on startup");

		SetApply(ToggleSetting(GameConfigKeys::ForcePortrait, "Force portrait (don't use if already in portrait)"));

		SelectionSetting(GameConfigKeys::AntiAliasing, m_aaModes, "Anti-aliasing (requires restart):");
		SetApply(ToggleSetting(GameConfigKeys::VSync, "VSync"));
		SetApply(ToggleSetting(GameConfigKeys::ShowFps, "Show FPS"));
		SetApply(ToggleSetting(GameConfigKeys::KeepFontTexture, "Save font texture (settings load faster but uses more memory)"));



		SectionHeader("Update");

		ToggleSetting(GameConfigKeys::CheckForUpdates, "Check for updates on startup");

		if (m_channels.size() > 0)
		{
			StringSelectionSetting(GameConfigKeys::UpdateChannel, m_channels, "Update Channel:");
		}

		SectionHeader("Logging");

		SetApply(EnumSetting<Logger::Enum_Severity>(GameConfigKeys::LogLevel, "Logging level:"));

		if (applySettings)
		{
			g_application->ApplySettings();
		}
	}
};

class SettingsPage_Practice : public SettingsPage
{
public:
	SettingsPage_Practice(nk_context* nctx) : SettingsPage(nctx, "Practice") {}

protected:
	void Load() override {}
	void Save() override {}

protected:
	void RenderContents() override
	{
		SectionHeader("Settings");
		IntSetting(GameConfigKeys::PracticeLeadInTime, "Lead-in time (ms)", 250, 10000, 250);
		ToggleSetting(GameConfigKeys::PracticeSetupNavEnabled, "Enable navigation inputs for the setup");
		ToggleSetting(GameConfigKeys::RevertToSetupAfterScoreScreen, "Revert to the setup after the result is shown");
		ToggleSetting(GameConfigKeys::DisplayPracticeInfoInGame, "Show practice-mode info during gameplay");

		SectionHeader("Defaults for Playback and Loop Control");
		IntSetting(GameConfigKeys::DefaultPlaybackSpeed, "Playback speed (%)", 25, 100);

		Separator();

		ToggleSetting(GameConfigKeys::DefaultLoopOnSuccess, "Loop on success");
		ToggleSetting(GameConfigKeys::DefaultIncSpeedOnSuccess, "Increase speed on success");
		IntSetting(GameConfigKeys::DefaultIncSpeedAmount, "Increment (%p)", 1, 10);
		IntSetting(GameConfigKeys::DefaultIncStreak, "Required streaks", 1, 10);

		Separator();

		ToggleSetting(GameConfigKeys::DefaultLoopOnSuccess, "Loop on fail");
		ToggleSetting(GameConfigKeys::DefaultDecSpeedOnFail, "Decrease speed on fail");
		IntSetting(GameConfigKeys::DefaultDecSpeedAmount, "Decrement (%p)", 1, 10);
		IntSetting(GameConfigKeys::DefaultMinPlaybackSpeed, "Minimum speed (%)", 25, 100);

		Separator();

		ToggleSetting(GameConfigKeys::DefaultEnableMaxRewind, "Set maximum amount of rewinding on fail");
		IntSetting(GameConfigKeys::DefaultMaxRewindMeasure, "Amount in # of measures", 0, 20);

		SectionHeader("Defaults for Mission");
		SelectionSetting(GameConfigKeys::DefaultFailConditionType, GameFailCondition::TYPE_STR, "Fail Condition");

		Separator();

		IntSetting(GameConfigKeys::DefaultFailConditionScore, "Score less than", 800 * 10000, 1000 * 10000, 10000);
		SelectionSetting(GameConfigKeys::DefaultFailConditionGrade, GRADE_MARK_STR, "Grade less than");
		IntSetting(GameConfigKeys::DefaultFailConditionMiss, "Miss more than", 0, 100);
		IntSetting(GameConfigKeys::DefaultFailConditionMissNear, "Miss+Near more than", 0, 100);
		IntSetting(GameConfigKeys::DefaultFailConditionGauge, "Gauge less than", 0, 100);
	}
};

class SettingsPage_Online : public SettingsPage
{
public:
	SettingsPage_Online(nk_context* nctx) : SettingsPage(nctx, "Online") {}

protected:
	void Load() override
	{
		m_multiplayerHost.Load();
		m_multiplayerPassword.Load();
		m_multiplayerUsername.Load();

		m_irBaseURL.Load();
		m_irToken.Load();
	}

	void Save() override
	{
		m_multiplayerHost.Save();
		m_multiplayerPassword.Save();
		m_multiplayerUsername.Save();

		m_irBaseURL.Save();
		m_irToken.Save();
	}

	GameConfigTextData m_multiplayerHost = { GameConfigKeys::MultiplayerHost };
	GameConfigTextData m_multiplayerPassword = { GameConfigKeys::MultiplayerPassword };
	GameConfigTextData m_multiplayerUsername = { GameConfigKeys::MultiplayerUsername };

	GameConfigTextData m_irBaseURL = { GameConfigKeys::IRBaseURL };
	GameConfigTextData m_irToken = { GameConfigKeys::IRToken };

	void RenderContents() override
	{
		SectionHeader("Multiplayer");

		Label("Server:");
		m_multiplayerHost.Render(m_nctx);

		Label("Username:");
		m_multiplayerUsername.Render(m_nctx);

		Label("Password:");
		m_multiplayerPassword.RenderPassword(m_nctx);

		SectionHeader("Internet Ranking");

		Label("IR base URL:");
		m_irBaseURL.Render(m_nctx);

		Label("IR token:");
		m_irToken.RenderPassword(m_nctx);

		ToggleSetting(GameConfigKeys::IRLowBandwidth, "Low bandwidth mode (disables sending replays)");
	}
};

class SettingsPage_Skin : public SettingsPage
{
public:
	SettingsPage_Skin(nk_context* nctx) : SettingsPage(nctx, "Skin") {}

protected:
	void Load() override
	{
		m_skin = g_gameConfig.GetString(GameConfigKeys::Skin);
		if (m_skin == g_application->GetCurrentSkin())
		{
			m_skinConfig = g_skinConfig;
		}
		else
		{
			m_skinConfig = new SkinConfig(m_skin);
		}

		m_allSkins = Path::GetSubDirs(Path::Normalize(Path::Absolute("skins/")));
		m_skinConfigTextData.clear();
		m_useHSVMap.clear();
	}

	void Save() override
	{
		for (auto& it : m_skinConfigTextData)
		{
			it.second.Save();
		}

		if (m_skinConfig != g_skinConfig && m_skinConfig)
		{
			delete m_skinConfig;
			m_skinConfig = nullptr;
			// These keep a ref to skinConfig so clear too
			m_skinConfigTextData.clear();
		}
	}

	String m_skin;
	SkinConfig* m_skinConfig = nullptr;

	Vector<String> m_allSkins;

	void RenderContents() override
	{
		SectionHeader("Skin");

		if (SkinSelectionSetting("Selected skin:"))
		{
			Image cursorImg = ImageRes::Create(Path::Absolute("skins/" + g_gameConfig.GetString(GameConfigKeys::Skin) + "/textures/cursor.png"));
			g_gameWindow->SetCursor(cursorImg, Vector2i(5, 5));
		}

		if (m_skinConfig == nullptr)
		{
			return;
		}

		Separator();
		SectionHeader(Utility::Sprintf("Settings for [%s]", m_skin.data()));

		for (const SkinSetting& setting : m_skinConfig->GetSettings())
		{
			RenderSkinSetting(setting);
		}
	}

private:
	class SkinConfigTextData : public SettingTextData
	{
	public:
		SkinConfigTextData(SkinConfig* skinConfig, const String& key) : m_skinConfig(skinConfig), m_key(key) {}

	protected:
		String LoadConfig() override { return m_skinConfig ? m_skinConfig->GetString(m_key) : ""; }
		void SaveConfig(const String& value) override { if(m_skinConfig) m_skinConfig->Set(m_key, value); }

		SkinConfig* m_skinConfig = nullptr;
		String m_key;
	};

	Map<String, SkinConfigTextData> m_skinConfigTextData;
	Map<String, bool> m_useHSVMap;

	bool SkinSelectionSetting(const std::string_view& label)
	{
		if (m_allSkins.empty())
		{
			return false;
		}

		if (StringSelectionSetting(GameConfigKeys::Skin, m_allSkins, label))
		{
			Save(); Load();
			return true;
		}

		return false;
	}

	inline bool SkinToggleSetting(const SkinSetting& setting)
	{
		const bool value = m_skinConfig->GetBool(setting.key);
		const bool newValue = ToggleInput(value, setting.label);

		if (newValue != value)
		{
			m_skinConfig->Set(setting.key, newValue);
			return true;
		}

		return false;
	}

	inline bool SkinStringSelectionSetting(const SkinSetting& setting)
	{
		if (setting.selectionSetting.numOptions == 0)
		{
			return false;
		}

		const String& value = m_skinConfig->GetString(setting.key);
		int selection = 0;

		const String* options = setting.selectionSetting.options;
		const auto stringSearch = std::find(options, options + setting.selectionSetting.numOptions, value);
		if (stringSearch != options + setting.selectionSetting.numOptions)
		{
			selection = static_cast<int>(stringSearch - options);
		}

		Vector<const char*> displayData;
		for (int i=0; i<setting.selectionSetting.numOptions; ++i)
		{
			displayData.Add(setting.selectionSetting.options[i].data());
		}

		const int newSelection = SelectionInput(selection, displayData, setting.label);

		if (newSelection != selection) {
			m_skinConfig->Set(setting.key, options[newSelection]);
			return true;
		}

		return false;
	}

	inline bool SkinIntSetting(const SkinSetting& setting)
	{
		const int value = m_skinConfig->GetInt(setting.key);
		const int newValue = IntInput(value, setting.label,
			setting.intSetting.min, setting.intSetting.max);

		if (newValue != value) {
			m_skinConfig->Set(setting.key, newValue);
			return true;
		}

		return false;
	}

	inline bool SkinFloatSetting(const SkinSetting& setting)
	{
		float value = m_skinConfig->GetFloat(setting.key);
		const float prevValue = value;
		const float step = 0.01f;

		nk_labelf(m_nctx, nk_text_alignment::NK_TEXT_LEFT, setting.label.data(), value);
		nk_slider_float(m_nctx, setting.floatSetting.min, &value, setting.floatSetting.max, step);

		if (prevValue != value) {
			m_skinConfig->Set(setting.key, value);
			return true;
		}

		return false;
	}

	inline void SkinTextSetting(const SkinSetting& setting)
	{
		auto it = m_skinConfigTextData.find(setting.key);
		if (it == m_skinConfigTextData.end())
		{
			it = m_skinConfigTextData.emplace_hint(it, setting.key, SkinConfigTextData { m_skinConfig, setting.key });
			it->second.Load();
		}

		if (it == m_skinConfigTextData.end())
		{
			return;
		}


		nk_label(m_nctx, setting.label.data(), nk_text_alignment::NK_TEXT_LEFT);

		SkinConfigTextData& data = it->second;

		if (setting.textSetting.secret)
		{
			data.RenderPassword(m_nctx);
		}
		else
		{
			data.Render(m_nctx);
		}
	}

	inline bool SkinColorSetting(const SkinSetting& setting)
	{
		const Color value = m_skinConfig->GetColor(setting.key);
		const Color newValue = ColorInput(value, setting.label, m_useHSVMap[setting.key]);

		if (newValue != value)
		{
			m_skinConfig->Set(setting.key, newValue);
			return true;
		}

		return false;
	}

	void RenderSkinSetting(const SkinSetting& setting)
	{
		switch (setting.type)
		{
		case SkinSetting::Type::Separator:
			Separator();
			break;
		case SkinSetting::Type::Label:
			Label(setting.key);
			break;
		case SkinSetting::Type::Boolean:
			SkinToggleSetting(setting);
			break;
		case SkinSetting::Type::Selection:
			SkinStringSelectionSetting(setting);
			break;
		case SkinSetting::Type::Integer:
			SkinIntSetting(setting);
			break;
		case SkinSetting::Type::Float:
			SkinFloatSetting(setting);
			break;
		case SkinSetting::Type::Text:
			SkinTextSetting(setting);
			break;
		case SkinSetting::Type::Color:
			SkinColorSetting(setting);
			break;
		}
	}
};

class SettingsScreen_Impl : public SettingsPageCollection
{
public:
	void OnSuspend() override
	{
	}

	void OnRestore() override
	{
		g_application->DiscordPresenceMenu("Settings");
	}

protected:
	void AddPages(Vector<std::unique_ptr<SettingsPage>>& pages) override
	{
		pages.emplace_back(std::make_unique<SettingsPage_Input>(m_nctx));
		pages.emplace_back(std::make_unique<SettingsPage_Game>(m_nctx));
		pages.emplace_back(std::make_unique<SettingsPage_Visual>(m_nctx));
		pages.emplace_back(std::make_unique<SettingsPage_System>(m_nctx));
		pages.emplace_back(std::make_unique<SettingsPage_Practice>(m_nctx));
		pages.emplace_back(std::make_unique<SettingsPage_Online>(m_nctx));
		pages.emplace_back(std::make_unique<SettingsPage_Skin>(m_nctx));
	}
};

IApplicationTickable* SettingsScreen::Create()
{
	return new SettingsScreen_Impl();
}

class ButtonBindingScreen_Impl : public ButtonBindingScreen
{
private:
	Ref<Gamepad> m_gamepad;
	//Label* m_prompt;

	GameConfigKeys m_key;
	String m_keyName;

	bool m_isGamepad;
	int m_gamepadIndex;
	bool m_completed = false;
	bool m_knobs = false;
	bool m_isAlt = false;
	Vector<float> m_gamepadAxes;

public:
	ButtonBindingScreen_Impl(GameConfigKeys key, bool gamepad, int controllerIndex, bool isAlt)
	{
		m_key = key;
		m_gamepadIndex = controllerIndex;
		m_isGamepad = gamepad;
		m_knobs = (key == GameConfigKeys::Controller_Laser0Axis || key == GameConfigKeys::Controller_Laser1Axis);
		m_isAlt = isAlt;

		m_keyName = GetKeyNameFromConfigKey(m_key);
		if (isAlt)
		{
			m_keyName += " (alt)";
		}
	}

	bool Init() override
	{
		if (m_isGamepad)
		{
			m_gamepad = g_gameWindow->OpenGamepad(m_gamepadIndex);
			if (!m_gamepad)
			{
				Logf("Failed to open gamepad: %d", Logger::Severity::Error, m_gamepadIndex);
				g_gameWindow->ShowMessageBox("Warning",
					"The controller configured to use is unavailable.\n"
					"Ensure the controller is connected, or (if applicable) set the controller in the correct mode.          \n"
					, 1);
				g_gameWindow->ShowMessageBox("Warning",
					"Otherwise, select another controller to use in the settings page.          \n"
					, 1);
				return false;
			}
			if (m_knobs)
			{
				for (uint8 i = 0; i < m_gamepad->NumAxes(); i++)
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

	void Tick(float deltatime) override
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
			m_gamepad.reset();

			g_application->RemoveTickable(this);
		}
		else if (m_completed && !m_knobs)
		{
			g_application->RemoveTickable(this);
		}

	}

	void Render(float deltatime) override
	{
		String prompt = "Press the key";

		if (m_knobs)
		{
			prompt = "Press the left key";
			if (m_completed)
			{
				prompt = "Press the right key";
			}
		}

		if (m_isGamepad)
		{
			prompt = "Press the controller button";
			m_gamepad = g_gameWindow->OpenGamepad(m_gamepadIndex);
			if (m_knobs)
			{
				prompt = "Turn the knob";
				for (uint8 i = 0; i < m_gamepad->NumAxes(); i++)
				{
					m_gamepadAxes.Add(m_gamepad->GetAxis(i));
				}
			}
		}

		prompt += " for " + m_keyName + ".";

		g_application->FastText(prompt, static_cast<float>(g_resolution.x / 2), static_cast<float>(g_resolution.y / 2), 40, NVGalign::NVG_ALIGN_CENTER | NVGalign::NVG_ALIGN_MIDDLE);
	}

	void OnButtonPressed(uint8 key, int32 delta)
	{
		if (!m_knobs)
		{
			g_gameConfig.Set(m_key, key);
			m_completed = true;
		}
	}

	virtual void OnKeyPressed(SDL_Scancode code, int32 delta)
	{
		if (!m_isGamepad && !m_knobs)
		{
			g_gameConfig.Set(m_key, code);
			m_completed = true; // Needs to be set because pressing right alt triggers two keypresses on the same frame.
		}
		else if (!m_isGamepad && m_knobs)
		{
			switch (m_key)
			{
			case GameConfigKeys::Controller_Laser0Axis:
				g_gameConfig.Set(m_completed ?
					m_isAlt ? GameConfigKeys::Key_Laser0PosAlt : GameConfigKeys::Key_Laser0Pos :
					m_isAlt ? GameConfigKeys::Key_Laser0NegAlt : GameConfigKeys::Key_Laser0Neg,
					code);
				break;
			case GameConfigKeys::Controller_Laser1Axis:
				g_gameConfig.Set(m_completed ?
					m_isAlt ? GameConfigKeys::Key_Laser1PosAlt : GameConfigKeys::Key_Laser1Pos :
					m_isAlt ? GameConfigKeys::Key_Laser1NegAlt : GameConfigKeys::Key_Laser1Neg,
					code);
				break;
			default:
				break;
			}

			if (!m_completed)
			{
				m_completed = true;
			}
			else
			{
				g_application->RemoveTickable(this);
			}
		}

		if (m_isGamepad && code == SDL_Scancode::SDL_SCANCODE_ESCAPE)
		{
			if (m_gamepad)
			{
				m_gamepad->OnButtonPressed.RemoveAll(this);
			}

			g_application->RemoveTickable(this);
		}
	}

	void OnSuspend() override
	{
		//g_rootCanvas->Remove(m_canvas.As<GUIElementBase>());
	}
	void OnRestore() override
	{
		//Canvas::Slot* slot = g_rootCanvas->Add(m_canvas.As<GUIElementBase>());
		//slot->anchor = Anchors::Full;
	}
};

ButtonBindingScreen* ButtonBindingScreen::Create(GameConfigKeys key, bool gamepad, int controllerIndex, bool isAlternative)
{
	ButtonBindingScreen_Impl* impl = new ButtonBindingScreen_Impl(key, gamepad, controllerIndex, isAlternative);
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
	MouseLockHandle m_mouseLock;
public:
	LaserSensCalibrationScreen_Impl()
	{

	}

	~LaserSensCalibrationScreen_Impl()
	{
		g_input.OnButtonPressed.RemoveAll(this);
	}

	bool Init() override
	{
		g_input.GetInputLaserDir(0); //poll because there might be something idk

		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice) == InputDevice::Controller)
		{
			m_currentSetting = g_gameConfig.GetFloat(GameConfigKeys::Controller_Sensitivity);
		}
		else
		{
			m_currentSetting = g_gameConfig.GetFloat(GameConfigKeys::Mouse_Sensitivity);
			m_mouseLock = g_input.LockMouse();
		}

		g_input.OnButtonPressed.Add(this, &LaserSensCalibrationScreen_Impl::OnButtonPressed);
		return true;
	}

	void Tick(float deltatime) override
	{
		m_delta += g_input.GetAbsoluteInputLaserDir(0);

	}

	void Render(float deltatime) override
	{
		const Vector2 center = { static_cast<float>(g_resolution.x / 2), static_cast<float>(g_resolution.y / 2) };

		if (m_state)
		{
			float sens = 6.0f / m_delta;
			if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice) == InputDevice::Mouse)
			{
				sens = Input::CalculateSensFromPpr(m_delta);
			}


			g_application->FastText("Turn left knob one revolution clockwise", center.x, center.y, 40, NVGalign::NVG_ALIGN_CENTER | NVGalign::NVG_ALIGN_MIDDLE);
			g_application->FastText("then press start.", center.x, center.y + 45, 40, NVGalign::NVG_ALIGN_CENTER | NVGalign::NVG_ALIGN_MIDDLE);
			if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice) == InputDevice::Mouse)
				g_application->FastText(Utility::Sprintf("Current Sens: %.2f, ppr: (%.0f)", sens, fabs(m_delta)), center.x, center.y + 90, 40, NVGalign::NVG_ALIGN_CENTER | NVGalign::NVG_ALIGN_MIDDLE);
			else
				g_application->FastText(Utility::Sprintf("Current Sens: %.2f", sens), center.x, center.y + 90, 40, NVGalign::NVG_ALIGN_CENTER | NVGalign::NVG_ALIGN_MIDDLE);

		}
		else
		{
			m_delta = 0;
			g_application->FastText("Press start twice", center.x, center.y, 40, NVGalign::NVG_ALIGN_CENTER | NVGalign::NVG_ALIGN_MIDDLE);
		}
	}

	void OnButtonPressed(Input::Button button, int32 delta)
	{
		if (button == Input::Button::BT_S)
		{
			if (m_firstStart)
			{
				if (m_state)
				{
					// calc sens and then call delegate
					float sens = 6.0f / m_delta;
					if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice) == InputDevice::Mouse)
					{
						sens = Input::CalculateSensFromPpr(m_delta);
					}
					SensSet.Call(sens);
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

	void OnKeyPressed(SDL_Scancode code, int32 delta) override
	{
		if (code == SDL_SCANCODE_ESCAPE)
			g_application->RemoveTickable(this);
	}

	void OnSuspend() override
	{
		//g_rootCanvas->Remove(m_canvas.As<GUIElementBase>());
	}
	void OnRestore() override
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

