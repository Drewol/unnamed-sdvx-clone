#include "stdafx.h"
#include "GameConfig.hpp"
#include "SDL2/SDL_keycode.h"

GameConfig::GameConfig()
{
	// Default state
	Clear();
}

void GameConfig::SetKeyBinding(GameConfigKeys key, Graphics::Key value)
{
	SetEnum<Enum_Key>(key, value);
}

void GameConfig::InitDefaults()
{
	Set(GameConfigKeys::ScreenWidth, 1280);
	Set(GameConfigKeys::ScreenHeight, 720);
	Set(GameConfigKeys::FullScreenWidth, -1);
	Set(GameConfigKeys::FullScreenHeight, -1);
	Set(GameConfigKeys::Fullscreen, false);
	Set(GameConfigKeys::FullscreenMonitorIndex, 0);
	Set(GameConfigKeys::WindowedFullscreen, false);
	Set(GameConfigKeys::AntiAliasing, 0);
	Set(GameConfigKeys::MasterVolume, 1.0f);
	Set(GameConfigKeys::ScreenX, -1);
	Set(GameConfigKeys::ScreenY, -1);
	Set(GameConfigKeys::VSync, false);
	Set(GameConfigKeys::ShowFps, false);
	Set(GameConfigKeys::ForcePortrait, false);
	Set(GameConfigKeys::SkipScore, true);
	Set(GameConfigKeys::HiSpeed, 1.0f);
	Set(GameConfigKeys::GlobalOffset, 0);
	Set(GameConfigKeys::InputOffset, 0);
	Set(GameConfigKeys::FPSTarget, 0);
	Set(GameConfigKeys::LaserAssistLevel, 1.5f);
	Set(GameConfigKeys::LaserPunish, 1.5f);
	Set(GameConfigKeys::LaserSlamBoost, 2.0f);
	Set(GameConfigKeys::LaserChangeTime, 50.0f);
	Set(GameConfigKeys::LaserChangeExponent, 1.0f);
	Set(GameConfigKeys::GaugeDrainNormal, 180);
	Set(GameConfigKeys::GaugeDrainHalf, 300);
	Set(GameConfigKeys::ModSpeed, 300.0f);
	Set(GameConfigKeys::SongFolder, "songs");
	Set(GameConfigKeys::Skin, "Default");
	Set(GameConfigKeys::Laser0Color, 200.0f);
	Set(GameConfigKeys::Laser1Color, 330.0f);

	SetEnum<Enum_SpeedMods>(GameConfigKeys::SpeedMod, SpeedMods::XMod);

	// Input settings
	SetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice, InputDevice::Keyboard);
	SetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice, InputDevice::Keyboard);

	// Default keyboard bindings
	Set(GameConfigKeys::Key_BTS, SDLK_1); // Start button on Dao controllers
	Set(GameConfigKeys::Key_BT0, SDLK_d);
	Set(GameConfigKeys::Key_BT1, SDLK_f);
	Set(GameConfigKeys::Key_BT2, SDLK_j);
	Set(GameConfigKeys::Key_BT3, SDLK_k);
	Set(GameConfigKeys::Key_BT0Alt, -1);
	Set(GameConfigKeys::Key_BT1Alt, -1);
	Set(GameConfigKeys::Key_BT2Alt, -1);
	Set(GameConfigKeys::Key_BT3Alt, -1);
	Set(GameConfigKeys::Key_FX0, SDLK_c);
	Set(GameConfigKeys::Key_FX1, SDLK_m);
	Set(GameConfigKeys::Key_FX0Alt, -1);
	Set(GameConfigKeys::Key_FX1Alt, -1);
	Set(GameConfigKeys::Key_Laser0Neg, SDLK_w);
	Set(GameConfigKeys::Key_Laser0Pos, SDLK_e);
	Set(GameConfigKeys::Key_Laser1Neg, SDLK_o);
	Set(GameConfigKeys::Key_Laser1Pos, SDLK_p);
	Set(GameConfigKeys::Key_Sensitivity, 3.0f);
	Set(GameConfigKeys::Key_LaserReleaseTime, 0.0f);

	// Default controller settings
	Set(GameConfigKeys::Controller_DeviceID, 0); // First device
	Set(GameConfigKeys::Controller_BTS, 0);
	Set(GameConfigKeys::Controller_BT0, 1);
	Set(GameConfigKeys::Controller_BT1, 2);
	Set(GameConfigKeys::Controller_BT2, 3);
	Set(GameConfigKeys::Controller_BT3, 4);
	Set(GameConfigKeys::Controller_FX0, 5);
	Set(GameConfigKeys::Controller_FX1, 6);
	Set(GameConfigKeys::Controller_Laser0Axis, 0);
	Set(GameConfigKeys::Controller_Laser1Axis, 1);
	Set(GameConfigKeys::Controller_Sensitivity, 1.0f);
	Set(GameConfigKeys::Controller_Deadzone, 0.f);
	Set(GameConfigKeys::Controller_DirectMode, false);

	// Default mouse settings
	Set(GameConfigKeys::Mouse_Laser0Axis, 0);
	Set(GameConfigKeys::Mouse_Laser1Axis, 1);
	Set(GameConfigKeys::Mouse_Sensitivity, 1.0f);

	// Default to 10ms input bounce guard
	Set(GameConfigKeys::InputBounceGuard, 10);

	Set(GameConfigKeys::LastSelected, 0);
	Set(GameConfigKeys::LevelFilter, 0);
	Set(GameConfigKeys::FolderFilter, 0);

	Set(GameConfigKeys::AutoResetSettings, false);
	Set(GameConfigKeys::AutoResetToSpeed, 400.0f);
	Set(GameConfigKeys::SlamThicknessMultiplier, 1.0f);

	SetEnum<Enum_AutoScoreScreenshotSettings>(GameConfigKeys::AutoScoreScreenshot, AutoScoreScreenshotSettings::Off);

	Set(GameConfigKeys::EditorPath, "PathToEditor");
	Set(GameConfigKeys::EditorParamsFormat, "%s");
	Set(GameConfigKeys::WASAPI_Exclusive, false);

	Set(GameConfigKeys::CheckForUpdates, true);
	Set(GameConfigKeys::OnlyRelease, true);

	// Multiplayer
	Set(GameConfigKeys::MultiplayerHost, "usc-multi.drewol.me:39079");
	Set(GameConfigKeys::MultiplayerPassword, "");
	Set(GameConfigKeys::MultiplayerUsername, "");
}
