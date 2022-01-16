#include "stdafx.h"
#include "GameConfig.hpp"

#include "Shared/Log.hpp"
#include "HitStat.hpp"
#include "Input.hpp"

// When this should change, the UpdateVersion MUST be updated to update the old config files.
// If there's no need to update the UpdateVersion, there's no need to touch this too.
int32 GameConfig::VERSION = 2;

inline static void ConvertKeyCodeToScanCode(GameConfig& config, std::vector<GameConfigKeys> keys)
{
	// To use SDL_GetScancodeFromKey, SDL must be initialized before.
	assert(SDL_WasInit(SDL_INIT_EVENTS) != 0);

	for (const GameConfigKeys key : keys)
	{
		const int32 keycodeInt = config.GetInt(key);
		if (keycodeInt < 0) continue;

		const SDL_Keycode keycode = static_cast<SDL_Keycode>(keycodeInt);
		const SDL_Scancode scancode = SDL_GetScancodeFromKey(keycode);

		if (scancode != SDL_SCANCODE_UNKNOWN)
		{
			config.Set(key, static_cast<int32>(scancode));
		}
		else
		{
			const char* keyName = SDL_GetKeyName(keycode);
			if (keyName == nullptr) keyName = "unknown";

			const String& fieldName = Enum_GameConfigKeys::ToString(key);

			Logf("Unable to convert key \"%s\" (%d) into scancode, for config field \"%s\".", Logger::Severity::Error, keyName, keycode, fieldName.c_str());
			config.Set(key, -1);
		}
	}
}

GameConfig::GameConfig()
{
    //XXX We can't do clear here as it leads to UB with the initialization of hitstat static values
    // This sould be ok as Clear will be called in the Load function
}

void GameConfig::SetKeyBinding(GameConfigKeys key, Graphics::Key value)
{
	SetEnum<Enum_Key>(key, value);
}

void GameConfig::InitDefaults()
{
	// Do not set ConfigVersion to GameConfig::VERSION here. It will cause problems for config files with no ConfigVersion field.
	// For a new config, ConfigVersion will be set to the appropriate value in Application::m_LoadConfig.
	Set(GameConfigKeys::ConfigVersion, 0);

	Set(GameConfigKeys::ScreenWidth, 1280);
	Set(GameConfigKeys::ScreenHeight, 720);
	Set(GameConfigKeys::FullScreenWidth, -1);
	Set(GameConfigKeys::FullScreenHeight, -1);
	Set(GameConfigKeys::Fullscreen, false);
	Set(GameConfigKeys::FullscreenMonitorIndex, 0);
	Set(GameConfigKeys::WindowedFullscreen, false);
	Set(GameConfigKeys::AdjustWindowPositionOnStartup, true);
	Set(GameConfigKeys::AntiAliasing, 1);
	Set(GameConfigKeys::MasterVolume, 1.0f);
	Set(GameConfigKeys::FXVolume, 1.0f);
	Set(GameConfigKeys::SlamVolume, 1.0f);
	Set(GameConfigKeys::ScreenX, -1);
	Set(GameConfigKeys::ScreenY, -1);
	Set(GameConfigKeys::VSync, false);
	Set(GameConfigKeys::ShowFps, false);
	Set(GameConfigKeys::ForcePortrait, false);
	Set(GameConfigKeys::SkipScore, true);

	Set(GameConfigKeys::HitWindowPerfect, HitWindow::NORMAL.perfect);
	Set(GameConfigKeys::HitWindowGood, HitWindow::NORMAL.good);
	Set(GameConfigKeys::HitWindowHold, HitWindow::NORMAL.hold);
	Set(GameConfigKeys::HitWindowSlam, HitWindow::NORMAL.slam);
	Set(GameConfigKeys::HiSpeed, 1.0f);
	Set(GameConfigKeys::GlobalOffset, 0);
	Set(GameConfigKeys::InputOffset, 0);
	Set(GameConfigKeys::LaserOffset, 0);
	Set(GameConfigKeys::FPSTarget, 0);
	Set(GameConfigKeys::GaugeDrainNormal, 180);
	Set(GameConfigKeys::GaugeDrainHalf, 300);
	Set(GameConfigKeys::ModSpeed, 300.0f);
	Set(GameConfigKeys::AutoSaveSpeed, true);
	Set(GameConfigKeys::SongFolder, "songs");
	Set(GameConfigKeys::Skin, "Default");
	Set(GameConfigKeys::Laser0Color, 200.0f);
	Set(GameConfigKeys::Laser1Color, 330.0f);
	Set(GameConfigKeys::SongSelSensMult, 1.0f);
	Set(GameConfigKeys::EnableHiddenSudden, false);
	Set(GameConfigKeys::HiddenCutoff, 0.0f);
	Set(GameConfigKeys::HiddenFade, 0.2f);
	Set(GameConfigKeys::SuddenCutoff, 1.0f);
	Set(GameConfigKeys::SuddenFade, 0.2f);
	Set(GameConfigKeys::ShowCover, true);
	Set(GameConfigKeys::DistantButtonScale, 1.0f);
	Set(GameConfigKeys::BTOverFXScale, 0.8f);
	Set(GameConfigKeys::DisableBackgrounds, false);
	Set(GameConfigKeys::LeadInTime, 3000);
	Set(GameConfigKeys::PracticeLeadInTime, 1500);
	Set(GameConfigKeys::AutoComputeSongOffset, false);
	Set(GameConfigKeys::ResponsiveInputs, true);
	SetEnum<Enum_SongOffsetUpdateMethod>(GameConfigKeys::UpdateSongOffsetAfterFirstPlay, SongOffsetUpdateMethod::None);
	SetEnum<Enum_SongOffsetUpdateMethod>(GameConfigKeys::UpdateSongOffsetAfterEveryPlay, SongOffsetUpdateMethod::None);

	Set(GameConfigKeys::PracticeSetupNavEnabled, true);
	Set(GameConfigKeys::RevertToSetupAfterScoreScreen, false);
	Set(GameConfigKeys::DisplayPracticeInfoInGame, true);

	Set(GameConfigKeys::DefaultPlaybackSpeed, 100);
	Set(GameConfigKeys::DefaultLoopOnSuccess, true);
	Set(GameConfigKeys::DefaultLoopOnFail, true);
	Set(GameConfigKeys::DefaultIncSpeedOnSuccess, false);
	Set(GameConfigKeys::DefaultIncSpeedAmount, 2);
	Set(GameConfigKeys::DefaultIncStreak, 1);
	Set(GameConfigKeys::DefaultDecSpeedOnFail, false);
	Set(GameConfigKeys::DefaultDecSpeedAmount, 2);
	Set(GameConfigKeys::DefaultMinPlaybackSpeed, 50);
	Set(GameConfigKeys::DefaultEnableMaxRewind, false);
	Set(GameConfigKeys::DefaultMaxRewindMeasure, 1);

	Set(GameConfigKeys::DefaultFailConditionType, 0);
	Set(GameConfigKeys::DefaultFailConditionScore, (int32) MAX_SCORE);
	Set(GameConfigKeys::DefaultFailConditionGrade, 0);
	Set(GameConfigKeys::DefaultFailConditionMiss, 0);
	Set(GameConfigKeys::DefaultFailConditionMissNear, 0);
	Set(GameConfigKeys::DefaultFailConditionGauge, 0);

	SetEnum<Logger::Enum_Severity>(GameConfigKeys::LogLevel, Logger::Severity::Normal);

	SetEnum<Enum_SpeedMods>(GameConfigKeys::SpeedMod, SpeedMods::MMod);
	SetEnum<Enum_ScoreDisplayModes>(GameConfigKeys::ScoreDisplayMode, ScoreDisplayModes::Additive);

	// Input settings
	SetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice, InputDevice::Keyboard);
	SetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice, InputDevice::Keyboard);
	SetEnum<Enum_ButtonComboModeSettings>(GameConfigKeys::UseBackCombo, ButtonComboModeSettings::Hold);
	SetEnum<Enum_LaserAxisOption>(GameConfigKeys::InvertLaserInput, LaserAxisOption::None);

	// Default keyboard bindings
	Set(GameConfigKeys::Key_BTS, SDL_SCANCODE_1); // Start button on Dao controllers
	Set(GameConfigKeys::Key_BTSAlt, -1); // How about setting this to the return key, after dealing with hard-coded SDL_SCANCODE_RETURNs?
	Set(GameConfigKeys::Key_BT0, SDL_SCANCODE_D);
	Set(GameConfigKeys::Key_BT1, SDL_SCANCODE_F);
	Set(GameConfigKeys::Key_BT2, SDL_SCANCODE_J);
	Set(GameConfigKeys::Key_BT3, SDL_SCANCODE_K);
	Set(GameConfigKeys::Key_BT0Alt, -1);
	Set(GameConfigKeys::Key_BT1Alt, -1);
	Set(GameConfigKeys::Key_BT2Alt, -1);
	Set(GameConfigKeys::Key_BT3Alt, -1);
	Set(GameConfigKeys::Key_FX0, SDL_SCANCODE_C);
	Set(GameConfigKeys::Key_FX1, SDL_SCANCODE_M);
	Set(GameConfigKeys::Key_FX0Alt, -1);
	Set(GameConfigKeys::Key_FX1Alt, -1);
	Set(GameConfigKeys::Key_Laser0Neg, SDL_SCANCODE_W);
	Set(GameConfigKeys::Key_Laser0Pos, SDL_SCANCODE_E);
	Set(GameConfigKeys::Key_Laser1Neg, SDL_SCANCODE_O);
	Set(GameConfigKeys::Key_Laser1Pos, SDL_SCANCODE_P);
	Set(GameConfigKeys::Key_Laser0NegAlt, -1);
	Set(GameConfigKeys::Key_Laser0PosAlt, -1);
	Set(GameConfigKeys::Key_Laser1NegAlt, -1);
	Set(GameConfigKeys::Key_Laser1PosAlt, -1);
	Set(GameConfigKeys::Key_Back, SDL_SCANCODE_ESCAPE);
	Set(GameConfigKeys::Key_BackAlt, -1);
	Set(GameConfigKeys::Key_Sensitivity, 3.0f);
	Set(GameConfigKeys::Key_LaserReleaseTime, 0.0f);

	Set(GameConfigKeys::Key_SongSelect_Up, SDL_SCANCODE_UP),
	Set(GameConfigKeys::Key_SongSelect_Down, SDL_SCANCODE_DOWN),
	Set(GameConfigKeys::Key_SongSelect_FastUp, SDL_SCANCODE_PAGEUP),
	Set(GameConfigKeys::Key_SongSelect_FastDown, SDL_SCANCODE_PAGEDOWN),
	Set(GameConfigKeys::Key_SongSelect_Easier, SDL_SCANCODE_LEFT),
	Set(GameConfigKeys::Key_SongSelect_Harder, SDL_SCANCODE_RIGHT),
	Set(GameConfigKeys::Key_SongSelect_Collections, SDL_SCANCODE_F1),
	Set(GameConfigKeys::Key_SongSelect_Random, SDL_SCANCODE_F2),
	Set(GameConfigKeys::Key_SongSelect_ReloadSongs, SDL_SCANCODE_F5),
	Set(GameConfigKeys::Key_SongSelect_Demo, SDL_SCANCODE_F8),
	Set(GameConfigKeys::Key_SongSelect_OpenEditor, SDL_SCANCODE_F11),
	Set(GameConfigKeys::Key_SongSelect_OpenDirectory, SDL_SCANCODE_F12),
	Set(GameConfigKeys::Key_SongSelect_OpenSearch, SDL_SCANCODE_TAB),
	Set(GameConfigKeys::Key_SongSelect_CloseSearch, SDL_SCANCODE_RETURN),
	Set(GameConfigKeys::Key_SongSelect_StartPractice, SDL_SCANCODE_GRAVE),
	Set(GameConfigKeys::Key_Game_Bailout, SDL_SCANCODE_BACKSPACE),
	Set(GameConfigKeys::Key_Game_Pause, SDL_SCANCODE_PAUSE),
	Set(GameConfigKeys::Key_Game_SkipIntro, SDL_SCANCODE_RETURN),
	Set(GameConfigKeys::Key_Game_Advance, SDL_SCANCODE_PAGEUP),
	Set(GameConfigKeys::Key_Game_Restart, SDL_SCANCODE_F5),
	Set(GameConfigKeys::Key_Game_DebugHUD, SDL_SCANCODE_F8),
	Set(GameConfigKeys::Key_Result_Continue, SDL_SCANCODE_RETURN),
	Set(GameConfigKeys::Key_Result_Screenshot, SDL_SCANCODE_F12),
	Set(GameConfigKeys::Key_ReloadSkin, SDL_SCANCODE_F9),
	Set(GameConfigKeys::Key_ScrollLeft, SDL_SCANCODE_LEFT),
	Set(GameConfigKeys::Key_ScrollRight, SDL_SCANCODE_RIGHT),
	Set(GameConfigKeys::Key_ScrollUp, SDL_SCANCODE_UP),
	Set(GameConfigKeys::Key_ScrollDown, SDL_SCANCODE_DOWN),

	// Default controller settings
	SetBlob<16>(GameConfigKeys::Controller_DeviceID, { 0 }); // null device
	Set(GameConfigKeys::Controller_BTS, 0);
	Set(GameConfigKeys::Controller_BT0, 1);
	Set(GameConfigKeys::Controller_BT1, 2);
	Set(GameConfigKeys::Controller_BT2, 3);
	Set(GameConfigKeys::Controller_BT3, 4);
	Set(GameConfigKeys::Controller_FX0, 5);
	Set(GameConfigKeys::Controller_FX1, 6);
	Set(GameConfigKeys::Controller_Laser0Axis, 0);
	Set(GameConfigKeys::Controller_Laser1Axis, 1);
	Set(GameConfigKeys::Controller_Back, -1);
	Set(GameConfigKeys::Controller_Sensitivity, 1.0f);
	Set(GameConfigKeys::Controller_Deadzone, 0.f);
	Set(GameConfigKeys::Controller_DirectMode, false);

	// Default mouse settings
	Set(GameConfigKeys::Mouse_Laser0Axis, 0);
	Set(GameConfigKeys::Mouse_Laser1Axis, 1);
	Set(GameConfigKeys::Mouse_Sensitivity, 1.0f);

	// Default to 10ms input bounce guard
	Set(GameConfigKeys::InputBounceGuard, 10);

	SetEnum<Enum_AbortMethod>(GameConfigKeys::RestartPlayMethod, AbortMethod::Press);
	Set(GameConfigKeys::RestartPlayHoldDuration, 2000);

	SetEnum<Enum_AbortMethod>(GameConfigKeys::ExitPlayMethod, AbortMethod::Press);
	Set(GameConfigKeys::ExitPlayHoldDuration, 2000);

	Set(GameConfigKeys::DisableNonButtonInputsDuringPlay, false);

	Set(GameConfigKeys::LastSelected, 0);
	Set(GameConfigKeys::LastSelectedChal, 0);
	Set(GameConfigKeys::LastSort, 0);
	Set(GameConfigKeys::LastSortChal, 0);
	Set(GameConfigKeys::LevelFilter, 0);
	Set(GameConfigKeys::LevelFilterChal, 0);
	Set(GameConfigKeys::FolderFilter, 0);

	Set(GameConfigKeys::AutoResetSettings, false);
	Set(GameConfigKeys::AutoResetToSpeed, 400.0f);
	Set(GameConfigKeys::SlamThicknessMultiplier, 1.0f);
	Set(GameConfigKeys::DelayedHitEffects, true);

	SetEnum<Enum_AutoScoreScreenshotSettings>(GameConfigKeys::AutoScoreScreenshot, AutoScoreScreenshotSettings::Off);

	Set(GameConfigKeys::EditorPath, "PathToEditor");
	Set(GameConfigKeys::EditorParamsFormat, "%s");
	Set(GameConfigKeys::WASAPI_Exclusive, false);
	Set(GameConfigKeys::MuteUnfocused, false);
	Set(GameConfigKeys::PrerenderEffects, false);

	Set(GameConfigKeys::CheckForUpdates, true);
	Set(GameConfigKeys::OnlyRelease, true); // deprecated
	Set(GameConfigKeys::LimitSettingsFont, false);

	// Multiplayer
	Set(GameConfigKeys::MultiplayerHost, "usc-multi.drewol.me:39079");
	Set(GameConfigKeys::MultiplayerPassword, "");
	Set(GameConfigKeys::MultiplayerUsername, "");

	Set(GameConfigKeys::EnableFancyHighwayRoll, true);

	// IR
	Set(GameConfigKeys::IRBaseURL, "");
	Set(GameConfigKeys::IRToken, "");
	Set(GameConfigKeys::IRLowBandwidth, false);

	//Gameplay
	Set(GameConfigKeys::RandomizeChart, false);
	Set(GameConfigKeys::MirrorChart, false);
	SetEnum<Enum_GaugeTypes>(GameConfigKeys::GaugeType, GaugeTypes::Normal);
	Set(GameConfigKeys::BackupGauge, false);
	Set(GameConfigKeys::BlastiveLevel, 1);

	Set(GameConfigKeys::GameplaySettingsDialogLastTab, 0);
	Set(GameConfigKeys::SettingsLastTab, 0);
	Set(GameConfigKeys::TransferScoresOnChartUpdate, true);
	Set(GameConfigKeys::FastGUI, false);
	Set(GameConfigKeys::SkinDevMode, false);

	Set(GameConfigKeys::CurrentProfileName, "Main");
	Set(GameConfigKeys::UpdateChannel, "master");

#ifndef EMBEDDED
	Set(GameConfigKeys::KeepFontTexture, true);
#else
	Set(GameConfigKeys::KeepFontTexture, false);
#endif
}

void GameConfig::UpdateVersion()
{
	int32 configVersion = GetInt(GameConfigKeys::ConfigVersion);
	if (configVersion == GameConfig::VERSION) return;

	// Abnormal cases
	if (configVersion < 0)
	{
		Logf("The version of the config(%d) is invalid.", Logger::Severity::Error, configVersion);
		return;
	}
	if (configVersion > GameConfig::VERSION)
	{
		Logf("The version of the config(%d) is higher than the maximum compatible version(%d). Try updating USC.",
			Logger::Severity::Error, configVersion, GameConfig::VERSION);
		return;
	}

	Logf("Updating the version of GameConfig from %d to %d...", Logger::Severity::Normal, configVersion, GameConfig::VERSION);

	/* Config Conversion Code Collection */

	// 0 -> 1: button keys should be converted from keycodes to scancodes.
	if (configVersion == 0)
	{
		ConvertKeyCodeToScanCode(*this, {
			GameConfigKeys::Key_BTS,
			GameConfigKeys::Key_BT0,
			GameConfigKeys::Key_BT1,
			GameConfigKeys::Key_BT2,
			GameConfigKeys::Key_BT3,
			GameConfigKeys::Key_BT0Alt,
			GameConfigKeys::Key_BT1Alt,
			GameConfigKeys::Key_BT2Alt,
			GameConfigKeys::Key_BT3Alt,
			GameConfigKeys::Key_FX0,
			GameConfigKeys::Key_FX1,
			GameConfigKeys::Key_FX0Alt,
			GameConfigKeys::Key_FX1Alt,
			GameConfigKeys::Key_Laser0Neg,
			GameConfigKeys::Key_Laser0Pos,
			GameConfigKeys::Key_Laser1Neg,
			GameConfigKeys::Key_Laser1Pos,
			GameConfigKeys::Key_Back,
			GameConfigKeys::Key_BackAlt,
		});

		++configVersion;
	}

	// 1 -> 2: Convert mouse sensitivity from old range to new range.
	if (configVersion == 1)
	{
		float oldSens = GetFloat(GameConfigKeys::Mouse_Sensitivity);
		float newSens = static_cast<float>(Input::CalculateSensFromPpr(6.0f / oldSens));
		Logf("Recalculated mouse sensitivity: %.4f -> %.4f", Logger::Severity::Info, oldSens, newSens);

		Set(GameConfigKeys::Mouse_Sensitivity, newSens);
		++configVersion;
	}

	assert(configVersion == GameConfig::VERSION);
	Set(GameConfigKeys::ConfigVersion, configVersion);
}

#define Key(v) static_cast<uint32>(GameConfigKeys::v)
ConfigBase::KeyList GameConfigProfileSettings = {
	Key(HitWindowPerfect),
	Key(HitWindowGood),
	Key(HitWindowHold),
	Key(HitWindowSlam),
	Key(GlobalOffset),
	Key(InputOffset),
	Key(LaserOffset),

	Key(HiddenCutoff),
	Key(HiddenFade),
	Key(SuddenCutoff),
	Key(SuddenFade),

	Key(UseBackCombo),
	Key(LaserInputDevice),
	Key(ButtonInputDevice),
	Key(Mouse_Laser0Axis),
	Key(Mouse_Laser1Axis),
	Key(Mouse_Sensitivity),

	Key(Key_BTS),
	Key(Key_BTSAlt),
	Key(Key_BT0),
	Key(Key_BT1),
	Key(Key_BT2),
	Key(Key_BT3),
	Key(Key_BT0Alt),
	Key(Key_BT1Alt),
	Key(Key_BT2Alt),
	Key(Key_BT3Alt),
	Key(Key_FX0),
	Key(Key_FX1),
	Key(Key_FX0Alt),
	Key(Key_FX1Alt),
	Key(Key_Laser0Pos),
	Key(Key_Laser0Neg),
	Key(Key_Laser1Pos),
	Key(Key_Laser1Neg),
	Key(Key_Laser0PosAlt),
	Key(Key_Laser0NegAlt),
	Key(Key_Laser1PosAlt),
	Key(Key_Laser1NegAlt),
	Key(Key_Back),
	Key(Key_BackAlt),
	Key(Key_Sensitivity),
	Key(Key_LaserReleaseTime),

	Key(Key_SongSelect_Up),
	Key(Key_SongSelect_Down),
	Key(Key_SongSelect_FastUp),
	Key(Key_SongSelect_FastDown),
	Key(Key_SongSelect_Easier),
	Key(Key_SongSelect_Harder),
	Key(Key_SongSelect_Collections),
	Key(Key_SongSelect_Random),
	Key(Key_SongSelect_ReloadSongs),
	Key(Key_SongSelect_Demo),
	Key(Key_SongSelect_OpenEditor),
	Key(Key_SongSelect_OpenDirectory),
	Key(Key_SongSelect_OpenSearch),
	Key(Key_SongSelect_CloseSearch),
	Key(Key_SongSelect_StartPractice),
	Key(Key_Game_Bailout),
	Key(Key_Game_Pause),
	Key(Key_Game_SkipIntro),
	Key(Key_Game_Advance),
	Key(Key_Game_Restart),
	Key(Key_Game_DebugHUD),
	Key(Key_Result_Continue),
	Key(Key_Result_Screenshot),
	Key(Key_ReloadSkin),
	Key(Key_ScrollLeft),
	Key(Key_ScrollRight),
	Key(Key_ScrollUp),
	Key(Key_ScrollDown),

	Key(Controller_DeviceID),
	Key(Controller_BTS),
	Key(Controller_BT0),
	Key(Controller_BT1),
	Key(Controller_BT2),
	Key(Controller_BT3),
	Key(Controller_FX0),
	Key(Controller_FX1),
	Key(Controller_Back),
	Key(Controller_Laser0Axis),
	Key(Controller_Laser1Axis),
	Key(Controller_Deadzone),
	Key(Controller_DirectMode),
	Key(Controller_Sensitivity),
	Key(InputBounceGuard),
	Key(SongSelSensMult),
	Key(InvertLaserInput),

	Key(RestartPlayMethod),
	Key(RestartPlayHoldDuration),
	Key(ExitPlayMethod),
	Key(ExitPlayHoldDuration),
	Key(DisableNonButtonInputsDuringPlay),

	Key(MultiplayerUsername)
};
#undef Key
