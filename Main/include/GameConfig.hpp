#pragma once
#include "Shared/Config.hpp"
#include "Input.hpp"

DefineEnum(GameConfigKeys,
		   // Version of the config
		   ConfigVersion,

		   // Screen settings
		   ScreenWidth,
		   ScreenHeight,
		   FullScreenWidth,
		   FullScreenHeight,
		   ScreenX,
		   ScreenY,
		   Fullscreen,
		   FullscreenMonitorIndex,
		   WindowedFullscreen,
		   AntiAliasing,
		   MasterVolume,
		   VSync,
		   ShowFps,
		   ForcePortrait,
	       LogLevel,

		   // Game settings
		   HiSpeed,
		   SpeedMod,
		   ModSpeed,
		   AutoSaveSpeed,
		   SkipScore,
		   GlobalOffset,
		   InputOffset,
		   SongFolder,
		   Skin,
		   Laser0Color,
		   Laser1Color,
		   FPSTarget,
		   LaserAssistLevel,
		   LaserPunish,
		   LaserChangeTime,
		   LaserChangeExponent,
		   GaugeDrainNormal,
		   GaugeDrainHalf,

		   HiddenCutoff,
		   HiddenFade,
		   SuddenCutoff,
		   SuddenFade,
		   ShowCover,
		   UseBackCombo,
		   DistantButtonScale,
		   BTOverFXScale,
		   DisableBackgrounds,

		   // Input device setting per element
		   LaserInputDevice,
		   ButtonInputDevice,

		   // Mouse settings (primary axes are x=0, y=1)
		   Mouse_Laser0Axis,
		   Mouse_Laser1Axis,
		   Mouse_Sensitivity,

		   // Key bindings
		   Key_BTS,
		   Key_BTSAlt,
		   Key_BT0,
		   Key_BT1,
		   Key_BT2,
		   Key_BT3,
		   Key_BT0Alt,
		   Key_BT1Alt,
		   Key_BT2Alt,
		   Key_BT3Alt,
		   Key_FX0,
		   Key_FX1,
		   Key_FX0Alt,
		   Key_FX1Alt,
		   Key_Laser0Pos,
		   Key_Laser0Neg,
		   Key_Laser1Pos,
		   Key_Laser1Neg,
		   Key_Laser0PosAlt,
		   Key_Laser0NegAlt,
		   Key_Laser1PosAlt,
		   Key_Laser1NegAlt,
		   Key_Back,
		   Key_BackAlt,
		   Key_Sensitivity,
		   Key_LaserReleaseTime,

		   // Controller bindings
		   Controller_DeviceID,
		   Controller_BTS,
		   Controller_BT0,
		   Controller_BT1,
		   Controller_BT2,
		   Controller_BT3,
		   Controller_FX0,
		   Controller_FX1,
		   Controller_Back,
		   Controller_Laser0Axis,
		   Controller_Laser1Axis,
		   Controller_Deadzone,
		   Controller_DirectMode,
		   Controller_Sensitivity,
		   InputBounceGuard,
		   SongSelSensMult,

		   // In-Game Abort
		   RestartPlayMethod,
		   RestartPlayHoldDuration,
		   ExitPlayMethod,
		   ExitPlayHoldDuration,
		   DisableNonButtonInputsDuringPlay, // TODO: after enabling key customization for non-button commands, remove this.

		   LastSelected,
		   LastSort,
		   LevelFilter,
		   FolderFilter,

		   AutoResetSettings,		//Reset game settings after each song (good for convention setups)
		   AutoResetToSpeed,		//Mod-Speed to reset to after each song (when AutoResetSettings is true)
		   SlamThicknessMultiplier, //TODO: Remove after better values have been found(?)

		   EditorPath,
		   EditorParamsFormat,

		   AutoScoreScreenshot,

		   WASAPI_Exclusive,
		   MuteUnfocused,

		   CheckForUpdates,
		   OnlyRelease,
		   LimitSettingsFont,

		   // Multiplayer
		   MultiplayerHost,
		   MultiplayerPassword,
		   MultiplayerUsername,

		   EnableFancyHighwayRoll,

		   GameplaySettingsDialogLastTab,
		   TransferScoresOnChartUpdate,

		   // Gameplay options
		   GaugeType,
		   MirrorChart,
		   RandomizeChart,
		   
		   // Internet Ranking
		   IRBaseURL,
		   IRUsername,
		   IRPassword)

DefineEnum(GaugeTypes,
		   Normal,
		   Hard)

DefineEnum(SpeedMods,
		   XMod,
		   MMod,
		   CMod)

DefineEnum(AbortMethod,
		   None,
		   Press,
		   Hold)

#ifdef Always
#undef Always
#endif
DefineEnum(AutoScoreScreenshotSettings,
		   Off,
		   Highscore,
		   Always)

DefineEnum(ButtonComboModeSettings,
		   Disabled,
		   Hold,
		   Instant)

	// Config for game settings
	class GameConfig : public Config<Enum_GameConfigKeys>
{
public:
	GameConfig();
	void SetKeyBinding(GameConfigKeys key, Key value);


	// When this should change, the UpdateVersion MUST be updated to update the old config files.
	// If there's no need to update the UpdateVersion, there's no need to touch this too.
	constexpr static int32 VERSION = 1;

	// Update the version of the config file to VERSION.
	void UpdateVersion();

protected:
	virtual void InitDefaults() override;
};

// Main config instance
extern class GameConfig g_gameConfig;
