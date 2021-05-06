#include "stdafx.h"
#include "Game.hpp"
#include "Application.hpp"

#include <array>
#include <random>
#include <unordered_set>
#include <Beatmap/BeatmapPlayback.hpp>
#include <Shared/Profiling.hpp>
#include <Audio/Audio.hpp>

#include "Scoring.hpp"
#include "Track.hpp"
#include "Camera.hpp"
#include "Background.hpp"
#include "AudioPlayback.hpp"
#include "Input.hpp"
#include "SongSelect.hpp"
#include "ChallengeSelect.hpp"
#include "ScoreScreen.hpp"
#include "TransitionScreen.hpp"
#include "AsyncAssetLoader.hpp"
#include "MultiplayerScreen.hpp"
#include "GameConfig.hpp"
#include <Shared/Time.hpp>
#include "Gauge.hpp"

#include "PracticeModeSettingsDialog.hpp"
#include "Audio/OffsetComputer.hpp"

// Try load map helper
Ref<Beatmap> TryLoadMap(const String& path)
{
	// Load map file
	Beatmap* newMap = new Beatmap();
	File mapFile;
	if(!mapFile.OpenRead(path))
	{
		delete newMap;
		return Ref<Beatmap>();
	}
	FileReader reader(mapFile);
	if(!newMap->Load(reader))
	{
		delete newMap;
		return Ref<Beatmap>();
	}
	return Ref<Beatmap>(newMap);
}

/* 
	Game implementation class
*/
class Game_Impl : public Game
{
public:
	// Startup parameters
	String m_chartRootPath;
	String m_chartPath;
	ChartIndex* m_chartIndex = nullptr;

private:
	bool m_playing = true;
	bool m_started = false;
	bool m_demo = false;
	bool m_introCompleted = false;
	bool m_outroCompleted = false;
	bool m_paused = false;
	bool m_triggerPause = false; // Whether to trigger a pause on next first gameplay tick
	bool m_triggerEnd = false; // Whether the end of the chart is reached
	bool m_ended = false;
	bool m_transitioning = false;
	bool m_saveSpeed = false;

	bool m_renderDebugHUD = false;

	MultiplayerScreen* m_multiplayer = nullptr;
	ChallengeManager* m_challengeManager = nullptr;

	// Making this into a separate class would be better, but it will (obviously) take a lot of work.
	// If you who are reading this comment have a lot of free time, feel free to refactor Game_Impl. :)
	bool m_isPracticeSetup = false;
	bool m_isPracticeMode = false;
	bool m_isPracticeSetupNavEnabled = false;
	std::unique_ptr<PracticeModeSettingsDialog> m_practiceSetupDialog = nullptr;
	bool m_playOnDialogClose = false; // Whether to unpause on dialog closing
	unsigned int m_loopCount = 0;
	unsigned int m_loopSuccess = 0;
	unsigned int m_loopStreak = 0;

	// Map object approach speed, scaled by BPM
	float m_hispeed = 1.0f;
	float m_hispeedAdvance = 0.f;

	// Current lane toggle status
	bool m_hideLane = false;

	// Use m-mod and what m-mod speed
	SpeedMods m_speedMod;
	float m_modSpeed = 400;

	bool m_delayedHitEffects;

	// Texture of the map jacket image, if available
	Image m_jacketImage;
	Texture m_jacketTexture;

	// The beatmap
	Ref<Beatmap> m_beatmap;
	// Scoring system object
	Scoring m_scoring;
	// Beatmap playback manager (object and timing point selector)
	BeatmapPlayback m_playback;
	// Audio playback manager (music and FX))
	AudioPlayback m_audioPlayback;
	// Applied audio offset
	int32 m_globalOffset = 0;
	int32 m_songOffset = 0;
	int32 m_tempOffset = 0;

	int32 m_fpsTarget = 0;
	// The play field
	Track* m_track = nullptr;

	PlayOptions m_playOptions;
	MapTimeRange m_practiceSetupRange = { 0, 0 };

	// Current position
	MapTime m_lastMapTime = 0;
	// End of the map
	MapTime m_endTime = 0;

	// The camera watching the playfield
	Camera m_camera;

	MouseLockHandle m_lockMouse;

	// Current background visualization
	Background* m_background = nullptr;
	Background* m_foreground = nullptr;

	// Lua state
	lua_State* m_lua = nullptr;

	// Currently active timing point
	const TimingPoint* m_currentTiming;
	// Currently visible gameplay objects
	Vector<ObjectState*> m_currentObjectSet;

	// Combo gain animation
	Timer m_comboAnimation;

	Sample m_slamSample;
	Sample m_clickSamples[2];
	Sample* m_fxSamples = nullptr;

	// Roll intensity, default = 1
	float m_rollIntensity = MAX_ROLL_ANGLE;
	bool m_manualTiltEnabled = false;

	// Particle effects
	Material particleMaterial;
	Texture basicParticleTexture;
	ParticleSystem m_particleSystem;
	Ref<ParticleEmitter> m_laserFollowEmitters[2];
	Ref<ParticleEmitter> m_holdEmitters[6];

	bool m_manualExit = false;
	bool m_showCover = true;

	Vector<ScoreReplay> m_scoreReplays;
	MapDatabase* m_db;
	std::unordered_set<ObjectState*> m_hiddenObjects;

	// Hold detection for restart and exit
	MapTime m_restartTriggerTime = 0;
	bool m_restartTriggerTimeSet = false;
	MapTime m_exitTriggerTime = 0;
	bool m_exitTriggerTimeSet = false;

	HitWindow m_hitWindow = HitWindow::NORMAL;

public:
	Game_Impl(const String& mapPath, PlayOptions&& options) : m_playOptions(std::move(options))
	{
		// Store path to map
		m_chartPath = Path::Normalize(mapPath);
		// Get Parent path
		m_chartRootPath = Path::RemoveLast(m_chartPath, nullptr);

		m_hispeed = g_gameConfig.GetFloat(GameConfigKeys::HiSpeed);
		m_speedMod = g_gameConfig.GetEnum<Enum_SpeedMods>(GameConfigKeys::SpeedMod);
		m_modSpeed = g_gameConfig.GetFloat(GameConfigKeys::ModSpeed);
	}

	Game_Impl(ChartIndex* chart, PlayOptions&& options) : m_playOptions(std::move(options))
	{
		// Store path to map
		m_chartPath = Path::Normalize(chart->path);
		m_chartIndex = chart;

		// Get Parent path
		m_chartRootPath = Path::RemoveLast(m_chartPath, nullptr);

		m_hispeed = g_gameConfig.GetFloat(GameConfigKeys::HiSpeed);
		m_speedMod = g_gameConfig.GetEnum<Enum_SpeedMods>(GameConfigKeys::SpeedMod);
		m_modSpeed = g_gameConfig.GetFloat(GameConfigKeys::ModSpeed);
	}

	~Game_Impl()
	{
        delete m_track;
		delete m_background;
		delete m_foreground;
		if (m_lua)
		{
			g_application->DisposeLua(m_lua);
			// Clear the state we stored in in the multiplayer's socket
			if (m_multiplayer != nullptr)
				m_multiplayer->GetTCP().ClearState(m_lua);
		}
		delete[] m_fxSamples;
		
		// Save hispeed
		if (m_saveSpeed)
		{
			g_gameConfig.Set(GameConfigKeys::HiSpeed, m_hispeed);
		}

		// Save practice indices
		// TODO: this crashes when the window is closed during gaming
		// because m_db is freed (which is owned by SongSelect) before this destructor is called.
		if (m_db && m_isPracticeMode)
		{
			StorePracticeSetupIndex();
		}

		// In case the cursor was still hidden
		g_gameWindow->SetCursorVisible(true); 
		g_input.OnButtonPressed.RemoveAll(this);
		g_transition->OnLoadingComplete.RemoveAll(this);
	}

	AsyncAssetLoader loader;
	virtual bool AsyncLoad() override
	{
		ProfilerScope $("AsyncLoad Game");

		// If CMod is not allowed, switch to MMod
		if (IsChallenge() && m_speedMod == SpeedMods::CMod
				&& !m_challengeManager->GetCurrentOptions().allow_cmod.Get(true))
			m_speedMod = SpeedMods::MMod;

		if(!Path::FileExists(m_chartPath))
		{
			Logf("Couldn't find chart at %s", Logger::Severity::Error, m_chartPath);
			return false;
		}

		m_beatmap = TryLoadMap(m_chartPath);

		// Check failure of above loading attempts
		if(!m_beatmap)
		{
			Log("Failed to load map", Logger::Severity::Warning);
			return false;
		}

		// Enable debug functionality
		if(g_application->GetAppCommandLine().Contains("-debug"))
		{
			m_renderDebugHUD = true;
		}
				
		m_endTime = m_beatmap->GetLastObjectTime();

		if (IsMultiplayerGame())
			m_hitWindow = HitWindow::NORMAL;
		else if (IsChallenge())
		{
			m_hitWindow = HitWindow(
				m_challengeManager->GetCurrentOptions().crit_judge.Get(
					g_gameConfig.GetInt(GameConfigKeys::HitWindowPerfect)
				),
				m_challengeManager->GetCurrentOptions().near_judge.Get(
					g_gameConfig.GetInt(GameConfigKeys::HitWindowGood)
				),
				m_challengeManager->GetCurrentOptions().hold_judge.Get(
					g_gameConfig.GetInt(GameConfigKeys::HitWindowHold)
				),
				m_challengeManager->GetCurrentOptions().slam_judge.Get(
					g_gameConfig.GetInt(GameConfigKeys::HitWindowSlam)
				)
			);
		}
		else
			m_hitWindow = HitWindow::FromConfig();

		// Double check that the window has not been widened by accident
		if (!(m_hitWindow <= HitWindow::NORMAL))
		{
			Log("HitWindow is automatically adjusted to NORMAL", Logger::Severity::Warning);
			m_hitWindow = HitWindow::NORMAL;
		}

		const BeatmapSettings& mapSettings = m_beatmap->GetMapSettings();

		// Move this somewhere else?
		// Set hi-speed for m-Mod
		// Uses the "mode" of BPMs in the chart, should use median?
		if(m_speedMod == SpeedMods::MMod)
		{
			Map<double, MapTime> bpmDurations;
			const Vector<TimingPoint*>& timingPoints = m_beatmap->GetLinearTimingPoints();
			MapTime lastMT = mapSettings.offset;
			MapTime largestMT = -1;
			double useBPM = -1;
			double lastBPM = -1;
			for (TimingPoint* tp : timingPoints)
			{
				double thisBPM = tp->GetBPM();
				if (!bpmDurations.count(lastBPM))
				{
					bpmDurations[lastBPM] = 0;
				}
				MapTime timeSinceLastTP = tp->time - lastMT;
				bpmDurations[lastBPM] += timeSinceLastTP;
				if (bpmDurations[lastBPM] > largestMT)
				{
					useBPM = lastBPM;
					largestMT = bpmDurations[lastBPM];
				}
				lastMT = tp->time;
				lastBPM = thisBPM;
			}
			bpmDurations[lastBPM] += m_endTime - lastMT;

			if (bpmDurations[lastBPM] > largestMT)
			{
				useBPM = lastBPM;
			}

			m_hispeed = m_modSpeed / useBPM; 
			CheckChallengeHispeed(useBPM);
		}
		else if (m_speedMod == SpeedMods::CMod)
		{
			double bpm = m_beatmap->GetLinearTimingPoints().front()->GetBPM();
			m_hispeed = m_modSpeed / bpm;
			CheckChallengeHispeed(bpm);
		}
		else if (m_speedMod == SpeedMods::XMod)
		{
			CheckChallengeHispeed(m_beatmap->GetLinearTimingPoints().front()->GetBPM());
		}

		// Load replays
		if (m_chartIndex)
			for (ScoreIndex* score : m_chartIndex->scores)
			{
				File replayFile;
				if (replayFile.OpenRead(score->replayPath)) {
					ScoreReplay& replay = m_scoreReplays.Add(ScoreReplay());
					replay.maxScore = score->score;
					FileReader replayReader(replayFile);
					replayReader.SerializeObject(replay.replay);

					if (replayReader.Tell() + 16 <= replayReader.GetSize())
					{
						replayReader.Serialize(&(replay.hitWindow.perfect), 4);
						replayReader.Serialize(&(replay.hitWindow.good), 4);
						replayReader.Serialize(&(replay.hitWindow.hold), 4);
						replayReader.Serialize(&(replay.hitWindow.miss), 4);
						replayReader.Serialize(&replay.hitWindow.slam, 4);
					}
				}
			}

        m_delayedHitEffects = g_gameConfig.GetBool(GameConfigKeys::DelayedHitEffects);

		// Initialize input/scoring
		if(!InitGameplay())
			return false;

		// Load beatmap audio
		if(!m_audioPlayback.Init(m_playback, m_chartRootPath, g_gameConfig.GetBool(GameConfigKeys::PrerenderEffects)))
			return false;

		m_songOffset = 0;

		// Compute chart offset
		if (g_gameConfig.GetBool(GameConfigKeys::AutoComputeSongOffset) && m_scoreReplays.empty()) {
			if (OffsetComputer(m_audioPlayback).Compute(m_songOffset) && m_chartIndex)
			{
				Logf("Setting the chart offset of '%s' to %d (previously %d)", Logger::Severity::Info, m_chartIndex->title, m_songOffset, m_chartIndex->custom_offset);
				m_chartIndex->custom_offset = m_songOffset;
			}
		}

		if (m_chartIndex)
		{
			m_songOffset = m_chartIndex->custom_offset;
		}

		// Get fps limit
		m_fpsTarget = g_gameConfig.GetInt(GameConfigKeys::FPSTarget);

		m_lastMapTime = GetPlayStartTime();

		// Load audio offset
		m_globalOffset = g_gameConfig.GetInt(GameConfigKeys::GlobalOffset);
		m_tempOffset = 0;

		InitPlaybacks(0);

		m_saveSpeed = g_gameConfig.GetBool(GameConfigKeys::AutoSaveSpeed);

		/// TODO: Check if debugmute is enabled
		g_audio->SetGlobalVolume(g_gameConfig.GetFloat(GameConfigKeys::MasterVolume));

		if(!InitSFX())
			return false;

		// Intialize track graphics
		m_track = new Track();
		loader.AddLoadable(*m_track, "Track");

		if(!InitHUD())
			return false;

		if(!loader.Load())
			return false;

		// Load particle material
		m_particleSystem = ParticleSystemRes::Create(g_gl);

		if (m_isPracticeSetup)
			m_practiceSetupDialog = std::make_unique<PracticeModeSettingsDialog>(*this, m_lastMapTime, m_tempOffset, m_playOptions, m_practiceSetupRange);
		
		return true;
	}

	virtual bool AsyncFinalize() override
	{
		if (!loader.Finalize())
			return false;

		// Always hide mouse during gameplay no matter what input mode.
		g_gameWindow->SetCursorVisible(false);

		//Lua
		m_lua = g_application->LoadScript("gameplay");
		if (!m_lua)
			return false;

		if (g_gameConfig.GetBool(GameConfigKeys::EnableHiddenSudden)) {
			m_track->suddenCutoff = g_gameConfig.GetFloat(GameConfigKeys::SuddenCutoff);
			m_track->hiddenCutoff = g_gameConfig.GetFloat(GameConfigKeys::HiddenCutoff);
		}
		else {
			m_track->suddenCutoff = 1.0f;
			m_track->hiddenCutoff = 0.0f;
		}
		if (IsChallenge())
		{
			float hiddenMin = m_challengeManager->GetCurrentOptions().hidden_min.Get(0.0);
			if (m_track->hiddenCutoff < hiddenMin)
				m_track->hiddenCutoff = hiddenMin;

			// Sudden is reversed since it starts at 1.0
			float suddenMin = 1.0 - m_challengeManager->GetCurrentOptions().sudden_min.Get(0.0);
			if (m_track->suddenCutoff > suddenMin)
				m_track->suddenCutoff = suddenMin;
		}
		m_track->suddenFadewindow = g_gameConfig.GetFloat(GameConfigKeys::SuddenFade);
		m_track->hiddenFadewindow = g_gameConfig.GetFloat(GameConfigKeys::HiddenFade);

		m_track->distantButtonScale = g_gameConfig.GetFloat(GameConfigKeys::DistantButtonScale);
		m_showCover = g_gameConfig.GetBool(GameConfigKeys::ShowCover);

		if (m_delayedHitEffects)
		{
			m_scoring.OnHoldEnter.Add(m_track, &Track::OnHoldEnter);
			m_scoring.OnHoldLeave.Add(m_track, &Track::OnButtonReleased);
		}

		#ifdef EMBEDDED
		basicParticleTexture = Ref<TextureRes>();
		particleMaterial = Ref<MaterialRes>();
		#else
		// Load particle textures
		basicParticleTexture = g_application->LoadTexture("particle_flare.png");
		particleMaterial = g_application->LoadMaterial("particle");
		if (particleMaterial)
		{
			particleMaterial->blendMode = MaterialBlendMode::Additive;
			particleMaterial->opaque = false;
		}
		#endif

		const BeatmapSettings& mapSettings = m_beatmap->GetMapSettings();
		int64 startTime = Shared::Time::Now().Data();

		// TODO: Set more accurate endTime
		int64 endTime = startTime + (m_playOptions.range.Length(m_endTime) + GetAudioLeadIn()) / 1000;
		g_application->DiscordPresenceSong(mapSettings, startTime, endTime);

		String jacketPath = m_chartRootPath + "/" + mapSettings.jacketPath;
		// Set gameplay table
		SetInitialGameplayLua(m_lua);

		// For multiplayer we also bind the TCP in
		if (m_multiplayer != nullptr)
			m_multiplayer->GetTCP().PushFunctions(m_lua);

		// Background 
		/// TODO: Load this async
		if (!g_gameConfig.GetBool(GameConfigKeys::DisableBackgrounds))
		{
			m_background = CreateBackground(this);
			m_foreground = CreateBackground(this, true);
		}

		// Do this here so we don't get input events while still loading
		m_scoring.SetOptions(GetPlaybackOptions());
		m_scoring.SetPlayback(m_playback);
		m_scoring.SetEndTime(m_endTime);
		m_scoring.SetInput(&g_input);
		m_scoring.Reset(m_playOptions.range);

		m_scoring.SetHitWindow(GetHitWindow());

		g_input.OnButtonPressed.Add(this, &Game_Impl::m_OnButtonPressed);

		m_track->hitEffectAutoplay = m_scoring.autoplayInfo.IsAutoplayButtons();

		if (GetPlaybackOptions().random)
		{
			//Randomize
			std::array<int,4> swaps = { 0,1,2,3 };
			
			std::shuffle(swaps.begin(), swaps.end(), std::default_random_engine((int)(1000 * g_application->GetAppTime())));

			bool unchanged = true;
			for (int i = 0; i < 4; i++)
			{
				if (swaps[i] != i)
				{
					unchanged = false;
					break;
				}
			}
			bool flipFx = false;

			if (unchanged)
			{
				flipFx = true;
			}
			else
			{
				std::srand((int)(1000 * g_application->GetAppTime()));
				flipFx = (std::rand() % 2) == 1;
			}

			const Vector<ObjectState*> chartObjects = m_playback.GetBeatmap().GetLinearObjects();
			for (ObjectState* currentobj : chartObjects)
			{
				if (currentobj->type == ObjectType::Single || currentobj->type == ObjectType::Hold)
				{
					ButtonObjectState* bos = (ButtonObjectState*)currentobj;
					if (bos->index < 4)
					{
						bos->index = swaps[bos->index];
					}
					else if (flipFx)
					{
						bos->index = (bos->index - 3) % 2;
						bos->index += 4;
					}
				}
			}

		}

		if (GetPlaybackOptions().mirror)
		{
			int buttonSwaps[] = { 3,2,1,0,5,4 };

			const Vector<ObjectState*> chartObjects = m_playback.GetBeatmap().GetLinearObjects();
			for (ObjectState* currentobj : chartObjects)
			{
				if (currentobj->type == ObjectType::Single || currentobj->type == ObjectType::Hold)
				{
					ButtonObjectState* bos = (ButtonObjectState*)currentobj;
					bos->index = buttonSwaps[bos->index];
				}
				else if (currentobj->type == ObjectType::Laser)
				{
					LaserObjectState* los = (LaserObjectState*)currentobj;
					los->index = (los->index + 1) % 2;
					for (size_t i = 0; i < 2; i++)
					{
						los->points[i] = fabsf(los->points[i] - 1.0f);
					}
				}
			}
		}

		if (m_practiceSetupDialog)
		{
			m_InitPracticeSetupDialog();
			m_practiceSetupDialog->Open();
		}

		return true;
	}

	void CheckChallengeHispeed(double bpm)
	{
		if (!IsChallenge())
			return;
		// Get the current modspeed for the hispeed
		m_modSpeed = m_hispeed * bpm;
		// TODO should we optimize these accesses?
		uint32 min = m_challengeManager->GetCurrentOptions().min_modspeed.Get(0);
		uint32 max = m_challengeManager->GetCurrentOptions().max_modspeed.Get(INT_MAX);
		if (m_modSpeed < min)
			m_modSpeed = min;
		else if (m_modSpeed > max)
			m_modSpeed = max;
		else
			return; // No change needed
		m_hispeed = m_modSpeed / bpm;
	}

	bool Init() override
	{
		return true;
	}

	// Restart map
	void Restart()
	{
		m_paused = false;
		m_triggerPause = false;
		m_playOnDialogClose = true;
		JumpTo(GetPlayStartTime());
	}

	void JumpTo(MapTime newTime)
	{
		Logf("Game::JumpTo(%d)", Logger::Severity::Debug, newTime);

		m_triggerEnd = false;
		m_triggerPause = m_paused;

		if (m_endTime < newTime)
		{
			newTime = m_endTime;
		}
		if (m_playOptions.range.HasEnd() && m_playOptions.range.end <= newTime)
		{
			newTime = m_playOptions.range.end - 1;
		}

		const MapTime mapTimeDiff = m_lastMapTime - newTime;

		m_camera = Camera();
		
		// Audio leadin
		m_audioPlayback.SetEffectEnabled(0, false);
		m_audioPlayback.SetEffectEnabled(1, false);


		m_lastMapTime = newTime;
		InitPlaybacks(newTime);

		// Keep the exit trigger time
		if (m_exitTriggerTimeSet)
		{
			m_exitTriggerTime -= mapTimeDiff;
		}

		m_paused = false;
		m_started = false;
		m_ended = false;
		m_hideLane = false;
		m_transitioning = false;
		m_scoring.Reset(m_playOptions.range);
		m_scoring.SetInput(&g_input);
		m_camera.pLaneZoom = m_playback.GetZoom(0);
		m_camera.pLanePitch = m_playback.GetZoom(1);
		m_camera.pLaneOffset = m_playback.GetZoom(2);
		m_camera.pLaneTilt = m_playback.GetZoom(3);
		m_track->centerSplit = m_playback.GetZoom(4);

		for(uint32 i = 0; i < 2; i++)
		{
			if(m_laserFollowEmitters[i])
			{
				m_laserFollowEmitters[i].reset();
			}
		}
		for(uint32 i = 0; i < 6; i++)
		{
			if(m_holdEmitters[i])
			{
				m_holdEmitters[i].reset();
			}
		}

		for (auto& replay : m_scoreReplays)
		{
			replay.currentScore = 0;
			replay.currentMaxScore = 0;
			replay.nextHitStat = 0;
		}

		m_particleSystem->Reset();
		m_audioPlayback.SetVolume(1.0f);

		//unhide notes
		m_hiddenObjects.clear();
	}

	virtual void Tick(float deltaTime) override
	{
		if (m_ended && IsSuspended()) return;

		// Lock mouse to screen when playing
		if(g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice) == InputDevice::Mouse)
		{
			if(!m_paused && g_gameWindow->IsActive())
			{
				if(!m_lockMouse)
					m_lockMouse = g_input.LockMouse();
				g_gameWindow->SetCursorVisible(false);
			}
			else
			{
				if(m_lockMouse)
					m_lockMouse.reset();
				g_gameWindow->SetCursorVisible(true);
			}
		}

		if(!m_paused)
			TickGameplay(deltaTime);

		if (m_outroCompleted && !m_transitioning)
			BeginAfterGameTransition();

		// Handles held restart / exit button
		if (m_restartTriggerTimeSet)
		{
			if (m_restartTriggerTime <= m_lastMapTime)
			{
				m_restartTriggerTimeSet = false;
				Restart();
				return;
			}
		}

		if (m_exitTriggerTimeSet)
		{
			if (g_input.GetButton(Input::Button::Back))
			{
				if (m_exitTriggerTime <= m_lastMapTime)
				{
					m_exitTriggerTimeSet = false;
					TriggerManualExit();
					return;
				}
			}
			else
			{
				m_exitTriggerTimeSet = false;
			}
		}

		// Update hispeed or hidden range
		if (g_input.GetButton(Input::Button::BT_S))
		{
			for (int i = 0; i < 2; i++)
			{
				m_hispeedAdvance += g_input.GetInputLaserDir(i) / 3.0f;
				int hispeedSteps = static_cast<int>(truncf(m_hispeedAdvance * 10.0f));
				m_hispeedAdvance -= 0.1f * hispeedSteps;
				if (hispeedSteps != 0)
				{
					m_hispeed = static_cast<float>(static_cast<int>(m_hispeed * 10.0f) + hispeedSteps) / 10.0f;
					m_hispeed = Math::Clamp(m_hispeed, 0.1f, 16.f);

					if (m_saveSpeed)
					{
						g_gameConfig.Set(GameConfigKeys::ModSpeed, m_hispeed * (float)m_currentTiming->GetBPM());
					}
					m_modSpeed = m_hispeed * (float)m_currentTiming->GetBPM();
					// Have to check in here so we can update m_playback
					m_playback.cModSpeed = m_modSpeed;
					CheckChallengeHispeed(m_currentTiming->GetBPM());
				}
			}
		}

		if (m_isPracticeSetup && m_isPracticeSetupNavEnabled && !(m_practiceSetupDialog && m_practiceSetupDialog->IsActive()))
		{
			float scroll = g_input.GetInputLaserDir(0) + g_input.GetInputLaserDir(1) * 5.0f;
			if (scroll != 0)
			{
				if (!m_paused)
				{
					m_audioPlayback.Pause();
					m_paused = true;
				}

				m_lastMapTime = Math::Clamp(static_cast<MapTime>(m_lastMapTime + scroll * 500), 0, m_endTime);
				JumpTo(m_lastMapTime);
			}
		}

		if (m_practiceSetupDialog)
			m_practiceSetupDialog->Tick(deltaTime);
	}

	virtual void Render(float deltaTime) override
	{
		if (m_ended && IsSuspended()) return;

		// 8 beats (2 measures) in view at 1x hi-speed
		if (m_speedMod == SpeedMods::CMod)
		{
			m_track->SetViewRange(1.0 / m_playback.cModSpeed);
            m_track->scrollSpeed = m_playback.cModSpeed;
		}
		else
		{
			m_track->SetViewRange(8.0f / m_hispeed);
            m_track->scrollSpeed = m_hispeed * m_playback.GetCurrentTimingPoint().GetBPM();
		}

		// Get render state from the camera
		// Get roll when there's no laser slam roll and roll ignore being applied
		// This could be simplified but is necessary to have SDVX II-like roll keep and laser slams
		float rollL = m_camera.GetRollIgnoreTimer(0) <= 0 ? m_scoring.GetLaserRollOutput(0) : m_camera.GetSlamAmount(0);
		float rollR = m_camera.GetRollIgnoreTimer(1) <= 0 ? m_scoring.GetLaserRollOutput(1) : m_camera.GetSlamAmount(1);
		bool slowTilt = (rollL == -1 && rollR == 1) || (rollL == 0 && rollR == 0);
		rollL = m_camera.GetRollIgnoreTimer(0) <= 0 ? m_scoring.GetLaserRollOutput(0) : 0;
		rollR = m_camera.GetRollIgnoreTimer(1) <= 0 ? m_scoring.GetLaserRollOutput(1) : 0;
		m_camera.SetTargetRoll(rollL + rollR);
		m_camera.SetSlowTilt(slowTilt);

		// Set track zoom

		m_camera.pLaneZoom = m_playback.GetZoom(0);
		m_camera.pLanePitch = m_playback.GetZoom(1);
		m_camera.pLaneOffset = m_playback.GetZoom(2);
		m_camera.pLaneTilt = m_playback.GetZoom(3);
		m_track->centerSplit = m_playback.GetZoom(4);
		m_camera.SetManualTilt(m_manualTiltEnabled);
		m_camera.SetManualTiltInstant(m_playback.CheckIfManualTiltInstant());
		m_camera.track = m_track;
		m_camera.Tick(deltaTime,m_playback);
		m_track->Tick(m_playback, deltaTime);
		RenderState rs = m_camera.CreateRenderState(true);

		// Draw BG first
		if(m_background)
			m_background->Render(deltaTime);

		// Main render queue
		RenderQueue renderQueue(g_gl, rs);

		// Get objects in range
		MapTime msViewRange = m_playback.ViewDistanceToDuration(m_track->GetViewRange());
		if (m_speedMod == SpeedMods::CMod)
		{
			msViewRange = 480000.0 / m_playback.cModSpeed;
		}
		m_currentObjectSet = m_playback.GetObjectsInRange(msViewRange);
		// Sort objects to draw
		// fx holds -> bt holds -> fx chips -> bt chips
		m_currentObjectSet.Sort([](const TObjectState<void>* a, const TObjectState<void>* b)
		{
			auto ObjectRenderPriorty = [](const TObjectState<void>* a)
			{
				if (a->type == ObjectType::Single)
					return (((ButtonObjectState*)a)->index < 4) ? 1 : 2;
				if (a->type == ObjectType::Hold)
					return (((ButtonObjectState*)a)->index < 4) ? 3 : 4;
				return 0;
			};
			uint32 renderPriorityA = ObjectRenderPriorty(a);
			uint32 renderPriorityB = ObjectRenderPriorty(b);
			return renderPriorityA > renderPriorityB;
		});

		//TODO: Set as bool on the button object during parsing(?)
		std::unordered_set<MapTime> chipFXTimes[2];
		for (const auto& obj : m_currentObjectSet) {
			if (obj->type == ObjectType::Single) {
				auto b = (ButtonObjectState*)obj;
				if (b->index > 3) {
					chipFXTimes[b->index - 4].insert(b->time);
				}
			}
		}

		RenderQueue fxHoldObjectsRq(g_gl, rs);
		RenderQueue hitObjectsTrackCoverRq(g_gl, rs);

		/// TODO: Performance impact analysis.
		m_track->DrawLaserBase(renderQueue, m_playback, m_currentObjectSet);

		// Draw the base track + time division ticks
		m_track->DrawBase(renderQueue);

		for(auto& object : m_currentObjectSet)
		{
			if(m_hiddenObjects.find(object) == m_hiddenObjects.end())
			{
				MultiObjectState* mobj = (MultiObjectState*)object;
				if (object->type == ObjectType::Hold && (mobj->button.index == 4 || mobj->button.index == 5))
					m_track->DrawObjectState(fxHoldObjectsRq, m_playback, object, m_scoring.IsObjectHeld(object), chipFXTimes);
				else
					m_track->DrawObjectState(hitObjectsTrackCoverRq, m_playback, object, m_scoring.IsObjectHeld(object), chipFXTimes);
			}
		}
		if(m_showCover)
			m_track->DrawTrackCover(hitObjectsTrackCoverRq);

		// Use new camera for scoring overlay
		//	this is because otherwise some of the scoring elements would get clipped to
		//	the track's near and far planes
		rs = m_camera.CreateRenderState(false);
		RenderQueue hitEffectsRq(g_gl, rs);
		RenderQueue scoringRq(g_gl, rs);

		// Copy over laser position and extend info
		for(uint32 i = 0; i < 2; i++)
		{
			if(m_scoring.IsLaserHeld(i))
			{
				m_track->laserPositions[i] = m_scoring.laserTargetPositions[i];
				m_track->lasersAreExtend[i] = m_scoring.lasersAreExtend[i];
			}
			else
			{
				m_track->laserPositions[i] = m_scoring.laserPositions[i];
				m_track->lasersAreExtend[i] = m_scoring.lasersAreExtend[i];
			}
			m_track->laserPositions[i] = m_scoring.laserPositions[i];
			m_track->laserPointerOpacity[i] = (1.0f - Math::Clamp<float>(m_scoring.timeSinceLaserUsed[i] / 0.5f - 1.0f, 0, 1));
		}
		m_track->DrawHitEffects(hitEffectsRq);
		m_track->DrawOverlays(scoringRq);
		// Render queues
		renderQueue.Process();
		fxHoldObjectsRq.Process();
		hitEffectsRq.Process();
		hitObjectsTrackCoverRq.Process();
		scoringRq.Process();
		glFlush();

		// Set laser follow particle visiblity
		if (particleMaterial &&	particleMaterial)
		{
			for (uint32 i = 0; i < 2; i++)
			{
				if (m_scoring.IsLaserHeld(i))
				{
					if (!m_laserFollowEmitters[i])
						m_laserFollowEmitters[i] = CreateTrailEmitter(m_track->laserColors[i]);

					// Set particle position to follow laser
					float followPos = m_scoring.laserTargetPositions[i];
					if (m_scoring.lasersAreExtend[i])
						followPos = followPos * 2.0f - 0.5f;

					m_laserFollowEmitters[i]->position = m_track->TransformPoint(Vector3(m_track->trackWidth * followPos - m_track->trackWidth * 0.5f, 0.f, 0.f));
				}
				else
				{
					if (m_laserFollowEmitters[i])
					{
						m_laserFollowEmitters[i].reset();
					}
				}
			}

			// Set hold button particle visibility
			for (uint32 i = 0; i < 6; i++)
			{
				if (m_scoring.IsObjectHeld(i))
				{
					if (!m_holdEmitters[i])
					{
						Color hitColor = (i < 4) ? Color::White : Color::FromHSV(20, 0.7f, 1.0f);
						float hitWidth = (i < 4) ? m_track->buttonWidth : m_track->fxbuttonWidth;
						m_holdEmitters[i] = CreateHoldEmitter(hitColor, hitWidth);
					}
					m_holdEmitters[i]->position = m_track->TransformPoint(Vector3(m_track->GetButtonPlacement(i), 0.f, 0.f));
				}
				else
				{
					if (m_holdEmitters[i])
					{
						m_holdEmitters[i].reset();
					}
				}

			}
		}

		// IF YOU INCLUDE nanovg.h YOU CAN DO
		/* THIS WHICH IS FROM Application.cpp, lForceRender
		nvgEndFrame(g_guiState.vg);
		g_application->GetRenderQueueBase()->Process();
		nvgBeginFrame(g_guiState.vg, g_resolution.x, g_resolution.y, 1);
		*/
		// BUT OTHERWISE HERE DOES THE SAME THING BUT WITH LUA
#define NVG_FLUSH() do { \
		lua_getglobal(m_lua, "gfx"); \
		lua_getfield(m_lua, -1, "ForceRender"); \
		if (lua_pcall(m_lua, 0, 0, 0) != 0) { \
			Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(m_lua, -1)); \
			g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0); \
			assert(false); \
		} \
		lua_pop(m_lua, 1); \
		} while (0)

		// Render Critical Line Base
		lua_getglobal(m_lua, "render_crit_base");
		lua_pushnumber(m_lua, deltaTime);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
			assert(false);
		}
		// flush NVG
		NVG_FLUSH();

		// Render particle effects last
		if (particleMaterial && basicParticleTexture) 
		{
			RenderParticles(rs, deltaTime);
			glFlush();
		}

		// Render Critical Line Overlay
		lua_getglobal(m_lua, "render_crit_overlay");
		lua_pushnumber(m_lua, deltaTime);
		// only flush if the overlay exists. overlay isn't required, only one crit function is required.
		if (lua_pcall(m_lua, 1, 0, 0) == 0)
			NVG_FLUSH();

		// Render foreground
		if(m_foreground)
			m_foreground->Render(deltaTime);

		// Render Lua HUD
		lua_getglobal(m_lua, "render");
		lua_pushnumber(m_lua, deltaTime);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
			assert(false);
		}
		if (!m_introCompleted)
		{
			// Render Lua Intro
			lua_getglobal(m_lua, "render_intro");
			if (lua_isfunction(m_lua, -1))
			{
				lua_pushnumber(m_lua, deltaTime);
				if (lua_pcall(m_lua, 1, 1, 0) != 0)
				{
					Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
					g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
				}
				m_introCompleted = lua_toboolean(m_lua, lua_gettop(m_lua));
			}
			else
			{
				m_introCompleted = true;
			}
			
			lua_settop(m_lua, 0);
		}
		if (m_ended)
		{
			// Render Lua Outro
			lua_getglobal(m_lua, "render_outro");
			if (lua_isfunction(m_lua, -1))
			{
				lua_pushnumber(m_lua, deltaTime);
				lua_pushnumber(m_lua, static_cast<lua_Number>(m_getClearState()));
				if (lua_pcall(m_lua, 2, 2, 0) != 0)
				{
					Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
					g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
				}
				if (lua_isnumber(m_lua, lua_gettop(m_lua)))
				{
					float speed = Math::Clamp((float)lua_tonumber(m_lua, lua_gettop(m_lua)), 0.0f, 1.0f);
					m_audioPlayback.SetPlaybackSpeed(speed);
					m_audioPlayback.SetVolume(Math::Clamp(speed * 10.0f, 0.0f, 1.0f));
				}
				lua_pop(m_lua, 1);
				m_outroCompleted = lua_toboolean(m_lua, lua_gettop(m_lua));
			}
			else
			{
				m_outroCompleted = true;
			}
			lua_settop(m_lua, 0);
		}

		// Render debug hud if enabled
		if(m_renderDebugHUD)
		{
			RenderDebugHUD(deltaTime);
		}

		if (m_practiceSetupDialog)
			m_practiceSetupDialog->Render(deltaTime);
	}

	virtual void OnSuspend() override
	{
	}

	virtual void OnRestore() override
	{
		if (m_ended && m_isPracticeMode)
		{
			RevertToPracticeSetup();
		}
	}

	// Initialize HUD elements/layout
	bool InitHUD()
	{
		String skin = g_gameConfig.GetString(GameConfigKeys::Skin);
		return true;
	}

	// Wait before start of map
	MapTime GetPlayStartTime()
	{
		// Select the correct first object to set the intial playback position
		// if it starts before a certain time frame, the song starts at a negative time (lead-in)
		ObjectState* const* firstObj = &m_beatmap->GetLinearObjects().front();
		for (; firstObj != &m_beatmap->GetLinearObjects().back(); ++firstObj)
		{
			if ((*firstObj)->type == ObjectType::Event) continue;
			if ((*firstObj)->time < m_playOptions.range.begin) continue;

			break;
		}

		const MapTime beginTime = m_playOptions.range.begin;
		const MapTime firstObjectTime = (*firstObj)->time;

		return std::min(beginTime, firstObjectTime - GetAudioLeadIn());
	}

	inline int32 GetAudioOffset() const
	{
		return m_globalOffset + m_songOffset + m_tempOffset;
	}

	inline MapTime GetAudioLeadIn() const
	{
		const GameConfigKeys leadInTimeOpt = m_isPracticeMode ? GameConfigKeys::PracticeLeadInTime : GameConfigKeys::LeadInTime;
		return static_cast<MapTime>(g_gameConfig.GetInt(leadInTimeOpt) * m_audioPlayback.GetPlaybackSpeed());
	}

	void InitPlaybacks(int beginTime)
	{
		m_audioPlayback.SetPosition(m_lastMapTime + GetAudioOffset());

		m_playback.audioOffset = GetAudioOffset();
		m_playback.Reset(m_lastMapTime, std::max(beginTime, m_playOptions.range.begin));

		ApplyPlaybackSpeed();
		m_LuaUpdateProgress();
	}

	void m_LuaUpdateProgress()
	{
		if (!m_lua) return;

		lua_getglobal(m_lua, "gameplay");
		m_LuaUpdateProgress(m_lua);
		lua_setglobal(m_lua, "gameplay");
	}

	void m_LuaUpdateProgress(lua_State* L)
	{
		MapTime progress = m_lastMapTime - m_playOptions.range.begin;
		MapTime duration = m_playOptions.range.Length(m_endTime);

		// Fallback to default
		if (duration == 0)
		{
			progress = m_lastMapTime;
			duration = m_endTime;
		}

		lua_pushstring(L, "progress");
		lua_pushnumber(L, Math::Clamp((float)progress / duration, 0.f, 1.f));
		lua_settable(L, -3);
	}

	// Loads sound effects
	bool InitSFX()
	{
		CheckedLoad(m_slamSample = g_application->LoadSample("laser_slam"));
		CheckedLoad(m_clickSamples[0] = g_application->LoadSample("click-01"));
		CheckedLoad(m_clickSamples[1] = g_application->LoadSample("click-02"));

		Vector<String> default_sfx = {
			"clap",
			"clap_impact",
			"clap_punchy",
			"snare",
			"snare_lo",
		};

		auto samples = m_beatmap->GetSamplePaths();
		m_fxSamples = new Sample[samples.size()];
		for (size_t i = 0; i < samples.size(); i++)
		{
			if (default_sfx.Contains(samples[i]))
			{
				m_fxSamples[i] = g_application->LoadSample(samples[i]);
			}
			else
			{
				m_fxSamples[i] = g_application->LoadSample(m_chartRootPath + "/" + samples[i], true);
			}
			if (!m_fxSamples[i])
			{
				Logf("Failed to load FX chip sample: \"%s\"", Logger::Severity::Warning, samples[i]);
			}
		}

		return true;
	}

	bool InitGameplay()
	{
		// Playback and timing
		m_playback = BeatmapPlayback(*m_beatmap);
		m_playback.OnEventChanged.Add(this, &Game_Impl::OnEventChanged);
		m_playback.OnLaneToggleChanged.Add(this, &Game_Impl::OnLaneToggleChanged);
		m_playback.OnFXBegin.Add(this, &Game_Impl::OnFXBegin);
		m_playback.OnFXEnd.Add(this, &Game_Impl::OnFXEnd);
		m_playback.OnLaserAlertEntered.Add(this, &Game_Impl::OnLaserAlertEntered);
		m_playback.Reset();

		// Set camera start position
		m_camera.pLaneZoom = m_playback.GetZoom(0);
		m_camera.pLanePitch = m_playback.GetZoom(1);
		m_camera.pLaneOffset = m_playback.GetZoom(2);
		m_camera.pLaneTilt = m_playback.GetZoom(3);
		
		// Enable laser slams and roll ignore behaviour
		m_camera.SetFancyHighwayTilt(g_gameConfig.GetBool(GameConfigKeys::EnableFancyHighwayRoll));

		SDL_DisplayMode current;
		int displayIndex = g_gameWindow->GetDisplayIndex();
		int error = SDL_GetCurrentDisplayMode(displayIndex, &current);
		if (error)
			Logf("Could not get display mode info for display %d: %s", Logger::Severity::Warning, displayIndex, SDL_GetError());
		else
			m_camera.SetSlamShakeGuardDuration(current.refresh_rate);

		// If c-mod is used
		if (m_speedMod == SpeedMods::CMod)
		{
			m_playback.OnTimingPointChanged.Add(this, &Game_Impl::OnTimingPointChanged);
		}
		else if (IsChallenge())
		{
			m_playback.OnTimingPointChanged.Add(this, &Game_Impl::OnTimingPointChangedChallenge);
		}
		m_playback.cMod = m_speedMod == SpeedMods::CMod;
		CheckChallengeHispeed(m_playback.GetCurrentTimingPoint().GetBPM());
		m_playback.cModSpeed = m_hispeed * m_playback.GetCurrentTimingPoint().GetBPM();

		// Register input bindings
		m_scoring.OnButtonMiss.Add(this, &Game_Impl::OnButtonMiss);
		m_scoring.OnLaserSlamHit.Add(this, &Game_Impl::OnLaserSlamHit);
		m_scoring.OnButtonHit.Add(this, &Game_Impl::OnButtonHit);
		m_scoring.OnComboChanged.Add(this, &Game_Impl::OnComboChanged);
		m_scoring.OnObjectHold.Add(this, &Game_Impl::OnObjectHold);
		m_scoring.OnObjectReleased.Add(this, &Game_Impl::OnObjectReleased);
		m_scoring.OnScoreChanged.Add(this, &Game_Impl::OnScoreChanged);
		m_scoring.OnGaugeChanged.Add(this, &Game_Impl::OnGaugeChanged);

		m_scoring.OnLaserSlam.Add(this, &Game_Impl::OnLaserSlam);
		m_scoring.OnLaserExit.Add(this, &Game_Impl::OnLaserExit);

		m_playback.hittableObjectEnter = m_scoring.hitWindow.miss + g_gameConfig.GetInt(GameConfigKeys::InputOffset);
		m_playback.hittableObjectLeave = m_scoring.hitWindow.good;

		if(g_application->GetAppCommandLine().Contains("-autobuttons"))
		{
			m_scoring.autoplayInfo.autoplayButtons = true;
		}

		return true;
	}

	void ApplyPlaybackSpeed()
	{
		const float playbackSpeed = Math::Clamp(m_playOptions.playbackSpeed, 0.1f, 10.0f);
		m_audioPlayback.SetPlaybackSpeed(playbackSpeed);
	}

	// Processes input and Updates scoring, also handles audio timing management
	void TickGameplay(float deltaTime)
	{
		if(!m_started && m_introCompleted && (m_multiplayer == nullptr || !m_multiplayer->IsSyncing()))
		{
			if (m_multiplayer != nullptr && !m_multiplayer->IsSynced()) {
				// We are at the start of the song, so trigger a sync
				m_multiplayer->StartSync();
				return;
			}

			// Start playback of audio in first gameplay tick
			if (m_triggerPause)
			{
				m_triggerPause = false;
				m_audioPlayback.Pause();
			}
			else
			{
				m_audioPlayback.Play();
			}

			m_paused = m_audioPlayback.IsPaused();

			if(g_application->GetAppCommandLine().Contains("-autoskip"))
			{
				SkipIntro();
			}
		}

		const BeatmapSettings& beatmapSettings = m_beatmap->GetMapSettings();

		// Update beatmap playback
		const MapTime playbackPositionMs = m_audioPlayback.GetPosition() - GetAudioOffset();
		m_playback.Update(playbackPositionMs);

		const MapTime delta = playbackPositionMs - m_lastMapTime;
		int32 beatStart = 0;
		uint32 numBeats = m_playback.CountBeats(m_lastMapTime, delta, beatStart, 1);
		if(numBeats > 0)
		{
			// Click Track
			//uint32 beat = beatStart % m_playback.GetCurrentTimingPoint().measure;
			//if(beat == 0)
			//{
			//	m_clickSamples[0]->Play();
			//}
			//else
			//{
			//	m_clickSamples[1]->Play();
			//}
		}

		/// #Scoring
		// Update music filter states
		m_audioPlayback.SetLaserFilterInput(m_scoring.GetLaserOutput(), m_scoring.IsLaserHeld(0, false) || m_scoring.IsLaserHeld(1, false));
		m_audioPlayback.Tick(deltaTime);

		m_audioPlayback.SetFXTrackEnabled(m_scoring.GetLaserActive() || m_scoring.GetFXActive());

		// Stop playing if last gauge has reached its failstate
		if (m_scoring.IsFailOut())
		{
			// In multiplayer we don't stop, but we send the final score
			if (m_multiplayer == nullptr) {
				FailCurrentRun();
			} else if (!m_multiplayer->HasFailed()) {
				m_multiplayer->Fail();

				//TODO(gauge refactor): ?
				//m_playOptions.flags = m_playOptions.flags & ~GameFlags::Hard;
				//m_scoring.SetFlags(m_playOptions.flags);
			}
		}


		// Update scoring
		if (!m_ended)
		{
			m_scoring.Tick(deltaTime);
		}

		// Get the current timing point
		m_currentTiming = &m_playback.GetCurrentTimingPoint();

		// Update song info display
		ObjectState *const* lastObj = &m_beatmap->GetLinearObjects().back();

		if (m_multiplayer != nullptr)
			m_multiplayer->PerformScoreTick(m_scoring, m_lastMapTime);

		if (delta >= 0)
		{
			m_lastMapTime = playbackPositionMs;
		}

		SetGameplayLua(m_lua);
		
		if(m_triggerEnd || m_audioPlayback.HasEnded())
		{
			EndCurrentRun();
		}
		else if (m_playOptions.range.begin < m_lastMapTime && !m_playOptions.range.Includes(m_lastMapTime))
		{
			EndCurrentRun();
		}
		else if (!m_scoring.autoplayInfo.autoplay && !m_isPracticeSetup && m_playOptions.failCondition && m_playOptions.failCondition->IsFailed(m_scoring))
		{
			Gauge* gauge = m_scoring.GetTopGauge();

			if (gauge) 
				gauge->SetValue(0.0f);
			FailCurrentRun();
		}
	}

	void BeginAfterGameTransition()
	{
		g_transition->OnLoadingComplete.RemoveAll(this);
		g_transition->OnLoadingComplete.Add(this, &Game_Impl::OnScoreScreenLoaded);

		if ((m_manualExit && g_gameConfig.GetBool(GameConfigKeys::SkipScore)
			&& m_multiplayer == nullptr && m_challengeManager == nullptr)
			|| (m_manualExit && m_demo))
		{
			g_application->RemoveTickable(this);
		}
		else if (m_demo)
		{
			// Transition to another random track
			Game* game = nullptr;
			while (!game) // ensure a working game
			{
				ChartIndex* chart = m_db->GetRandomChart();
				game = Game::Create(chart, std::move(m_playOptions));
			}
			game->GetScoring().autoplayInfo.autoplay = true;
			game->SetDemoMode(true);
			game->SetSongDB(m_db);

			// Transition to game
			g_transition->TransitionTo(game);
			m_transitioning = true;
		}
		else
		{
			// Transition to score screen
			if (IsMultiplayerGame())
			{
				g_transition->TransitionTo(ScoreScreen::Create(
					this, m_multiplayer->GetUserId(),
					m_multiplayer->GetFinalStats(), m_multiplayer));
			}
			else if (m_challengeManager != nullptr)
			{
				g_transition->TransitionTo(ScoreScreen::Create(
					this, m_challengeManager));
			}
			else
			{
				g_transition->TransitionTo(ScoreScreen::Create(this));
			}
			m_transitioning = true;
		}
	}

	// Called when the end is reached
	void EndCurrentRun()
	{
		m_triggerEnd = false;

		if (m_isPracticeSetup)
		{
			m_audioPlayback.Pause();
			m_paused = true;

			return;
		}

		if (!IsMultiplayerGame() && m_playOptions.loopOnSuccess)
		{
			m_OnEndRun(true);
			Restart();
		}
		else
		{
			FinishGame();
		}
	}

	// Called when the current play is failed for whatever reason
	void FailCurrentRun()
	{
		if (!IsMultiplayerGame() && m_playOptions.loopOnFail)
		{
			m_OnEndRun(false);
			Restart();
		}
		else
		{
			FinishGame();
		}
	}

	void m_OnEndRun(bool success)
	{
		++m_loopCount;
		if (success)
		{
			++m_loopSuccess;
			++m_loopStreak;
		}
		else
		{
			m_loopStreak = 0;
		}

		if (!m_isPracticeMode) return;

		if (m_playOptions.incSpeedOnSuccess && m_loopStreak >= static_cast<unsigned int>(m_playOptions.incStreak))
		{
			m_loopStreak = 0;

			int speedPercentage = Math::RoundToInt(100 * (m_playOptions.playbackSpeed + m_playOptions.incSpeedAmount));

			m_playOptions.playbackSpeed = speedPercentage >= 100 ? 1.0f : speedPercentage / 100.0f;
		}
		
		if (m_playOptions.decSpeedOnFail && !success)
		{
			const int minSpeedPercentage = Math::RoundToInt(100 * m_playOptions.minPlaybackSpeed);
			int speedPercentage = Math::RoundToInt(100 * (m_playOptions.playbackSpeed - m_playOptions.decSpeedAmount));
			if (speedPercentage < minSpeedPercentage) speedPercentage = minSpeedPercentage;
			if (speedPercentage <= 10) speedPercentage = 10;

			m_playOptions.playbackSpeed = speedPercentage / 100.0f;
		}

		if (m_playOptions.enableMaxRewind)
		{
			if (success)
			{
				m_playOptions.range.begin = m_practiceSetupRange.begin;
			}
			else
			{
				const int measureInd = m_beatmap->GetMeasureIndFromMapTime(m_lastMapTime) - m_playOptions.maxRewindMeasure;
				MapTime maxRewind = m_beatmap->GetMapTimeFromMeasureInd(measureInd);

				if (m_playOptions.range.begin < maxRewind) m_playOptions.range.begin = maxRewind;
			}
		}

		if (g_gameConfig.GetBool(GameConfigKeys::DisplayPracticeInfoInGame))
		{
			lua_getglobal(m_lua, "practice_end_run");
			if (lua_isfunction(m_lua, -1))
			{
				lua_pushinteger(m_lua, m_loopCount);
				lua_pushinteger(m_lua, m_loopSuccess);
				lua_pushboolean(m_lua, success);

				lua_newtable(m_lua);

				lua_pushstring(m_lua, "score");
				lua_pushinteger(m_lua, m_scoring.CalculateCurrentDisplayScore());
				lua_settable(m_lua, -3);

				lua_pushstring(m_lua, "perfects");
				lua_pushinteger(m_lua, m_scoring.GetPerfects());
				lua_settable(m_lua, -3);

				lua_pushstring(m_lua, "goods");
				lua_pushinteger(m_lua, m_scoring.GetGoods());
				lua_settable(m_lua, -3);

				lua_pushstring(m_lua, "misses");
				lua_pushinteger(m_lua, m_scoring.GetMisses());
				lua_settable(m_lua, -3);

				lua_pushstring(m_lua, "medianHitDelta");
				lua_pushinteger(m_lua, m_scoring.GetMedianHitDelta(false));
				lua_settable(m_lua, -3);

				lua_pushstring(m_lua, "meanHitDelta");
				lua_pushinteger(m_lua, m_scoring.GetMeanHitDelta(false));
				lua_settable(m_lua, -3);

				lua_pushstring(m_lua, "medianHitDeltaAbs");
				lua_pushinteger(m_lua, m_scoring.GetMedianHitDelta(true));
				lua_settable(m_lua, -3);

				lua_pushstring(m_lua, "meanHitDeltaAbs");
				lua_pushinteger(m_lua, m_scoring.GetMeanHitDelta(true));
				lua_settable(m_lua, -3);

				if (lua_pcall(m_lua, 4, 0, 0) != 0)
				{
					Logf("Lua error on calling laser_alert: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
				}
			}
			lua_settop(m_lua, 0);
		}
	}

	// Called when game is finished and the score screen should show up
	void FinishGame()
	{
		if(m_ended)
			return;

		// Send the final scores to the server
		if (m_multiplayer)
			m_multiplayer->SendFinalScore(this, m_getClearState());

		if (m_challengeManager)
			m_challengeManager->ReportScore(this, m_getClearState());

		m_scoring.FinishGame();
		m_ended = true;
	}
	void OnScoreScreenLoaded(IAsyncLoadableApplicationTickable* tickable)
	{
		//if demo and tickable failed, try another diff
		if (m_demo && !tickable)
		{
			Game* game = nullptr;
			while (!game) // ensure a working game
			{
				ChartIndex* diff = m_db->GetRandomChart();
				game = Game::Create(diff, m_playOptions.playbackOptions);
			}
			game->GetScoring().autoplayInfo.autoplay = true;
			game->SetDemoMode(true);
			game->SetSongDB(m_db);

			// Transition to game
			g_transition->TransitionTo(game);
			m_transitioning = true;
			return;
		}

		if (m_isPracticeMode && g_gameConfig.GetBool(GameConfigKeys::RevertToSetupAfterScoreScreen))
		{
			return;
		}
		
		// Remove self
		g_application->RemoveTickable(this);
	}

	void RenderParticles(const RenderState& rs, float deltaTime)
	{
		// Render particle effects
		m_particleSystem->Render(rs, deltaTime);
	}
	
	Ref<ParticleEmitter> CreateTrailEmitter(const Color& color)
	{
		Ref<ParticleEmitter> emitter = m_particleSystem->AddEmitter();
		emitter->material = particleMaterial;
		emitter->texture = basicParticleTexture;
		emitter->loops = 0;
		emitter->duration = 5.0f;
		emitter->SetSpawnRate(PPRandomRange<float>(250, 300));
		emitter->SetStartPosition(PPBox({ 0.5f, 0.0f, 0.0f }));
		emitter->SetStartSize(PPRandomRange<float>(0.25f, 0.4f));
		emitter->SetScaleOverTime(PPRange<float>(2.0f, 1.0f));
		emitter->SetFadeOverTime(PPRangeFadeIn<float>(1.0f, 0.0f, 0.4f));
		emitter->SetLifetime(PPRandomRange<float>(0.17f, 0.2f));
		emitter->SetStartDrag(PPConstant<float>(0.0f));
		emitter->SetStartVelocity(PPConstant<Vector3>({ 0, -4.0f, 2.0f }));
		emitter->SetSpawnVelocityScale(PPRandomRange<float>(0.9f, 2));
		emitter->SetStartColor(PPConstant<Color>((Color)(color * 0.7f)));
		emitter->SetGravity(PPConstant<Vector3>(Vector3(0.0f, 0.0f, -9.81f)));
		emitter->position.y = 0.0f;
		emitter->position = m_track->TransformPoint(emitter->position);
		emitter->scale = 0.3f;
		return emitter;
	}
	Ref<ParticleEmitter> CreateHoldEmitter(const Color& color, float width)
	{
		Ref<ParticleEmitter> emitter = m_particleSystem->AddEmitter();
		emitter->material = particleMaterial;
		emitter->texture = basicParticleTexture;
		emitter->loops = 0;
		emitter->duration = 5.0f;
		emitter->SetSpawnRate(PPRandomRange<float>(50, 100));
		emitter->SetStartPosition(PPBox({ width, 0.0f, 0.0f }));
		emitter->SetStartSize(PPRandomRange<float>(0.3f, 0.35f));
		emitter->SetScaleOverTime(PPRange<float>(1.2f, 1.0f));
		emitter->SetFadeOverTime(PPRange<float>(1.0f, 0.0f));
		emitter->SetLifetime(PPRandomRange<float>(0.10f, 0.15f));
		emitter->SetStartDrag(PPConstant<float>(0.0f));
		emitter->SetStartVelocity(PPConstant<Vector3>({ 0.0f, 0.0f, 0.0f }));
		emitter->SetSpawnVelocityScale(PPRandomRange<float>(0.2f, 0.2f));
		emitter->SetStartColor(PPConstant<Color>((Color)(color*0.6f)));
		emitter->SetGravity(PPConstant<Vector3>(Vector3(0.0f, 0.0f, -4.81f)));
		emitter->position.y = 0.0f;
		emitter->position = m_track->TransformPoint(emitter->position);
		emitter->scale = 1.0f;
		return emitter;
	}
	Ref<ParticleEmitter> CreateExplosionEmitter(const Color& color, const Vector3 dir)
	{
		Ref<ParticleEmitter> emitter = m_particleSystem->AddEmitter();
		emitter->material = particleMaterial;
		emitter->texture = basicParticleTexture;
		emitter->loops = 1;
		emitter->duration = 0.2f;
		emitter->SetSpawnRate(PPRange<float>(200, 0));
		emitter->SetStartPosition(PPSphere(0.1f));
		emitter->SetStartSize(PPRandomRange<float>(0.7f, 1.1f));
		emitter->SetFadeOverTime(PPRangeFadeIn<float>(0.9f, 0.0f, 0.0f));
		emitter->SetLifetime(PPRandomRange<float>(0.22f, 0.3f));
		emitter->SetStartDrag(PPConstant<float>(0.2f));
		emitter->SetSpawnVelocityScale(PPRandomRange<float>(1.0f, 4.0f));
		emitter->SetScaleOverTime(PPRange<float>(1.0f, 0.4f));
		emitter->SetStartVelocity(PPConstant<Vector3>(dir * 5.0f));
		emitter->SetStartColor(PPConstant<Color>(color));
		emitter->SetGravity(PPConstant<Vector3>(Vector3(0.0f, 0.0f, -9.81f)));
		emitter->position.y = 0.0f;
		emitter->position = m_track->TransformPoint(emitter->position);
		emitter->scale = 0.4f;
		return emitter;
	}
	Ref<ParticleEmitter> CreateHitEmitter(const Color& color, float width)
	{
		Ref<ParticleEmitter> emitter = m_particleSystem->AddEmitter();
		emitter->material = particleMaterial;
		emitter->texture = basicParticleTexture;
		emitter->loops = 1;
		emitter->duration = 0.15f;
		emitter->SetSpawnRate(PPRange<float>(50, 0));
		emitter->SetStartPosition(PPBox(Vector3(width * 0.5f, 0.0f, 0)));
		emitter->SetStartSize(PPRandomRange<float>(0.3f, 0.1f));
		emitter->SetFadeOverTime(PPRangeFadeIn<float>(0.7f, 0.0f, 0.0f));
		emitter->SetLifetime(PPRandomRange<float>(0.35f, 0.4f));
		emitter->SetStartDrag(PPConstant<float>(6.0f));
		emitter->SetSpawnVelocityScale(PPConstant<float>(0.0f));
		emitter->SetScaleOverTime(PPRange<float>(1.0f, 0.4f));
		emitter->SetStartVelocity(PPCone(Vector3(0,0,-1), 90.0f, 1.0f, 4.0f));
		emitter->SetStartColor(PPConstant<Color>(color));
		emitter->position.y = 0.0f;
		return emitter;
	}

	// Main GUI/HUD Rendering loop
	virtual void RenderDebugHUD(float deltaTime)
	{
		// Render debug overlay elements
		//RenderQueue& debugRq = g_guiRenderer->Begin();
		auto RenderText = [&](const String& text, const Vector2& pos, const Color& color = {1.0f, 1.0f, 0.5f, 1.0f})
		{
			g_application->FastText(text, pos.x + 1, pos.y + 1, 12, 0, Color::Black);
			g_application->FastText(text, pos.x, pos.y, 12, 0, color);
			return Vector2(0, 12);
		};

		//Vector2 canvasRes = GUISlotBase::ApplyFill(FillMode::Fit, Vector2(640, 480), Rect(0, 0, g_resolution.x, g_resolution.y)).size;
		//Vector2 topLeft = Vector2(g_resolution / 2 - canvasRes / 2);
		//Vector2 bottomRight = topLeft + canvasRes;
		//topLeft.y = Math::Min(topLeft.y, g_resolution.y * 0.2f);

		const BeatmapSettings& bms = m_beatmap->GetMapSettings();
		const TimingPoint& tp = m_playback.GetCurrentTimingPoint();
		//Vector2 textPos = topLeft + Vector2i(5, 0);
		Vector2 textPos = Vector2i(5, 0);
		textPos.y += RenderText(bms.title, textPos).y;
		textPos.y += RenderText(bms.artist, textPos).y;
		textPos.y += RenderText(Utility::Sprintf("%.2f FPS", g_application->GetRenderFPS()), textPos).y;
		textPos.y += RenderText(Utility::Sprintf("Offset (ms): Global %d, Song %d, Audio %d (%d)",
			m_globalOffset, m_songOffset, GetAudioOffset(), g_audio->audioLatency), textPos).y;

		float currentBPM = (float)(60000.0 / tp.beatDuration);
		textPos.y += RenderText(Utility::Sprintf("BPM: %.1f | Time Sig: %d/%d", currentBPM, tp.numerator, tp.denominator), textPos).y;
		textPos.y += RenderText(Utility::Sprintf("Hit Window: p=%d g=%d h=%d s=%d m=%d",
			m_scoring.hitWindow.perfect, m_scoring.hitWindow.good, m_scoring.hitWindow.hold, m_scoring.hitWindow.slam, m_scoring.hitWindow.miss), textPos).y;
		textPos.y += RenderText(Utility::Sprintf("Paused: %s, LastMapTime: %d", m_paused ? "Yes" : "No", m_lastMapTime), textPos).y;
		if (IsPartialPlay())
			textPos.y += RenderText(Utility::Sprintf("Partial play: from %d ms to %d ms", m_playOptions.range.begin, m_playOptions.range.end), textPos).y;
		textPos.y += RenderText(Utility::Sprintf("Laser Filter Input: %f", m_scoring.GetLaserOutput()), textPos).y;

		textPos.y += RenderText(Utility::Sprintf("Score: %d/%d (Max: %d)", m_scoring.currentHitScore, m_scoring.currentMaxScore, m_scoring.mapTotals.maxScore), textPos).y;
		textPos.y += RenderText(Utility::Sprintf("Actual Score: %d", m_scoring.CalculateCurrentScore()), textPos).y;
		Gauge* gauge = m_scoring.GetTopGauge();

		if (gauge)
		{
			textPos.y += RenderText(Utility::Sprintf("Health Gauge: %s, %f", gauge->GetName(), gauge->GetValue()), textPos).y;
		}

		textPos.y += RenderText(Utility::Sprintf("Roll: %f(x%f) %s",
			m_camera.GetRoll(), m_rollIntensity, m_camera.GetRollKeep() ? "[Keep]" : ""), textPos).y;

		textPos.y += RenderText(Utility::Sprintf("Track Zoom Top: %f", m_camera.pLanePitch), textPos).y;
		textPos.y += RenderText(Utility::Sprintf("Track Zoom Bottom: %f", m_camera.pLaneZoom), textPos).y;

		Vector2 buttonStateTextPos = Vector2(g_resolution.x - 200.0f, 100.0f);
		RenderText(g_input.GetControllerStateString(), buttonStateTextPos);

		if (!IsStorableScore())
		{
			if (m_isPracticeSetup)
			{
				textPos.y += RenderText("Practice setup", textPos, Color::Magenta).y;
			}
			else if(m_scoring.autoplayInfo.autoplay)
			{
				textPos.y += RenderText("Autoplay enabled", textPos, Color::Magenta).y;
			}
			else
			{
				textPos.y += RenderText("Score not storable", textPos, Color::Magenta).y;
			}
		}

		uint32 hitsShown = 0;
		// Show all hit debug info on screen (up to a maximum)
		for(auto it = m_scoring.hitStats.rbegin(); it != m_scoring.hitStats.rend(); it++)
		{
			if(hitsShown++ > 16) // Max of 16 entries to display
				break;

			static Color hitColors[] = {
				Color::Red,
				Color::Yellow,
				Color::Green,
			};
			Color c = hitColors[(size_t)(*it)->rating];
			if((*it)->hasMissed && (*it)->hold > 0)
				c = Color(1, 0.65f, 0);
			String text;

			MultiObjectState* obj = *(*it)->object;
			if(obj->type == ObjectType::Single)
			{
				text = Utility::Sprintf("Button [%d] %d", obj->button.index, (*it)->delta);
			}
			else if(obj->type == ObjectType::Hold)
			{
				text = Utility::Sprintf("Hold [%d] [%d/%d]", obj->button.index, (*it)->hold, (*it)->holdMax);
			}
			else if(obj->type == ObjectType::Laser)
			{
				text = Utility::Sprintf("Laser [%d] [%d/%d]", obj->laser.index, (*it)->hold, (*it)->holdMax);
			}
			textPos.y += RenderText(text, textPos, c).y;
		}

		//g_guiRenderer->End();
	}

	void OnLaserSlam(LaserObjectState* object)
	{
		// Note: this merely simulates the slam roll effect. The way SDVX does laser slams is probably just putting
		// a straight laser segment to the tail of the slam. This isn't exactly ideal for USC as it'll limit laser skinning.
		if (object != nullptr)
		{
			// Set flag
			object->flags |= LaserObjectState::flag_slamProcessed;
			uint8 index = object->index;
			float tail = m_scoring.GetLaserPosition(index, object->points[1]);
			m_camera.SetSlamAmount(index, tail);
		}
	}

	void OnLaserExit(LaserObjectState* object)
	{
		if (object != nullptr)
			m_camera.SetRollIgnore(object->index, false);
	}

	void OnLaserSlamHit(LaserObjectState* object)
	{
		float slamSize = object->points[1] - object->points[0];
		float direction = Math::Sign(slamSize);
		m_camera.AddCameraShake(slamSize);
		m_slamSample->Play();

		if (object->spin.type != 0)
		{
			if (object->spin.type == SpinStruct::SpinType::Bounce)
				m_camera.SetXOffsetBounce(object->GetDirection(), object->spin.duration, object->spin.amplitude, object->spin.frequency, object->spin.duration, m_playback);
			else m_camera.SetSpin(object->GetDirection(), object->spin.duration, object->spin.type, m_playback);
		}

		float width = (object->flags & LaserObjectState::flag_Extended) ? 2.0 : 1.0;
		float startPos = (m_track->trackWidth * object->points[0] - m_track->trackWidth * 0.5f) * width;
		float endPos = (m_track->trackWidth * object->points[1] - m_track->trackWidth * 0.5f) * width;

		Ref<ParticleEmitter> ex = CreateExplosionEmitter(m_track->laserColors[object->index], Vector3(direction, 0, 0));
		ex->position = Vector3(endPos, 0.0f, -0.05f);
		ex->position = m_track->TransformPoint(ex->position);

		//call lua button_hit if it exists
		lua_getglobal(m_lua, "laser_slam_hit");
		if (lua_isfunction(m_lua, -1))
		{
			// Slam size and direction
			lua_pushnumber(m_lua, slamSize * width);
			// Start position
			lua_pushnumber(m_lua, startPos);
			// End position
			lua_pushnumber(m_lua, endPos);
			// Laser index
			lua_pushnumber(m_lua, object->index);
			if (lua_pcall(m_lua, 4, 0, 0) != 0)
			{
				Logf("Lua error on calling laser_slam_hit: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			}
		}
		lua_settop(m_lua, 0);
	}

	void OnButtonHit(Input::Button button, ScoreHitRating rating, ObjectState* hitObject, MapTime delta)
	{
		ButtonObjectState* st = (ButtonObjectState*)hitObject;
		uint32 buttonIdx = (uint32)button;
		Color c = m_track->hitColors[(size_t)rating];
		bool skipEffect = m_scoring.HoldObjectAvailable((uint32) button, false) && !m_delayedHitEffects;

		if (!skipEffect)
            m_track->AddHitEffect(buttonIdx, c, st && st->type == ObjectType::Hold);

		if (st != nullptr && st->hasSample)
		{
			if (m_fxSamples[st->sampleIndex])
			{
				m_fxSamples[st->sampleIndex]->SetVolume(st->sampleVolume);
				m_fxSamples[st->sampleIndex]->Play();
			}
		}

		if (rating != ScoreHitRating::Idle)
		{
			// Floating text effect
			m_track->AddEffect(new ButtonHitRatingEffect(buttonIdx, rating));

			if (rating == ScoreHitRating::Good)
			{
				//m_track->timedHitEffect->late = late;
				//m_track->timedHitEffect->Reset(0.75f);
				lua_getglobal(m_lua, "near_hit");
				lua_pushboolean(m_lua, delta > 0);
				if (lua_pcall(m_lua, 1, 0, 0) != 0)
				{
					Logf("Lua error on calling near_hit: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
				}
			}

			//set button invisible
			if (st != nullptr)
				m_hiddenObjects.insert(hitObject);

			// Create hit effect particle
			Color hitColor = (buttonIdx < 4) ? Color::White : Color::FromHSV(20, 0.7f, 1.0f);
			float hitWidth = (buttonIdx < 4) ? m_track->buttonWidth : m_track->fxbuttonWidth;
			if (particleMaterial && basicParticleTexture) 
			{
				Ref<ParticleEmitter> emitter = CreateHitEmitter(hitColor, hitWidth);
				emitter->position.x = m_track->GetButtonPlacement(buttonIdx);
				emitter->position.z = -0.05f;
				emitter->position.y = 0.0f;
				emitter->position = m_track->TransformPoint(emitter->position);
			}
		}

		//call lua button_hit if it exists
		lua_getglobal(m_lua, "button_hit");
		if (lua_isfunction(m_lua, -1))
		{
			lua_pushnumber(m_lua, buttonIdx);
			lua_pushnumber(m_lua, (int)rating);
			lua_pushnumber(m_lua, delta);
			if (lua_pcall(m_lua, 3, 0, 0) != 0)
			{
				Logf("Lua error on calling button_hit: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			}
		}
		lua_settop(m_lua, 0);
	}

	void OnButtonMiss(Input::Button button, bool hitEffect, ObjectState* object)
	{
		uint32 buttonIdx = (uint32)button;
		if (hitEffect)
		{
			ButtonObjectState* st = (ButtonObjectState*)object;
			//m_hiddenObjects.insert(object);
			Color c = m_track->hitColors[0];
			m_track->AddHitEffect(buttonIdx, c);
		}
		m_track->AddEffect(new ButtonHitRatingEffect(buttonIdx, ScoreHitRating::Miss));


		lua_getglobal(m_lua, "button_hit");
		if (lua_isfunction(m_lua, -1))
		{
			lua_pushnumber(m_lua, buttonIdx);
			lua_pushnumber(m_lua, (int)ScoreHitRating::Miss);
			lua_pushnumber(m_lua, 0);
			if (lua_pcall(m_lua, 3, 0, 0) != 0)
			{
				Logf("Lua error on calling button_hit: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			}
		}
		lua_settop(m_lua, 0);
	}

	void OnComboChanged(uint32 newCombo)
	{
		m_comboAnimation.Restart();
		lua_getglobal(m_lua, "update_combo");
		lua_pushinteger(m_lua, newCombo);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on calling update_combo: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
		}
	}

	void OnScoreChanged()
	{
		lua_getglobal(m_lua, "update_score");
		lua_pushinteger(m_lua, m_scoring.CalculateCurrentDisplayScore());
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on calling update_score: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
		}
	}

	void OnGaugeChanged(Gauge* from, Gauge* to) {
		Logf("Gauge changed: %s -> %s", Logger::Severity::Info, from->GetName(), to->GetName());
		//TODO: Lua call?
	}

	// These functions control if FX button DSP's are muted or not
	void OnObjectHold(Input::Button buttonCode, ObjectState* object)
	{
	    auto buttonIdx = (int)buttonCode;
		if(object->type == ObjectType::Hold)
		{
			HoldObjectState* hold = (HoldObjectState*)object;
			if(hold->effectType != EffectType::None)
			{
                m_audioPlayback.SetEffectEnabled(hold->index - 4, true);
            }
		}
	}

	void OnObjectReleased(Input::Button, ObjectState* object)
	{
		if(object->type == ObjectType::Hold)
		{
			HoldObjectState* hold = (HoldObjectState*)object;
			if(hold->effectType != EffectType::None)
			{
				m_audioPlayback.SetEffectEnabled(hold->index - 4, false);
			}
		}
	}

	void OnTimingPointChanged(TimingPoint* tp)
	{
	   m_hispeed = m_modSpeed / tp->GetBPM(); 
	}
	void OnTimingPointChangedChallenge(TimingPoint* tp)
	{
		CheckChallengeHispeed(tp->GetBPM());
	}

	void OnLaneToggleChanged(LaneHideTogglePoint* tp)
	{
		// Calculate how long the transition should be in seconds
		double duration = m_currentTiming->beatDuration * 4.0f * (tp->duration / 192.0f) * 0.001f;
		m_track->SetLaneHide(!m_hideLane, duration);
		m_hideLane = !m_hideLane;
	}

	void OnEventChanged(EventKey key, EventData data)
	{
		if(key == EventKey::LaserEffectType)
		{
			m_audioPlayback.SetLaserEffect(data.effectVal);
		}
		else if(key == EventKey::LaserEffectMix)
		{
			m_audioPlayback.SetLaserEffectMix(data.floatVal);
		}
		else if(key == EventKey::TrackRollBehaviour)
		{
			m_camera.SetRollKeep((data.rollVal & TrackRollBehaviour::Keep) == TrackRollBehaviour::Keep);
			int32 i = (uint8)data.rollVal & 0x7;

			m_manualTiltEnabled = false;
			if (i == (uint8)TrackRollBehaviour::Manual)
			{
				// switch to manual tilt mode
				m_manualTiltEnabled = true;
			}
			else if (i == 0)
				m_rollIntensity = 0;
			else
			{
				m_rollIntensity = MAX_ROLL_ANGLE * (1.0 + 0.75 * (i - 1));
			}
			m_camera.SetRollIntensity(m_rollIntensity);
		}
		else if(key == EventKey::SlamVolume)
		{
			m_slamSample->SetVolume(data.floatVal);
		}
		else if (key == EventKey::ChartEnd)
		{
			// Don't call EndCurrentRun here
			// (Modifying BeatmapPlayback on OnEventChanged may cause crash, as it will invalidate the iterator.)
			m_triggerEnd = true;
		}
	}

	// These functions register / remove DSP's for the effect buttons
	// the actual hearability of these is toggled in the tick by wheneter the buttons are held down
	void OnFXBegin(HoldObjectState* object)
	{
		assert(object->index >= 4 && object->index <= 5);
		m_audioPlayback.SetEffect(object->index - 4, object, m_playback);
	}
	void OnFXEnd(HoldObjectState* object)
	{
		assert(object->index >= 4 && object->index <= 5);
		uint32 index = object->index - 4;
		m_audioPlayback.ClearEffect(index, object);
	}
	void OnLaserAlertEntered(LaserObjectState* object)
	{
		if (m_scoring.timeSinceLaserUsed[object->index] > 3.0f)
		{
			m_track->SendLaserAlert(object->index);
			lua_getglobal(m_lua, "laser_alert");
			lua_pushboolean(m_lua, object->index == 1);
			if (lua_pcall(m_lua, 1, 0, 0) != 0)
			{
				Logf("Lua error on calling laser_alert: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			}
		}
	}

	void OnKeyPressed(SDL_Scancode code) override
	{
		if (!m_isPracticeSetup && g_gameConfig.GetBool(GameConfigKeys::DisableNonButtonInputsDuringPlay))
			return;

		if (m_practiceSetupDialog && m_practiceSetupDialog->IsActive())
		{
			if (code != SDL_SCANCODE_F8) return;
		}

		if (m_isPracticeSetup && m_isPracticeSetupNavEnabled)
		{
			assert(!IsMultiplayerGame());

			switch (code)
			{
			case SDL_SCANCODE_SPACE:
				m_audioPlayback.TogglePause();
				m_paused = m_audioPlayback.IsPaused();
				return;
			case SDL_SCANCODE_UP:
			case SDL_SCANCODE_DOWN:
			{
				const MapTime increment = 2000 * (code == SDL_SCANCODE_UP ? 1 : -1);

				JumpTo(Math::Clamp(m_lastMapTime + increment, 0, m_endTime));
				return;
			}
			default:
				break;
			}
		}

		if (code == SDL_SCANCODE_BACKSPACE && m_isPracticeMode)
		{
			// Bailout
			m_manualExit = true;
			FinishGame();
		}
		else if(code == SDL_SCANCODE_PAUSE && !IsMultiplayerGame() && !IsChallenge())
		{
			m_audioPlayback.TogglePause();
			m_paused = m_audioPlayback.IsPaused();
		}
		else if(code == SDL_SCANCODE_RETURN) // Skip intro
		{
			if(!SkipIntro() && !m_isPracticeSetup)
				SkipOutro();
		}
		else if(code == SDL_SCANCODE_PAGEUP && !IsMultiplayerGame() && !IsChallenge())
		{
			m_audioPlayback.Advance(5000);
		}
		else if(code == SDL_SCANCODE_F5 && !IsMultiplayerGame() && !IsChallenge() && !m_isPracticeSetup)
		{
			AbortMethod abortMethod = g_gameConfig.GetEnum<Enum_AbortMethod>(GameConfigKeys::RestartPlayMethod);
			if (abortMethod == AbortMethod::Press)
			{
				Restart();
			}
			else if(abortMethod == AbortMethod::Hold && !m_restartTriggerTimeSet)
			{
				m_restartTriggerTime = m_lastMapTime + g_gameConfig.GetInt(GameConfigKeys::RestartPlayHoldDuration);
				m_restartTriggerTimeSet = true;
			}
		}
		else if(code == SDL_SCANCODE_F8)
		{
			m_renderDebugHUD = !m_renderDebugHUD;
		}
		else if(code == SDL_SCANCODE_TAB)
		{
			//g_gameWindow->SetCursorVisible(!m_settingsBar->IsShown());
			//m_settingsBar->SetShow(!m_settingsBar->IsShown());
		}
		else if(code == SDL_SCANCODE_F9)
		{
			g_application->ReloadScript("gameplay", m_lua);
		}
	}

	void OnKeyReleased(SDL_Scancode code) override
	{
		if (m_practiceSetupDialog && m_practiceSetupDialog->IsActive())
			return;

		if (code == SDL_SCANCODE_F5)
		{
			m_restartTriggerTimeSet = false;
		}
	}

	void TriggerManualExit()
	{
		if (IsSuccessfullyInitialized())
		{
			if (m_isPracticeMode && !m_isPracticeSetup)
			{
				RevertToPracticeSetup();
				return;
			}

			ObjectState* const* lastObj = &m_beatmap->GetLinearObjects().back();
			MapTime timePastEnd = m_lastMapTime - (*lastObj)->time;
			if (timePastEnd < 0)
				m_manualExit = true;

			FinishGame();
		}
	}
	void m_OnButtonPressed(Input::Button buttonCode)
	{
		if (m_practiceSetupDialog && m_practiceSetupDialog->IsActive())
			return;

		if (m_isPracticeSetup)
		{
			if (buttonCode != Input::Button::Back && !m_isPracticeSetupNavEnabled)
				return;

			switch (buttonCode)
			{
			case Input::Button::Back:
			case Input::Button::FX_0:
			case Input::Button::FX_1:
				if (buttonCode == Input::Button::Back || g_input.GetButton(buttonCode == Input::Button::FX_0 ? Input::Button::FX_1 : Input::Button::FX_0))
				{
					m_audioPlayback.Pause();
					m_paused = true;

					if (m_practiceSetupDialog)
						m_practiceSetupDialog->Open();
				}
				else
				{
					m_playOnDialogClose = !m_paused;
					m_audioPlayback.TogglePause();
					m_paused = m_audioPlayback.IsPaused();
				}
				break;
			default:
				break;
			}

			return;
		}

		if (buttonCode == Input::Button::Back && IsSuccessfullyInitialized())
		{
			AbortMethod abortMethod = g_gameConfig.GetEnum<Enum_AbortMethod>(GameConfigKeys::ExitPlayMethod);
			if (m_audioPlayback.IsPaused() || abortMethod == AbortMethod::Press)
			{
				TriggerManualExit();
			}
			else if(!m_exitTriggerTimeSet)
			{
				int exitPlayHoldDuration = g_gameConfig.GetInt(GameConfigKeys::ExitPlayHoldDuration);

				// Set arbitrary hold duration if the practice mode is being played now.
				// This is to prevent softlocks.
				if (abortMethod == AbortMethod::None && m_isPracticeMode && (m_playOptions.loopOnFail || m_playOptions.loopOnSuccess))
				{
					abortMethod = AbortMethod::Hold;
					exitPlayHoldDuration = 2000;
				}

				if (abortMethod == AbortMethod::Hold)
				{
					m_exitTriggerTime = m_lastMapTime + (MapTime)(exitPlayHoldDuration * m_audioPlayback.GetPlaybackSpeed());
					m_exitTriggerTimeSet = true;
				}
			}
		}
	}
	ClearMark m_getClearState()
	{
		Gauge* g = m_scoring.GetTopGauge();

		if (g == nullptr)
			return ClearMark::NotPlayed;

		if (m_manualExit)
			return ClearMark::NotPlayed;

		if (m_playOptions.failCondition && m_playOptions.failCondition->IsFailed(m_scoring))
			return ClearMark::Played;

		ScoreIndex scoreData;
		auto opts = GetPlaybackOptions();
		scoreData.miss = m_scoring.categorizedHits[0];
		scoreData.almost = m_scoring.categorizedHits[1];
		scoreData.crit = m_scoring.categorizedHits[2];
		scoreData.gaugeType = g->GetType();
		scoreData.gaugeOption = g->GetOpts();
		scoreData.mirror = opts.mirror;
		scoreData.random = opts.random;
		scoreData.autoFlags = opts.autoFlags;
		scoreData.gauge = g->GetValue();
		scoreData.score = m_scoring.CalculateCurrentScore();

		return Scoring::CalculateBadge(scoreData);
	}

	void m_setLuaHolds(lua_State* L)
	{
		//button
		lua_pushstring(L, "noteHeld");
		lua_newtable(L);
		for (size_t i = 0; i < 6; i++)
		{
			lua_pushnumber(L, i + 1);
			lua_pushboolean(L, m_scoring.IsObjectHeld(i));
			lua_settable(L, -3);
		}
		lua_settable(L, -3);

		//laser
		lua_pushstring(L, "laserActive");
		lua_newtable(L);
		for (size_t i = 0; i < 2; i++)
		{
			lua_pushnumber(L, i + 1);
			lua_pushboolean(L, m_scoring.IsObjectHeld(6 + i));
			lua_settable(L, -3);
		}
		lua_settable(L, -3);
	}

	// Skips ahead to the right before the first object in the map
	bool SkipIntro()
	{
		ObjectState* const* firstObj = &m_beatmap->GetLinearObjects().front();
		while ((*firstObj)->type == ObjectType::Event && firstObj != &m_beatmap->GetLinearObjects().back())
		{
			firstObj++;
		}
		MapTime skipTime = (*firstObj)->time - 1000;
		if (skipTime > m_lastMapTime)
		{
			// In multiplayer mode we have to stay synced
			if (m_multiplayer != nullptr)
				return true;

			m_audioPlayback.SetPosition(skipTime);
			return true;
		}
		return false;
	}
	// Skips ahead at the end to the score screen
	void SkipOutro()
	{
		// Just to be sure
		if (m_beatmap->GetLinearObjects().empty())
		{
			FinishGame();
			return;
		}

		// Check if last object has passed
		ObjectState* const* lastObj = &m_beatmap->GetLinearObjects().back();
		MapTime timePastEnd = m_lastMapTime - (*lastObj)->time;
		if (timePastEnd > 250)
		{
			FinishGame();
		}
	}

	void MakeMultiplayer(MultiplayerScreen* multiplayer)
	{
		assert(!m_isPracticeSetup);
		m_multiplayer = multiplayer;
	}

	void MakeChallenge(ChallengeManager* manager)
	{
		m_challengeManager = manager;
	}

	// Called only for initialization of practice setup
	// (Do not use this for re-entering the setup scene)
	void MakePracticeSetup()
	{
		assert(!IsMultiplayerGame());

		m_isPracticeSetup = true;
		m_isPracticeMode = true;

		m_isPracticeSetupNavEnabled = g_gameConfig.GetBool(GameConfigKeys::PracticeSetupNavEnabled);

		m_scoring.autoplayInfo.autoplay = true;

		m_practiceSetupRange = m_playOptions.range;
		m_playOptions.range = { 0, 0 };

		m_playOptions.loopOnSuccess = false;
		m_playOptions.loopOnFail = true;

		m_playOnDialogClose = true;
		m_triggerPause = true;
	}

	void LoadPracticeSetupIndex()
	{
		if (!m_db) return;
		assert(m_isPracticeMode);

		Vector<PracticeSetupIndex*> practiceSetups = m_db->GetOrAddPracticeSetups(m_chartIndex->id);
		if (practiceSetups.empty()) return;

		const PracticeSetupIndex* practiceSetup = practiceSetups.front();

		m_playOptions.loopOnSuccess = practiceSetup->loopSuccess != 0;
		m_playOptions.loopOnFail = practiceSetup->loopFail != 0;
		m_practiceSetupRange.begin = practiceSetup->rangeBegin;
		m_practiceSetupRange.end = practiceSetup->rangeEnd;
		m_playOptions.failCondition = GameFailCondition::CreateGameFailCondition(
			practiceSetup->failCondType, practiceSetup->failCondValue
		);
		m_playOptions.playbackSpeed = (float) practiceSetup->playbackSpeed;
		
		m_playOptions.incSpeedOnSuccess = practiceSetup->incSpeedOnSuccess != 0;
		m_playOptions.incSpeedAmount = (float) practiceSetup->incSpeed;
		m_playOptions.incStreak = practiceSetup->incStreak;

		m_playOptions.decSpeedOnFail = practiceSetup->decSpeedOnFail != 0;
		m_playOptions.decSpeedAmount = (float) practiceSetup->decSpeed;
		m_playOptions.minPlaybackSpeed = (float) practiceSetup->minPlaybackSpeed;

		m_playOptions.enableMaxRewind = practiceSetup->maxRewind != 0;
		m_playOptions.maxRewindMeasure = practiceSetup->maxRewindMeasure;
	}

	void StorePracticeSetupIndex()
	{
		if (!m_db) return;
		assert(m_isPracticeMode);

		Vector<PracticeSetupIndex*> practiceSetups = m_db->GetOrAddPracticeSetups(m_chartIndex->id);
		if (practiceSetups.empty()) return;

		PracticeSetupIndex* practiceSetup = practiceSetups.front();

		practiceSetup->loopSuccess = m_playOptions.loopOnSuccess ? 1 : 0;
		practiceSetup->loopFail = m_playOptions.loopOnFail ? 1 : 0;
		practiceSetup->rangeBegin = m_practiceSetupRange.begin;
		practiceSetup->rangeEnd = m_practiceSetupRange.end;
		practiceSetup->failCondType = static_cast<int32>(
			m_playOptions.failCondition ? m_playOptions.failCondition->GetType() : GameFailCondition::Type::None
		);
		practiceSetup->failCondValue = m_playOptions.failCondition ? m_playOptions.failCondition->GetThreshold() : 0;
		practiceSetup->playbackSpeed = m_playOptions.playbackSpeed;

		practiceSetup->incSpeedOnSuccess = m_playOptions.incSpeedOnSuccess ? 1 : 0;
		practiceSetup->incSpeed = m_playOptions.incSpeedAmount;
		practiceSetup->incStreak = m_playOptions.incStreak;

		practiceSetup->decSpeedOnFail = m_playOptions.decSpeedOnFail ? 1 : 0;
		practiceSetup->decSpeed = m_playOptions.decSpeedAmount;
		practiceSetup->minPlaybackSpeed = m_playOptions.minPlaybackSpeed;

		practiceSetup->maxRewind = m_playOptions.enableMaxRewind ? 1 : 0;
		practiceSetup->maxRewindMeasure = m_playOptions.maxRewindMeasure;

		m_db->UpdatePracticeSetup(practiceSetup);
	}

	void m_InitPracticeSetupDialog()
	{
		assert(m_isPracticeSetup && m_isPracticeMode && m_practiceSetupDialog && !IsMultiplayerGame());

		if (!m_practiceSetupDialog->Init())
		{
			Log("Failed to initialize PracticeSetupDialog!", Logger::Severity::Error);
			return;
		}

		m_practiceSetupDialog->onSetMapTime.AddLambda([this](MapTime time) { if (m_isPracticeSetup) { m_paused = true; JumpTo(time); } });
		m_practiceSetupDialog->onSpeedChange.AddLambda([this](float speed) { if (m_isPracticeSetup) { m_playOptions.playbackSpeed = speed; ApplyPlaybackSpeed(); } });
		m_practiceSetupDialog->onClose.AddLambda([this]() {
			if (m_playOnDialogClose) m_audioPlayback.Play();
			else m_audioPlayback.Pause();

			m_paused = m_audioPlayback.IsPaused();
		});
		m_practiceSetupDialog->onSettingChange.AddLambda([this]() {
			int oldAudioOffset = GetAudioOffset();

			m_globalOffset = g_gameConfig.GetInt(GameConfigKeys::GlobalOffset);
			if(m_chartIndex) m_songOffset = m_chartIndex->custom_offset;

			if(oldAudioOffset != GetAudioOffset())
			{
				JumpTo(m_lastMapTime);
			}

			m_isPracticeSetupNavEnabled = g_gameConfig.GetBool(GameConfigKeys::PracticeSetupNavEnabled);
			if (!m_isPracticeSetupNavEnabled)
				m_playOnDialogClose = true;
		});
		m_practiceSetupDialog->onPressStart.Add(this, &Game_Impl::StartPractice);
		m_practiceSetupDialog->onPressExit.Add(this, &Game_Impl::TriggerManualExit);
	}

	void RevertToPracticeSetup()
	{
		assert(m_isPracticeMode);

		m_isPracticeSetup = true;
		m_scoring.autoplayInfo.autoplay = true;

		m_track->hitEffectAutoplay = true;

		m_playOptions.range = { 0, 0 };
		m_playOnDialogClose = true;

		m_audioPlayback.Pause();
		m_paused = true;
		m_ended = false;
		m_outroCompleted = false;

		if (g_gameConfig.GetBool(GameConfigKeys::DisplayPracticeInfoInGame))
		{
			lua_getglobal(m_lua, "practice_end");
			if (lua_isfunction(m_lua, -1))
			{
				lua_pushinteger(m_lua, m_loopCount);
				lua_pushinteger(m_lua, m_loopSuccess);
				if (lua_pcall(m_lua, 2, 0, 0) != 0)
				{
					Logf("Lua error on calling practice_end: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
				}
			}
			lua_settop(m_lua, 0);
		}

		JumpTo(m_lastMapTime);
		
		if (m_practiceSetupDialog)
			m_practiceSetupDialog->Open();
	}

	void StartPractice()
	{
		assert(m_isPracticeMode && m_isPracticeSetup);

		m_isPracticeSetup = false;
		m_scoring.autoplayInfo.autoplay = false;

		m_track->hitEffectAutoplay = false;

		m_paused = false;
		m_triggerPause = false;

		m_playOptions.range = m_practiceSetupRange;
		
		if (m_practiceSetupDialog && m_practiceSetupDialog->IsActive())
			m_practiceSetupDialog->Close();

		m_loopCount = 0;
		m_loopSuccess = 0;
		m_loopStreak = 0;

		if (g_gameConfig.GetBool(GameConfigKeys::DisplayPracticeInfoInGame))
		{
			lua_getglobal(m_lua, "practice_start");
			if (lua_isfunction(m_lua, -1))
			{
				if (const auto& failCondition = m_playOptions.failCondition)
				{
					lua_pushstring(m_lua, failCondition->GetName());
					lua_pushinteger(m_lua, failCondition->GetThreshold());
					lua_pushstring(m_lua, failCondition->GetDescription().c_str());
				}
				else
				{
					lua_pushstring(m_lua, "None");
					lua_pushinteger(m_lua, 0);
					lua_pushstring(m_lua, "None");
				}
				if (lua_pcall(m_lua, 3, 0, 0) != 0)
				{
					Logf("Lua error on calling practice_start: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
				}
			}
			lua_settop(m_lua, 0);
		}

		if (m_db)
		{
			StorePracticeSetupIndex();
		}

		Restart();
	}

	constexpr bool IsPartialPlay() const noexcept
	{
		if (m_playOptions.range.begin > 0) return true;
		return m_playOptions.range.HasEnd();
	}
	
	inline HitWindow GetHitWindow() const
	{
		return m_hitWindow;
	}

	virtual bool IsPlaying() const override
	{
		return m_playing;
	}

	virtual bool GetTickRate(int32& rate) override
	{
		if(!m_audioPlayback.IsPaused())
		{
			rate = m_fpsTarget;
			return true;
		}
		return false; // Default otherwise
	}

	virtual Texture GetJacketImage() override
	{
		return m_jacketTexture;
	}
	virtual Ref<Beatmap> GetBeatmap() override
	{
		return m_beatmap;
	}
	virtual class Track& GetTrack() override
	{
		return *m_track;
	}
	virtual class Camera& GetCamera() override
	{
		return m_camera;
	}
	virtual class BeatmapPlayback& GetPlayback() override
	{
		return m_playback;
	}
	virtual class Scoring& GetScoring() override
	{
		return m_scoring;
	}
	virtual const std::array<float, 256>& GetGaugeSamples() override
	{
		return m_scoring.GetTopGauge()->GetSamples();
	}
	virtual PlaybackOptions GetPlaybackOptions() override
	{
		return m_playOptions.playbackOptions;
	}
	virtual lua_State* GetLuaState() override 
	{
		return m_lua;
	}
	virtual void SetGauge(float g) override
	{
		auto gauge = m_scoring.GetTopGauge();
		if (gauge)
		{
			gauge->SetValue(g);
		}
	}
	virtual void SetAllGaugeValues(const Vector<float> values)
	{
		m_scoring.SetAllGaugeValues(values);
	}
	virtual bool IsStorableScore() override
	{
		if (m_scoring.autoplayInfo.IsAutoplayButtons()) return false;
		if (m_isPracticeSetup) return false;

		// GetPlaybackSpeed() returns 0 on end of a song
		if (m_playOptions.playbackSpeed < 1.0f) return false;

		if (m_manualExit) return false;
		if (IsPartialPlay()) return false;

		if (!(m_scoring.hitWindow <= HitWindow::NORMAL)) return false;

		return true;
	}
	float GetPlaybackSpeed() override
	{
		return m_audioPlayback.GetPlaybackSpeed();
	}
	const PlayOptions& GetPlayOptions() const override
	{
		return m_playOptions;
	}
	int GetRetryCount() const override
	{
		return m_loopCount;
	}
	String GetMissionStr() const override
	{
		return m_playOptions.failCondition ? m_playOptions.failCondition->GetDescription() : "";
	}
	const String& GetChartRootPath() const override
	{
		return m_chartRootPath;
	}
	const String& GetChartPath() const override
	{
		return m_chartPath;
	}
	bool IsMultiplayerGame() const override
	{
		return m_multiplayer != nullptr;
	}
	virtual bool IsChallenge() const
	{
		return m_challengeManager != nullptr;
	}
	ChartIndex* GetChartIndex() override
	{
		return m_chartIndex;
	}
	void SetDemoMode(bool value) override
	{
		m_demo = value;
	}
	void SetSongDB(MapDatabase* db) override
	{
		m_db = db;
		
		if (m_isPracticeMode) LoadPracticeSetupIndex();
	}
	void SetGameplayLua(lua_State* L) override
	{
		Gauge* gauge = m_scoring.GetTopGauge();

		if (gauge == nullptr) //if gauge is null, assume something is wrong
			return;


		//set lua
		lua_getglobal(L, "gameplay");

		m_setLuaHolds(L);

		//set autoplay here as it's not set during the creation of the gameplay
		lua_pushstring(L, "autoplay");
		lua_pushboolean(L, m_scoring.autoplayInfo.autoplay);
		lua_settable(L, -3);

		if (m_isPracticeMode)
		{
			// Existence of this field implies that the game's in the practice mode.
			lua_pushstring(L, "practice_setup");
			lua_pushboolean(L, m_isPracticeSetup);
			lua_settable(L, -3);
		}

		// Update score replays
		lua_getfield(L, -1, "scoreReplays");
		int replayCounter = 1;
		for (auto& replay: m_scoreReplays)
		{
			if (replay.replay.size() > 0)
			{
				while (replay.nextHitStat < replay.replay.size()
					&& replay.replay[replay.nextHitStat].time < m_lastMapTime)
				{
					SimpleHitStat shs = replay.replay[replay.nextHitStat];
					if (shs.rating < 3)
					{
						replay.currentMaxScore += 2;
						replay.currentScore += shs.rating;
					}
					replay.nextHitStat++;
				}
			}
			lua_pushnumber(L, replayCounter);
			lua_newtable(L);

			lua_pushstring(L, "maxScore");
			lua_pushnumber(L, replay.maxScore);
			lua_settable(L, -3);

			lua_pushstring(L, "currentScore");
			lua_pushnumber(L, m_scoring.CalculateCurrentDisplayScore(replay));
			lua_settable(L, -3);

			lua_settable(L, -3);
			replayCounter++;
		}
		lua_setfield(L, -1, "scoreReplays");

		// progress
		m_LuaUpdateProgress(L);

		// hispeed
		lua_pushstring(L, "hispeed");
		lua_pushnumber(L, m_hispeed);
		lua_settable(L, -3);
		// playback speed
		lua_pushstring(L, "playbackSpeed");
		lua_pushnumber(L, m_playOptions.playbackSpeed);
		lua_settable(L, -3);
		// bpm
		lua_pushstring(L, "bpm");
		lua_pushnumber(L, m_currentTiming->GetBPM());
		lua_settable(L, -3);
		// gauge
		{
			Gauge* gauge = m_scoring.GetTopGauge();
			lua_getfield(L, -1, "gauge");


			lua_pushstring(L, "type");
			lua_pushinteger(L, (uint32)gauge->GetType());
			lua_settable(L, -3);

			lua_pushstring(L, "options");
			lua_pushinteger(L, gauge->GetOpts());
			lua_settable(L, -3);

			lua_pushstring(L, "value");
			lua_pushnumber(L, gauge->GetValue());
			lua_settable(L, -3);

			lua_pushstring(L, "name");
			lua_pushstring(L, gauge->GetName());
			lua_settable(L, -3);

			lua_pop(L, 1);
		}
		// combo state
		lua_pushstring(L, "comboState");
		lua_pushnumber(L, m_scoring.comboState);
		lua_settable(L, -3);

		// hidden/sudden
		lua_pushstring(L, "hiddenFade");
		lua_pushnumber(L, m_track->hiddenFadewindow);
		lua_settable(L, -3);

		lua_pushstring(L, "hiddenCutoff");
		lua_pushnumber(L, m_track->hiddenCutoff);
		lua_settable(L, -3);

		lua_pushstring(L, "suddenFade");
		lua_pushnumber(L, m_track->suddenFadewindow);
		lua_settable(L, -3);

		lua_pushstring(L, "suddenCutoff");
		lua_pushnumber(L, m_track->suddenCutoff);
		lua_settable(L, -3);

		// critLine
		// When the game's paused, the critline's coordinates are messed up.
		if(!m_paused)
		{
			lua_getfield(L, -1, "critLine");

			Vector2 critPos = m_camera.Project(m_camera.critOrigin.TransformPoint(Vector3(0, 0, 0)));
			Vector2 leftPos = m_camera.Project(m_camera.critOrigin.TransformPoint(Vector3(-m_track->trackWidth / 2.0, 0, 0)));
			Vector2 rightPos = m_camera.Project(m_camera.critOrigin.TransformPoint(Vector3(m_track->trackWidth / 2.0, 0, 0)));
			Vector2 line = rightPos - leftPos;

			lua_pushstring(L, "x"); // x screen position
			lua_pushnumber(L, critPos.x);
			lua_settable(L, -3);

			lua_pushstring(L, "y"); // y screen position
			lua_pushnumber(L, critPos.y);
			lua_settable(L, -3);

			lua_pushstring(L, "rotation"); // rotation based on laser roll
			lua_pushnumber(L, -atan2f(line.y, line.x));
			lua_settable(L, -3);

			lua_pushstring(L, "xOffset");
			lua_pushnumber(L, -m_camera.GetCritLineRoll() * 360);
			lua_settable(L, -3);

			//track x critline corners
			lua_getfield(L, -1, "line");
			{
				lua_pushstring(L, "x1");
				lua_pushnumber(L, leftPos.x);
				lua_settable(L, -3);
				lua_pushstring(L, "y1");
				lua_pushnumber(L, leftPos.y);
				lua_settable(L, -3);

				lua_pushstring(L, "x2");
				lua_pushnumber(L, rightPos.x);
				lua_settable(L, -3);
				lua_pushstring(L, "y2");
				lua_pushnumber(L, rightPos.y);
				lua_settable(L, -3);
			}
			lua_pop(L, 1);

			auto setCursorData = [&](int ci)
			{
				lua_geti(L, -1, ci);

#define TPOINT(name, y) Vector2 name = m_camera.Project(m_camera.critOrigin.TransformPoint(Vector3((m_scoring.laserPositions[ci] - Track::trackWidth * 0.5f) * (5.0f / 6), y, 0)))
				TPOINT(cPos, 0);
				TPOINT(cPosUp, 1);
				TPOINT(cPosDown, -1);
#undef TPOINT

				Vector2 cursorAngleVector = cPosUp - cPosDown;
				float distFromCritCenter = (critPos - cPos).Length() * (m_scoring.laserPositions[ci] < 0.5 ? -1 : 1);

				float skewAngle = -atan2f(cursorAngleVector.y, cursorAngleVector.x) + 3.1415 / 2;
				float alpha = (1.0f - Math::Clamp<float>(m_scoring.timeSinceLaserUsed[ci] / 0.5f - 1.0f, 0, 1));

				lua_pushstring(L, "pos");
				lua_pushnumber(L, distFromCritCenter * (m_scoring.lasersAreExtend[ci] ? 2 : 1));
				lua_settable(L, -3);

				lua_pushstring(L, "alpha");
				lua_pushnumber(L, alpha);
				lua_settable(L, -3);

				lua_pushstring(L, "skew");
				lua_pushnumber(L, skewAngle);
				lua_settable(L, -3);

				lua_pop(L, 1);
			};

			lua_getfield(L, -1, "cursors");
			setCursorData(0);
			setCursorData(1);

			lua_pop(L, 2); // cursors, critLine
		}

		lua_setglobal(L, "gameplay");
	}
	void SetInitialGameplayLua(lua_State* L) override
	{
		auto pushStringToTable = [&](const char* name, String data)
		{
			lua_pushstring(L, name);
			lua_pushstring(L, data.c_str());
			lua_settable(L, -3);
		};
		auto pushIntToTable = [&](const char* name, int data)
		{
			lua_pushstring(L, name);
			lua_pushinteger(L, data);
			lua_settable(L, -3);
		};
		auto pushFloatToTable = [&](const char* name, float data)
		{
			lua_pushstring(L, name);
			lua_pushnumber(L, data);
			lua_settable(L, -3);
		};

		auto mapSettings = GetBeatmap()->GetMapSettings();
		lua_newtable(L);
		String jacketPath = m_chartRootPath + "/" + mapSettings.jacketPath;
		pushStringToTable("jacketPath", jacketPath);
		pushStringToTable("title", mapSettings.title);
		pushStringToTable("artist", mapSettings.artist);

		lua_pushstring(L, "demoMode");
		lua_pushboolean(L, m_demo);
		lua_settable(L, -3);

		pushIntToTable("difficulty", mapSettings.difficulty);
		pushIntToTable("level", mapSettings.level);

		//gauge table
		{
			PlaybackOptions opts = GetPlaybackOptions();

			lua_pushstring(L, "gauge");
			lua_newtable(L);
			pushIntToTable("type", (uint32)opts.gaugeType);
			pushIntToTable("options", opts.gaugeOption);
			pushFloatToTable("value", 0.0f);
			pushStringToTable("name", "");
			lua_settable(L, -3);

		}


		lua_pushstring(L, "scoreReplays");
		lua_newtable(L);
		lua_settable(L, -3);
		lua_pushstring(L, "critLine");
		lua_newtable(L);
		lua_pushstring(L, "cursors");
		lua_newtable(L);
		{
			lua_newtable(L);
			lua_seti(L, -2, 0);

			lua_newtable(L);
			lua_seti(L, -2, 1);
		}
		lua_settable(L, -3); // cursors -> critLine
		lua_pushstring(L, "line");
		lua_newtable(L);
		lua_settable(L, -3); // line -> critLine
		lua_settable(L, -3); // critLine -> gameplay

		lua_pushstring(L, "multiplayer");
		lua_pushboolean(L, m_multiplayer != nullptr);
		lua_settable(L, -3);

		lua_pushstring(L, "hitWindow");
		GetHitWindow().ToLuaTable(L);
		lua_settable(L, -3);

		if (m_multiplayer != nullptr)
		{
			pushStringToTable("user_id", m_multiplayer->GetUserId());
			Log("[Multiplayer] Started game in multiplayer mode!", Logger::Severity::Info);
		}

		lua_pushstring(L, "autoplay");
		lua_pushboolean(L, m_scoring.autoplayInfo.autoplay);
		lua_settable(L, -3);

		if (m_isPracticeMode)
		{
			// Existence of this field implies that the game's in the practice mode.
			lua_pushstring(L, "practice_setup");
			lua_pushboolean(L, m_isPracticeSetup);
			lua_settable(L, -3);
		}

		//set hidden/sudden
		pushFloatToTable("hiddenFade", m_track->hiddenFadewindow);
		pushFloatToTable("hiddenCutoff", m_track->hiddenCutoff);
		pushFloatToTable("suddenFade", m_track->suddenFadewindow);
		pushFloatToTable("suddenCutoff", m_track->suddenCutoff);
		m_setLuaHolds(L);
		lua_setglobal(L, "gameplay");
	}
};

Game* Game::Create(ChartIndex* chart, PlayOptions&& options)
{
	Game_Impl* impl = new Game_Impl(chart, std::move(options));
	return impl;
}

Game* Game::Create(MultiplayerScreen* multiplayer, ChartIndex* chart, PlayOptions&& options)
{
	Game_Impl* impl = new Game_Impl(chart, std::move(options));
	impl->MakeMultiplayer(multiplayer);
	return impl;
}

Game* Game::Create(ChallengeManager* challenge, ChartIndex* chart, PlayOptions&& options)
{
	Game_Impl* impl = new Game_Impl(chart, std::move(options));
	impl->MakeChallenge(challenge);
	return impl;
}

Game* Game::Create(const String& mapPath, PlayOptions&& options)
{
	Game_Impl* impl = new Game_Impl(mapPath, std::move(options));
	return impl;
}

Game* Game::CreatePractice(ChartIndex* chart, PlayOptions&& options)
{
	Game_Impl* impl = new Game_Impl(chart, std::move(options));
	impl->MakePracticeSetup();
	return impl;
}

GameFlags operator|(const GameFlags & a, const GameFlags & b)
{
	return (GameFlags)((uint32)a | (uint32)b);

}

GameFlags operator&(const GameFlags & a, const GameFlags & b)
{
	return (GameFlags)((uint32)a & (uint32)b);
}

GameFlags operator~(const GameFlags & a)
{
	return (GameFlags)(~(uint32)a);
}

PlaybackOptions Game::PlaybackOptionsFromSettings()
{
	PlaybackOptions options;
	GaugeTypes gaugeType = g_gameConfig.GetEnum<Enum_GaugeTypes>(GameConfigKeys::GaugeType);
	if (gaugeType == GaugeTypes::Hard)
		options.gaugeType = GaugeType::Hard;
	options.mirror = g_gameConfig.GetBool(GameConfigKeys::MirrorChart);
	options.random = g_gameConfig.GetBool(GameConfigKeys::RandomizeChart);
	options.backupGauge = g_gameConfig.GetBool(GameConfigKeys::BackupGauge);

	return options;
}
