#pragma once
#include "Shared/Config.hpp"
#include "Input.hpp"

#ifdef Always
#undef Always
#endif

#ifdef None
#undef None
#endif

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
		   AdjustWindowPositionOnStartup,

		   AntiAliasing,
		   MasterVolume,
	       FXVolume,
	       SlamVolume,
		   VSync,
		   ShowFps,
		   ForcePortrait,
		   LogLevel,

		   // Game settings
		   HitWindowPerfect,
		   HitWindowGood,
		   HitWindowHold,
		   HitWindowSlam,
		   HiSpeed,
		   SpeedMod,
		   ModSpeed,
		   AutoSaveSpeed,
		   SkipScore,
		   GlobalOffset,
		   InputOffset,
		   LaserOffset,
		   SongFolder,
		   Skin,
		   Laser0Color,
		   Laser1Color,
		   FPSTarget,
		   GaugeDrainNormal,
		   GaugeDrainHalf,
		   ResponsiveInputs,

		   EnableHiddenSudden,
		   HiddenCutoff,
		   HiddenFade,
		   SuddenCutoff,
		   SuddenFade,
		   ShowCover,
		   UseBackCombo,
		   DistantButtonScale,
		   BTOverFXScale,
		   DisableBackgrounds,
		   ScoreDisplayMode,
		   AutoComputeSongOffset,
		   UpdateSongOffsetAfterFirstPlay,
		   UpdateSongOffsetAfterEveryPlay,

		   LeadInTime,
		   PracticeLeadInTime,

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

		   // UI navigation bindings
		   Key_SongSelect_Up,
		   Key_SongSelect_Down,
		   Key_SongSelect_FastUp,
		   Key_SongSelect_FastDown,
		   Key_SongSelect_Easier,
		   Key_SongSelect_Harder,
		   Key_SongSelect_Collections,
		   Key_SongSelect_Random,
		   Key_SongSelect_ReloadSongs,
		   Key_SongSelect_Demo,
		   Key_SongSelect_OpenEditor,
		   Key_SongSelect_OpenDirectory,
		   Key_SongSelect_OpenSearch,
		   Key_SongSelect_CloseSearch,
		   Key_SongSelect_StartPractice,
		   Key_Game_Bailout,
		   Key_Game_Pause,
		   Key_Game_SkipIntro,
		   Key_Game_Advance,
		   Key_Game_Restart,
		   Key_Game_DebugHUD,
		   Key_Result_Continue,
		   Key_Result_Screenshot,
		   Key_ReloadSkin,
		   Key_ScrollLeft,
		   Key_ScrollRight,
		   Key_ScrollUp,
		   Key_ScrollDown,

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
		   InvertLaserInput,

		   // In-Game Abort
		   RestartPlayMethod,
		   RestartPlayHoldDuration,
		   ExitPlayMethod,
		   ExitPlayHoldDuration,
		   DisableNonButtonInputsDuringPlay, // TODO: after enabling key customization for non-button commands, remove this.

		   LastSelected,
		   LastSelectedChal,
		   LastSort,
		   LastSortChal,
		   LevelFilter,
		   LevelFilterChal,
		   FolderFilter,

		   AutoResetSettings,		//Reset game settings after each song (good for convention setups)
		   AutoResetToSpeed,		//Mod-Speed to reset to after each song (when AutoResetSettings is true)
		   SlamThicknessMultiplier, //TODO: Remove after better values have been found(?)
		   DelayedHitEffects,		// TODO: Think of a better name

		   EditorPath,
		   EditorParamsFormat,

		   AutoScoreScreenshot,

		   WASAPI_Exclusive,
		   MuteUnfocused,
		   PrerenderEffects,

		   CheckForUpdates,
		   OnlyRelease,
		   LimitSettingsFont,

		   // Practice Mode
		   PracticeSetupNavEnabled,
		   RevertToSetupAfterScoreScreen,
		   DisplayPracticeInfoInGame,

		   DefaultPlaybackSpeed,
		   DefaultLoopOnSuccess,
		   DefaultLoopOnFail,
		   DefaultIncSpeedOnSuccess,
		   DefaultIncSpeedAmount,
		   DefaultIncStreak,
		   DefaultDecSpeedOnFail,
		   DefaultDecSpeedAmount,
		   DefaultMinPlaybackSpeed,
		   DefaultEnableMaxRewind,
		   DefaultMaxRewindMeasure,

		   DefaultFailConditionType,
		   DefaultFailConditionScore,
		   DefaultFailConditionGrade,
		   DefaultFailConditionMiss,
		   DefaultFailConditionMissNear,
		   DefaultFailConditionGauge,

		   // Multiplayer
		   MultiplayerHost,
		   MultiplayerPassword,
		   MultiplayerUsername,

		   IRBaseURL,
		   IRToken,
		   IRLowBandwidth,

		   EnableFancyHighwayRoll,

		   GameplaySettingsDialogLastTab,
		   SettingsLastTab,
		   TransferScoresOnChartUpdate,

		   KeepFontTexture,

		   CurrentProfileName,
		   FastGUI,
		   SkinDevMode,

		   // Gameplay options
		   GaugeType,
		   BlastiveLevel,
		   MirrorChart,
		   RandomizeChart,
		   BackupGauge,
		   UpdateChannel)

// List of settings overriden by profiles
extern ConfigBase::KeyList GameConfigProfileSettings;

DefineEnum(GaugeTypes,
		   Normal,
		   Hard,
		   Permissive,
	       Blastive)

DefineEnum(SpeedMods,
		   XMod,
		   MMod,
		   CMod)

DefineEnum(AbortMethod,
		   None,
		   Press,
		   Hold)

DefineEnum(ScoreDisplayModes,
		   Additive,
		   Subtractive,
		   Average)

DefineEnum(LaserAxisOption,
	       None,
	       Left,
	       Right,
	       Both)

DefineEnum(AutoScoreScreenshotSettings,
		   Off,
		   Highscore,
		   Always)

DefineEnum(SongOffsetUpdateMethod,
		   None,
		   Play,
		   PlayWholeChart,
		   Clear)

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

	static int32 VERSION;

	// Update the version of the config file to VERSION.
	void UpdateVersion();

protected:
	virtual void InitDefaults() override;
};

// Main config instance
extern class GameConfig g_gameConfig;
