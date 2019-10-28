#include "stdafx.h"
#include "Game.hpp"
#include "Application.hpp"
#include <array>
#include <random>
#include <unordered_set>
#include <Beatmap/BeatmapPlayback.hpp>
#include <Beatmap/MapDatabase.hpp>
#include <Shared/Profiling.hpp>
#include "Scoring.hpp"
#include <Audio/Audio.hpp>
#include "Track.hpp"
#include "Camera.hpp"
#include "Background.hpp"
#include "AudioPlayback.hpp"
#include "Input.hpp"
#include "SongSelect.hpp"
#include "ScoreScreen.hpp"
#include "TransitionScreen.hpp"
#include "AsyncAssetLoader.hpp"
#include "MultiplayerScreen.hpp"
#include "GameConfig.hpp"
#include <Shared/Time.hpp>
#include "SDL2/SDL_keycode.h"
#include "json.hpp"

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
}

#include "GUI/HealthGauge.hpp"

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
	String m_mapRootPath;
	String m_mapPath;
	DifficultyIndex m_diffIndex;

private:
	bool m_playing = true;
	bool m_started = false;
	bool m_demo = false;
	bool m_introCompleted = false;
	bool m_outroCompleted = false;
	bool m_paused = false;
	bool m_ended = false;
	bool m_transitioning = false;

	bool m_renderDebugHUD = false;

	MultiplayerScreen* m_multiplayer = nullptr;

	// Map object approach speed, scaled by BPM
	float m_hispeed = 1.0f;

	// Current lane toggle status
	bool m_hideLane = false;

    // Use m-mod and what m-mod speed
	SpeedMods m_speedMod;
    float m_modSpeed = 400;

	// Game Canvas
	Ref<HealthGauge> m_scoringGauge;
	//Ref<SettingsBar> m_settingsBar;
	//Ref<Label> m_scoreText;

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
	int32 m_audioOffset = 0;
	int32 m_fpsTarget = 0;
	// The play field
	Track* m_track = nullptr;

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
	MapTime m_lastMapTime;

	// Rate to sample gauge;
	MapTime m_gaugeSampleRate;
	float m_gaugeSamples[256] = { 0.0f };
	MapTime m_endTime;

	// Combo gain animation
	Timer m_comboAnimation;

	Sample m_slamSample;
	Sample m_clickSamples[2];
	Sample* m_fxSamples;

	// Roll intensity, default = 1
	float m_rollIntensity = 14 / 360.0;
	bool m_manualTiltEnabled = false;

	// Particle effects
	Material particleMaterial;
	Texture basicParticleTexture;
	Texture squareParticleTexture;
	ParticleSystem m_particleSystem;
	Ref<ParticleEmitter> m_laserFollowEmitters[2];
	Ref<ParticleEmitter> m_holdEmitters[6];
	GameFlags m_flags;
	bool m_manualExit = false;
	bool m_showCover = true;

	float m_shakeAmount = 3;
	float m_shakeDuration = 0.083;

	Map<ScoreIndex*, ScoreReplay> m_scoreReplays;
	MapDatabase* m_db;
	std::unordered_set<ObjectState*> m_hiddenObjects;

public:
	Game_Impl(const String& mapPath, GameFlags flags)
	{
		// Store path to map
		m_mapPath = Path::Normalize(mapPath);
		// Get Parent path
		m_mapRootPath = Path::RemoveLast(m_mapPath, nullptr);
		m_flags = flags;
		m_diffIndex.id = -1;
		m_diffIndex.mapId = -1;

		m_hispeed = g_gameConfig.GetFloat(GameConfigKeys::HiSpeed);
		m_speedMod = g_gameConfig.GetEnum<Enum_SpeedMods>(GameConfigKeys::SpeedMod);
		m_modSpeed = g_gameConfig.GetFloat(GameConfigKeys::ModSpeed);
	}

	Game_Impl(const DifficultyIndex& difficulty, GameFlags flags)
	{
		// Store path to map
		m_mapPath = Path::Normalize(difficulty.path);
		m_diffIndex = difficulty;
		m_flags = flags;
		// Get Parent path
		m_mapRootPath = Path::RemoveLast(m_mapPath, nullptr);

		m_hispeed = g_gameConfig.GetFloat(GameConfigKeys::HiSpeed);
		m_speedMod = g_gameConfig.GetEnum<Enum_SpeedMods>(GameConfigKeys::SpeedMod);
        m_modSpeed = g_gameConfig.GetFloat(GameConfigKeys::ModSpeed);
	}
	~Game_Impl()
	{
		if(m_track)
			delete m_track;
		if(m_background)
			delete m_background;
		if (m_foreground)
			delete m_foreground;
		if (m_lua)
		{
			g_application->DisposeLua(m_lua);
			// Clear the state we stored in in the multiplayer's socket
			if (m_multiplayer != nullptr)
				m_multiplayer->GetTCP().ClearState(m_lua);
		}
		// Save hispeed
		g_gameConfig.Set(GameConfigKeys::HiSpeed, m_hispeed);

		//g_rootCanvas->Remove(m_canvas.As<GUIElementBase>()); 

		// In case the cursor was still hidden
		g_gameWindow->SetCursorVisible(true); 
		g_input.OnButtonPressed.RemoveAll(this);
	}

	AsyncAssetLoader loader;
	virtual bool AsyncLoad() override
	{
		ProfilerScope $("AsyncLoad Game");

		if(!Path::FileExists(m_mapPath))
		{
			Logf("Couldn't find map at %s", Logger::Error, m_mapPath);
			return false;
		}

		m_beatmap = TryLoadMap(m_mapPath);

		// Check failure of above loading attempts
		if(!m_beatmap)
		{
			Log("Failed to load map", Logger::Warning);
			return false;
		}

		// Enable debug functionality
		if(g_application->GetAppCommandLine().Contains("-debug"))
		{
			m_renderDebugHUD = true;
		}

		const BeatmapSettings& mapSettings = m_beatmap->GetMapSettings();

		MapTime firstObjectTime = m_beatmap->GetLinearObjects().front()->time;
		ObjectState *const* lastObj = &m_beatmap->GetLinearObjects().back();
		while ((*lastObj)->type == ObjectType::Event && lastObj != &m_beatmap->GetLinearObjects().front())
		{
			lastObj--;
		}

		MapTime lastObjectTime = (*lastObj)->time;
		if ((*lastObj)->type == ObjectType::Hold)
		{
			HoldObjectState* lastHold = (HoldObjectState*)(*lastObj);
			lastObjectTime += lastHold->duration;
		}
		else if ((*lastObj)->type == ObjectType::Laser)
		{
			LaserObjectState* lastHold = (LaserObjectState*)(*lastObj);
			lastObjectTime += lastHold->duration;
		}
		
		m_endTime = lastObjectTime;
		m_gaugeSampleRate = lastObjectTime / 256;

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
            bpmDurations[lastBPM] += lastObjectTime - lastMT;

            if (bpmDurations[lastBPM] > largestMT)
            {
                useBPM = lastBPM;
            }

            m_hispeed = m_modSpeed / useBPM; 
        }
		else if (m_speedMod == SpeedMods::CMod)
		{
			m_hispeed = m_modSpeed / m_beatmap->GetLinearTimingPoints().front()->GetBPM();
		}

		// Initialize input/scoring
		if(!InitGameplay())
			return false;

		// Load beatmap audio
		if(!m_audioPlayback.Init(m_playback, m_mapRootPath))
			return false;

		// Get fps limit
		m_fpsTarget = g_gameConfig.GetInt(GameConfigKeys::FPSTarget);

		ApplyAudioLeadin();

		// Load audio offset
		m_audioOffset = g_gameConfig.GetInt(GameConfigKeys::GlobalOffset);
		m_playback.audioOffset = m_audioOffset;


		/// TODO: Check if debugmute is enabled
		g_audio->SetGlobalVolume(g_gameConfig.GetFloat(GameConfigKeys::MasterVolume));

		if(!InitSFX())
			return false;

		// Intialize track graphics
		m_track = new Track();
		loader.AddLoadable(*m_track, "Track");

		// Load particle textures
		loader.AddTexture(basicParticleTexture, "particle_flare.png");
		loader.AddTexture(squareParticleTexture, "particle_square.png");
		loader.AddMaterial(particleMaterial, "particle");

		if(!InitHUD())
			return false;

		if(!loader.Load())
			return false;



		// Load particle material
		m_particleSystem = ParticleSystemRes::Create(g_gl);


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

		m_track->suddenCutoff = g_gameConfig.GetFloat(GameConfigKeys::SuddenCutoff);
		m_track->suddenFadewindow = g_gameConfig.GetFloat(GameConfigKeys::SuddenFade);
		m_track->hiddenCutoff = g_gameConfig.GetFloat(GameConfigKeys::HiddenCutoff);
		m_track->hiddenFadewindow = g_gameConfig.GetFloat(GameConfigKeys::HiddenFade);
		m_showCover = g_gameConfig.GetBool(GameConfigKeys::ShowCover);

		auto pushStringToTable = [&](const char* name, String data)
		{
			lua_pushstring(m_lua, name);
			lua_pushstring(m_lua, data.c_str());
			lua_settable(m_lua, -3);
		};
		auto pushIntToTable = [&](const char* name, int data)
		{
			lua_pushstring(m_lua, name);
			lua_pushinteger(m_lua, data);
			lua_settable(m_lua, -3);
		};
		auto pushFloatToTable = [&](const char* name, float data)
		{
			lua_pushstring(m_lua, name);
			lua_pushnumber(m_lua, data);
			lua_settable(m_lua, -3);
		};

		const BeatmapSettings& mapSettings = m_beatmap->GetMapSettings();
		int64 startTime = Shared::Time::Now().Data();
		///TODO: Set more accurate endTime
		int64 endTime = startTime + (m_endTime / 1000) + 5;
		g_application->DiscordPresenceSong(mapSettings, startTime, endTime);

		String jacketPath = m_mapRootPath + "/" + mapSettings.jacketPath;
		//Set gameplay table
		{
			lua_newtable(m_lua);
			pushStringToTable("jacketPath", jacketPath);
			pushStringToTable("title", mapSettings.title);
			pushStringToTable("artist", mapSettings.artist);

			lua_pushstring(m_lua, "demoMode");
			lua_pushboolean(m_lua, m_demo);
			lua_settable(m_lua, -3);

			pushIntToTable("difficulty", mapSettings.difficulty);
			pushIntToTable("level", mapSettings.level);
			pushIntToTable("gaugeType", (m_flags & GameFlags::Hard) != GameFlags::None ? 1 : 0);
			lua_pushstring(m_lua, "scoreReplays");
			lua_newtable(m_lua);
			lua_settable(m_lua, -3);


			lua_pushstring(m_lua, "critLine");
			lua_newtable(m_lua);
			lua_pushstring(m_lua, "cursors");
			lua_newtable(m_lua);
			{
				lua_newtable(m_lua);
				lua_seti(m_lua, -2, 0);

				lua_newtable(m_lua);
				lua_seti(m_lua, -2, 1);
			}
			lua_settable(m_lua, -3); // cursors -> critLine

			lua_pushstring(m_lua, "line");
			lua_newtable(m_lua);
			lua_settable(m_lua, -3); // line -> critLine
			lua_settable(m_lua, -3); // critLine -> gameplay

			lua_pushstring(m_lua, "multiplayer");
			lua_pushboolean(m_lua, m_multiplayer != nullptr);
			lua_settable(m_lua, -3);

			if (m_multiplayer != nullptr)
			{
				pushStringToTable("user_id", m_multiplayer->GetUserId());
				Log("[Multiplayer] Started game in multiplayer mode!", Logger::Info);
			}

			lua_pushstring(m_lua, "autoplay");
			lua_pushboolean(m_lua, m_scoring.autoplay);
			lua_settable(m_lua, -3);

			//set hidden/sudden
			pushFloatToTable("hiddenFade", m_track->hiddenFadewindow);
			pushFloatToTable("hiddenCutoff", m_track->hiddenCutoff);
			pushFloatToTable("suddenFade", m_track->suddenFadewindow);
			pushFloatToTable("suddenCutoff", m_track->suddenCutoff);
			m_setLuaHolds();
			lua_setglobal(m_lua, "gameplay");
		}

		// For multiplayer we also bind the TCP in
		if (m_multiplayer != nullptr)
			m_multiplayer->GetTCP().PushFunctions(m_lua);

		// Background 
		/// TODO: Load this async
		CheckedLoad(m_background = CreateBackground(this));
		CheckedLoad(m_foreground = CreateBackground(this, true));
		g_application->LoadGauge((m_flags & GameFlags::Hard) != GameFlags::None);

		particleMaterial->blendMode = MaterialBlendMode::Additive;
		particleMaterial->opaque = false;
		// Do this here so we don't get input events while still loading
		m_scoring.SetFlags(m_flags);
		m_scoring.SetPlayback(m_playback);
		m_scoring.SetEndTime(m_endTime);
		m_scoring.SetInput(&g_input);
		m_scoring.Reset(); // Initialize

		g_input.OnButtonPressed.Add(this, &Game_Impl::m_OnButtonPressed);

		if ((m_flags & GameFlags::Random) != GameFlags::None)
		{
			//Randomize
			std::array<int,4> swaps = { 0,1,2,3 };
			
			std::shuffle(swaps.begin(), swaps.end(), std::default_random_engine((int)(1000 * g_application->GetAppTime())));

			bool unchanged = true;
			for (size_t i = 0; i < 4; i++)
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

		if ((m_flags & GameFlags::Mirror) != GameFlags::None)
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


		return true;
	}
	virtual bool Init() override
	{
		return true;
	}

	// Restart map
	virtual void Restart()
	{
		m_camera = Camera();

		//bool audioReinit = m_audioPlayback.Init(m_playback, m_mapRootPath);
		//assert(audioReinit);

		// Audio leadin
		m_audioPlayback.SetEffectEnabled(0, false);
		m_audioPlayback.SetEffectEnabled(1, false);
		ApplyAudioLeadin();

		m_paused = false;
		m_started = false;
		m_ended = false;
		m_hideLane = false;
		m_transitioning = false;
		m_playback.Reset(m_lastMapTime);
		m_scoring.Reset();
		m_scoring.SetInput(&g_input);
		m_camera.pLaneZoom = m_playback.GetZoom(0);
		m_camera.pLanePitch = m_playback.GetZoom(1);
		m_camera.pLaneOffset = m_playback.GetZoom(2);
		m_camera.pLaneTilt = m_playback.GetZoom(3);

		for(uint32 i = 0; i < 2; i++)
		{
			if(m_laserFollowEmitters[i])
			{
				m_laserFollowEmitters[i].Release();
			}
		}
		for(uint32 i = 0; i < 6; i++)
		{
			if(m_holdEmitters[i])
			{
				m_holdEmitters[i].Release();
			}
		}

		for (ScoreIndex* score : m_diffIndex.scores)
		{
			m_scoreReplays[score] = ScoreReplay();
		}

		m_track->ClearEffects();
		m_particleSystem->Reset();
		m_audioPlayback.SetPlaybackSpeed(1.0f);

		//unhide notes
		m_hiddenObjects.clear();

	}
	virtual void Tick(float deltaTime) override
	{
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
					m_lockMouse.Release();
				g_gameWindow->SetCursorVisible(true);
			}
		}

		if(!m_paused)
			TickGameplay(deltaTime);


		// Update hispeed or hidden range
		if (g_input.GetButton(Input::Button::BT_S))
		{
			if (g_input.GetButton(Input::Button::BT_1))
			{
				float change = g_input.GetInputLaserDir(0) / 10.0f;
				m_track->hiddenCutoff = Math::Clamp(m_track->hiddenCutoff + change, 0.f, 1.f);

				change = g_input.GetInputLaserDir(1) / 10.0f;
				m_track->suddenCutoff = Math::Clamp(m_track->suddenCutoff + change, 0.f, 1.f);

				g_gameConfig.Set(GameConfigKeys::HiddenCutoff, m_track->hiddenCutoff);
				g_gameConfig.Set(GameConfigKeys::SuddenCutoff, m_track->suddenCutoff);
			}
			else if (g_input.GetButton(Input::Button::BT_2))
			{
				float change = g_input.GetInputLaserDir(0) / 30.0f;
				m_track->hiddenFadewindow = Math::Clamp(m_track->hiddenFadewindow + change, 0.f, 1.f);

				change = g_input.GetInputLaserDir(1) / 30.0f;
				m_track->suddenFadewindow = Math::Clamp(m_track->suddenFadewindow + change, 0.f, 1.f);

				g_gameConfig.Set(GameConfigKeys::HiddenFade, m_track->hiddenFadewindow);
				g_gameConfig.Set(GameConfigKeys::SuddenFade, m_track->suddenFadewindow);
			}
			else
			{
				for (int i = 0; i < 2; i++)
				{
					float change = g_input.GetInputLaserDir(i) / 3.0f;
					m_hispeed += change;
					m_hispeed = Math::Clamp(m_hispeed, 0.1f, 16.f);
					if ((m_speedMod != SpeedMods::XMod) && change != 0.0f)
					{
						g_gameConfig.Set(GameConfigKeys::ModSpeed, m_hispeed * (float)m_currentTiming->GetBPM());
						m_modSpeed = m_hispeed * (float)m_currentTiming->GetBPM();
						m_playback.cModSpeed = m_modSpeed;
					}
				}
			}
		}

	}
	virtual void Render(float deltaTime) override
	{
		// 8 beats (2 measures) in view at 1x hi-speed
		if (m_speedMod == SpeedMods::CMod)
			m_track->SetViewRange(1.0 / m_playback.cModSpeed);
		else
			m_track->SetViewRange(8.0f / (m_hispeed)); 


		// Get render state from the camera
		float rollA = m_scoring.GetLaserRollOutput(0);
		float rollB = m_scoring.GetLaserRollOutput(1);
		m_camera.SetTargetRoll(rollA + rollB);
		m_camera.SetRollIntensity(m_rollIntensity);

		// Set track zoom

		m_camera.pLaneZoom = m_playback.GetZoom(0);
		m_camera.pLanePitch = m_playback.GetZoom(1);
		m_camera.pLaneOffset = m_playback.GetZoom(2);
		m_camera.pLaneTilt = m_playback.GetZoom(3);
		m_camera.pManualTiltEnabled = m_manualTiltEnabled;
		m_camera.track = m_track;
		m_camera.Tick(deltaTime,m_playback);
		m_track->Tick(m_playback, deltaTime);
		RenderState rs = m_camera.CreateRenderState(true);

		// Draw BG first
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
				else if (a->type == ObjectType::Hold)
					return (((ButtonObjectState*)a)->index < 4) ? 3 : 4;
				else
					return 0;
			};
			uint32 renderPriorityA = ObjectRenderPriorty(a);
			uint32 renderPriorityB = ObjectRenderPriorty(b);
			return renderPriorityA > renderPriorityB;
		});

		/// TODO: Performance impact analysis.
		m_track->DrawLaserBase(renderQueue, m_playback, m_currentObjectSet);

		// Draw the base track + time division ticks
		m_track->DrawBase(renderQueue);

		for(auto& object : m_currentObjectSet)
		{
			if(m_hiddenObjects.find(object) == m_hiddenObjects.end())
				m_track->DrawObjectState(renderQueue, m_playback, object, m_scoring.IsObjectHeld(object));
		}
		if(m_showCover)
			m_track->DrawTrackCover(renderQueue);

		// Use new camera for scoring overlay
		//	this is because otherwise some of the scoring elements would get clipped to
		//	the track's near and far planes
		rs = m_camera.CreateRenderState(false);
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
		m_track->DrawOverlays(scoringRq);
		float comboZoom = Math::Max(0.0f, (1.0f - (m_comboAnimation.SecondsAsFloat() / 0.2f)) * 0.5f);
		//m_track->DrawCombo(scoringRq, m_scoring.currentComboCounter, m_comboColors[m_scoring.comboState], 1.0f + comboZoom);

		// Render queues
		renderQueue.Process();
		scoringRq.Process();

		// Set laser follow particle visiblity
		for(uint32 i = 0; i < 2; i++)
		{
			if(m_scoring.IsLaserHeld(i))
			{
				if(!m_laserFollowEmitters[i])
					m_laserFollowEmitters[i] = CreateTrailEmitter(m_track->laserColors[i]);

				// Set particle position to follow laser
				float followPos = m_scoring.laserTargetPositions[i];
				if (m_scoring.lasersAreExtend[i])
					followPos = followPos * 2.0f - 0.5f; 

				m_laserFollowEmitters[i]->position = m_track->TransformPoint(Vector3(m_track->trackWidth * followPos - m_track->trackWidth * 0.5f, 0.f, 0.f));
			}
			else
			{
				if(m_laserFollowEmitters[i])
				{
					m_laserFollowEmitters[i].Release();
				}
			}
		}

		// Set hold button particle visibility
		for(uint32 i = 0; i < 6; i++)
		{
			if(m_scoring.IsObjectHeld(i))
			{
				if(!m_holdEmitters[i])
				{
					Color hitColor = (i < 4) ? Color::White : Color::FromHSV(20, 0.7f, 1.0f);
					float hitWidth = (i < 4) ? m_track->buttonWidth : m_track->fxbuttonWidth;
					m_holdEmitters[i] = CreateHoldEmitter(hitColor, hitWidth);
				}
				m_holdEmitters[i]->position = m_track->TransformPoint(Vector3(m_track->GetButtonPlacement(i), 0.f, 0.f));
			}
			else
			{
				if(m_holdEmitters[i])
				{
					m_holdEmitters[i].Release();
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
			Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1)); \
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
			Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
			assert(false);
		}
		// flush NVG
		NVG_FLUSH();

		// Render particle effects last
		RenderParticles(rs, deltaTime);
		glFlush();

		// Render Critical Line Overlay
		lua_getglobal(m_lua, "render_crit_overlay");
		lua_pushnumber(m_lua, deltaTime);
		// only flush if the overlay exists. overlay isn't required, only one crit function is required.
		if (lua_pcall(m_lua, 1, 0, 0) == 0)
			NVG_FLUSH();

		// Render foreground
		m_foreground->Render(deltaTime);

		// Render Lua HUD
		lua_getglobal(m_lua, "render");
		lua_pushnumber(m_lua, deltaTime);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
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
					Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
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
				lua_pushnumber(m_lua, m_getClearState());
				if (lua_pcall(m_lua, 2, 2, 0) != 0)
				{
					Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
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
	}

	// Initialize HUD elements/layout
	bool InitHUD()
	{
		String skin = g_gameConfig.GetString(GameConfigKeys::Skin);
		return true;
	}

	// Wait before start of map
	void ApplyAudioLeadin()
	{
		// Select the correct first object to set the intial playback position
		// if it starts before a certain time frame, the song starts at a negative time (lead-in)
		ObjectState *const* firstObj = &m_beatmap->GetLinearObjects().front();
		while((*firstObj)->type == ObjectType::Event && firstObj != &m_beatmap->GetLinearObjects().back())
		{
			firstObj++;
		}
		m_lastMapTime = 0;
		MapTime firstObjectTime = (*firstObj)->time;
		if(firstObjectTime < 3000)
		{
			// Set start time
			m_lastMapTime = firstObjectTime - 5000;
			m_audioPlayback.SetPosition(m_lastMapTime);
		}

		// Reset playback
		m_playback.Reset(m_lastMapTime);
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
		for (int i = 0; i < samples.size(); i++)
		{
			if (default_sfx.Contains(samples[i]))
			{
				m_fxSamples[i] = g_application->LoadSample(samples[i]);
			}
			else
			{
				m_fxSamples[i] = g_application->LoadSample(m_mapRootPath + "/" + samples[i], true);
			}
			if (!m_fxSamples[i])
			{
				Logf("Failed to load FX chip sample: \"%s\"", Logger::Warning, samples[i]);
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

        // If c-mod is used
		if (m_speedMod == SpeedMods::CMod)
		{
			m_playback.OnTimingPointChanged.Add(this, &Game_Impl::OnTimingPointChanged);
		}
		m_playback.cMod = m_speedMod == SpeedMods::CMod;
		m_playback.cModSpeed = m_hispeed * m_playback.GetCurrentTimingPoint().GetBPM();
		// Register input bindings
		m_scoring.OnButtonMiss.Add(this, &Game_Impl::OnButtonMiss);
		m_scoring.OnLaserSlamHit.Add(this, &Game_Impl::OnLaserSlamHit);
		m_scoring.OnButtonHit.Add(this, &Game_Impl::OnButtonHit);
		m_scoring.OnComboChanged.Add(this, &Game_Impl::OnComboChanged);
		m_scoring.OnObjectHold.Add(this, &Game_Impl::OnObjectHold);
		m_scoring.OnObjectReleased.Add(this, &Game_Impl::OnObjectReleased);
		m_scoring.OnScoreChanged.Add(this, &Game_Impl::OnScoreChanged);

		m_playback.hittableObjectEnter = Scoring::missHitTime;
		m_playback.hittableObjectLeave = Scoring::goodHitTime;

		if(g_application->GetAppCommandLine().Contains("-autobuttons"))
		{
			m_scoring.autoplayButtons = true;
		}

		return true;
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
			m_audioPlayback.Play();
			m_started = true;

			if(g_application->GetAppCommandLine().Contains("-autoskip"))
			{
				SkipIntro();
			}
		}

		const BeatmapSettings& beatmapSettings = m_beatmap->GetMapSettings();

		// Update beatmap playback
		MapTime playbackPositionMs = m_audioPlayback.GetPosition() - m_audioOffset;
		m_playback.Update(playbackPositionMs);

		MapTime delta = playbackPositionMs - m_lastMapTime;
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

		// If failed in multiplayer, stop giving rate, so its clear you failed
		if (m_multiplayer != nullptr && m_multiplayer->HasFailed()) {
			m_scoring.currentGauge = 0.0f;
		}
		// Stop playing if gauge is on hard and at 0%
		if ((m_flags & GameFlags::Hard) != GameFlags::None && m_scoring.currentGauge == 0.f)
		{
			// In multiplayer we don't stop, but we send the final score
			if (m_multiplayer == nullptr) {
				FinishGame();
			} else if (!m_multiplayer->HasFailed()) {
				m_multiplayer->Fail();
				//m_multiplayer->SendFinalScore(m_scoring, m_getClearState());

				m_flags = m_flags & ~GameFlags::Hard;
				m_scoring.SetFlags(m_flags);
			}
		}


		// Update scoring
		if (!m_ended)
		{
			m_scoring.Tick(deltaTime);
			// Update scoring gauge
			int32 gaugeSampleSlot = playbackPositionMs;
			gaugeSampleSlot /= m_gaugeSampleRate;
			gaugeSampleSlot = Math::Clamp(gaugeSampleSlot, (int32)0, (int32)255);
			m_gaugeSamples[gaugeSampleSlot] = m_scoring.currentGauge;
		}


		// Get the current timing point
		m_currentTiming = &m_playback.GetCurrentTimingPoint();



		// Update song info display
		ObjectState *const* lastObj = &m_beatmap->GetLinearObjects().back();

		if (m_multiplayer != nullptr)
			m_multiplayer->PerformScoreTick(m_scoring, m_lastMapTime);

		//set lua
		lua_getglobal(m_lua, "gameplay");

		m_setLuaHolds();

		//set autoplay here as it's not set during the creation of the gameplay
		lua_pushstring(m_lua, "autoplay");
		lua_pushboolean(m_lua, m_scoring.autoplay);
		lua_settable(m_lua, -3);

		// Update score replays
		lua_getfield(m_lua, -1, "scoreReplays");
		int replayCounter = 1;
		for (ScoreIndex* index : m_diffIndex.scores)
		{
			m_scoreReplays[index].maxScore = index->score;
			if (index->hitStats.size() > 0)
			{
				while (m_scoreReplays[index].nextHitStat < index->hitStats.size()
					&& index->hitStats[m_scoreReplays[index].nextHitStat].time < m_lastMapTime)
				{
					SimpleHitStat shs = index->hitStats[m_scoreReplays[index].nextHitStat];
					if (shs.rating < 3)
					{
						m_scoreReplays[index].currentScore += shs.rating;
					}
					m_scoreReplays[index].nextHitStat++;
				}
			}
			lua_pushnumber(m_lua, replayCounter);
			lua_newtable(m_lua);

			lua_pushstring(m_lua, "maxScore");
			lua_pushnumber(m_lua, index->score);
			lua_settable(m_lua, -3);

			lua_pushstring(m_lua, "currentScore");
			lua_pushnumber(m_lua, m_scoring.CalculateScore(m_scoreReplays[index].currentScore));
			lua_settable(m_lua, -3);

			lua_settable(m_lua, -3);
			replayCounter++;
		}
		lua_setfield(m_lua, -1, "scoreReplays");


		//progress
		lua_pushstring(m_lua, "progress");
		lua_pushnumber(m_lua, Math::Clamp((float)playbackPositionMs / m_endTime,0.f,1.f));
		lua_settable(m_lua, -3);
		//hispeed
		lua_pushstring(m_lua, "hispeed");
		lua_pushnumber(m_lua, m_hispeed);
		lua_settable(m_lua, -3);
		//bpm
		lua_pushstring(m_lua, "bpm");
		lua_pushnumber(m_lua, m_currentTiming->GetBPM());
		lua_settable(m_lua, -3);
		//gauge
		lua_pushstring(m_lua, "gauge");
		lua_pushnumber(m_lua, m_scoring.currentGauge);
		lua_settable(m_lua, -3);
		//combo state
		lua_pushstring(m_lua, "comboState");
		lua_pushnumber(m_lua, m_scoring.comboState);
		lua_settable(m_lua, -3);

		//hidden/sudden
		lua_pushstring(m_lua, "hiddenFade");
		lua_pushnumber(m_lua, m_track->hiddenFadewindow);
		lua_settable(m_lua, -3);

		lua_pushstring(m_lua, "hiddenCutoff");
		lua_pushnumber(m_lua, m_track->hiddenCutoff);
		lua_settable(m_lua, -3);

		lua_pushstring(m_lua, "suddenFade");
		lua_pushnumber(m_lua, m_track->suddenFadewindow);
		lua_settable(m_lua, -3);

		lua_pushstring(m_lua, "suddenCutoff");
		lua_pushnumber(m_lua, m_track->suddenCutoff);
		lua_settable(m_lua, -3);



		//critLine
		{
			lua_getfield(m_lua, -1, "critLine");

			Vector2 critPos = m_camera.Project(m_camera.critOrigin.TransformPoint(Vector3(0, 0, 0)));
			Vector2 leftPos = m_camera.Project(m_camera.critOrigin.TransformPoint(Vector3(-m_track->trackWidth / 2.0, 0, 0)));
			Vector2 rightPos = m_camera.Project(m_camera.critOrigin.TransformPoint(Vector3(m_track->trackWidth / 2.0, 0, 0)));
			Vector2 line = rightPos - leftPos;

			lua_pushstring(m_lua, "x"); // x screen position
			lua_pushnumber(m_lua, critPos.x);
			lua_settable(m_lua, -3);

			lua_pushstring(m_lua, "y"); // y screen position
			lua_pushnumber(m_lua, critPos.y);
			lua_settable(m_lua, -3);

			//track x critline corners
			lua_getfield(m_lua, -1, "line");
			{
				lua_pushstring(m_lua, "x1");
				lua_pushnumber(m_lua, leftPos.x);
				lua_settable(m_lua, -3);
				lua_pushstring(m_lua, "y1");
				lua_pushnumber(m_lua, leftPos.y);
				lua_settable(m_lua, -3);

				lua_pushstring(m_lua, "x2");
				lua_pushnumber(m_lua, rightPos.x);
				lua_settable(m_lua, -3);
				lua_pushstring(m_lua, "y2");
				lua_pushnumber(m_lua, rightPos.y);
				lua_settable(m_lua, -3);
			}
			lua_pop(m_lua, 1);

			lua_pushstring(m_lua, "rotation"); // rotation based on laser roll
			lua_pushnumber(m_lua, -atan2f(line.y, line.x));
			lua_settable(m_lua, -3);

			auto setCursorData = [&](int ci)
			{
				lua_geti(m_lua, -1, ci);

#define TPOINT(name, y) Vector2 name = m_camera.Project(m_camera.critOrigin.TransformPoint(Vector3((m_scoring.laserPositions[ci] - Track::trackWidth * 0.5f) * (5.0f / 6), y, 0)))
				TPOINT(cPos, 0);
				TPOINT(cPosUp, 1);
				TPOINT(cPosDown, -1);
#undef TPOINT

				Vector2 cursorAngleVector = cPosUp - cPosDown;
				float distFromCritCenter = (critPos - cPos).Length() * (m_scoring.laserPositions[ci] < 0.5 ? -1 : 1);

				float skewAngle = -atan2f(cursorAngleVector.y, cursorAngleVector.x) + 3.1415 / 2;
				float alpha = (1.0f - Math::Clamp<float>(m_scoring.timeSinceLaserUsed[ci] / 0.5f - 1.0f, 0, 1));

				lua_pushstring(m_lua, "pos");
				lua_pushnumber(m_lua, distFromCritCenter * (m_scoring.lasersAreExtend[ci] ? 2 : 1));
				lua_settable(m_lua, -3);

				lua_pushstring(m_lua, "alpha");
				lua_pushnumber(m_lua, alpha);
				lua_settable(m_lua, -3);

				lua_pushstring(m_lua, "skew");
				lua_pushnumber(m_lua, skewAngle);
				lua_settable(m_lua, -3);

				lua_pop(m_lua, 1);
			};

			lua_getfield(m_lua, -1, "cursors");
			setCursorData(0);
			setCursorData(1);

			lua_pop(m_lua, 2); // cursors, critLine
		}

		lua_setglobal(m_lua, "gameplay");

		m_lastMapTime = playbackPositionMs;
		
		if(m_audioPlayback.HasEnded())
		{
			FinishGame();
		}
		if (m_outroCompleted && !m_transitioning)
		{
			if ((m_manualExit && g_gameConfig.GetBool(GameConfigKeys::SkipScore)) || (m_manualExit && m_demo))
			{
				g_application->RemoveTickable(this);
			}
			else if (m_demo)
			{
				// Transition to another random track
				Game* game = nullptr;
				while (!game) // ensure a working game
				{
					DifficultyIndex* diff = m_db->GetRandomDiff();
					game = Game::Create(*diff, m_flags);
				}
				game->GetScoring().autoplay = true;
				game->SetDemoMode(true);
				game->SetSongDB(m_db);

				// Transition to game
				TransitionScreen* transition = TransitionScreen::Create(game);
				g_application->AddTickable(transition);
				m_transitioning = true;
				transition->OnLoadingComplete.Add(this, &Game_Impl::OnScoreScreenLoaded);
			}
			else
			{
				// Transition to score screen
				TransitionScreen* transition = TransitionScreen::Create(ScoreScreen::Create(this));
				transition->OnLoadingComplete.Add(this, &Game_Impl::OnScoreScreenLoaded);
				g_application->AddTickable(transition);
				m_transitioning = true;
			}
		}
	}

	// Called when game is finished and the score screen should show up
	void FinishGame()
	{
		if(m_ended)
			return;

		// Send the final scores to the server
		if (m_multiplayer)
			m_multiplayer->SendFinalScore(m_scoring,
				m_multiplayer->HasFailed()? 1 : m_getClearState());

		m_scoring.FinishGame();
		m_ended = true;
	}
	void OnScoreScreenLoaded(IAsyncLoadableApplicationTickable* tickable)
	{
		//if demo and tickable failed, try another diff
		if (!tickable && m_demo)
		{
			Game* game = nullptr;
			while (!game) // ensure a working game
			{
				DifficultyIndex* diff = m_db->GetRandomDiff();
				game = Game::Create(*diff, m_flags);
			}
			game->GetScoring().autoplay = true;
			game->SetDemoMode(true);
			game->SetSongDB(m_db);

			// Transition to game
			TransitionScreen* transition = TransitionScreen::Create(game);
			g_application->AddTickable(transition);
			m_transitioning = true;
			transition->OnLoadingComplete.Add(this, &Game_Impl::OnScoreScreenLoaded);
		}
		else
		{
			// Remove self
			g_application->RemoveTickable(this);
		}
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
		auto RenderText = [&](const String& text, const Vector2& pos, const Color& color = Color::White)
		{
			g_application->FastText(text, pos.x, pos.y, 12, 0);
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
		textPos.y += RenderText(Utility::Sprintf("Audio Offset: %d ms", g_audio->audioLatency), textPos).y;

		float currentBPM = (float)(60000.0 / tp.beatDuration);
		textPos.y += RenderText(Utility::Sprintf("BPM: %.1f", currentBPM), textPos).y;
		textPos.y += RenderText(Utility::Sprintf("Time Signature: %d/4", tp.numerator), textPos).y;
		textPos.y += RenderText(Utility::Sprintf("Laser Effect Mix: %f", m_audioPlayback.GetLaserEffectMix()), textPos).y;
		textPos.y += RenderText(Utility::Sprintf("Laser Filter Input: %f", m_scoring.GetLaserOutput()), textPos).y;

		textPos.y += RenderText(Utility::Sprintf("Score: %d (Max: %d)", m_scoring.currentHitScore, m_scoring.mapTotals.maxScore), textPos).y;
		
		textPos.y += RenderText(Utility::Sprintf("Actual Score: %d", m_scoring.CalculateCurrentScore()), textPos).y;

		textPos.y += RenderText(Utility::Sprintf("Health Gauge: %f", m_scoring.currentGauge), textPos).y;

		textPos.y += RenderText(Utility::Sprintf("Roll: %f(x%f) %s",
			m_camera.GetRoll(), m_rollIntensity, m_camera.rollKeep ? "[Keep]" : ""), textPos).y;

		textPos.y += RenderText(Utility::Sprintf("Track Zoom Top: %f", m_camera.pLanePitch), textPos).y;
		textPos.y += RenderText(Utility::Sprintf("Track Zoom Bottom: %f", m_camera.pLaneZoom), textPos).y;

		Vector2 buttonStateTextPos = Vector2(g_resolution.x - 200.0f, 100.0f);
		RenderText(g_input.GetControllerStateString(), buttonStateTextPos);

		if(m_scoring.autoplay)
			textPos.y += RenderText("Autoplay enabled", textPos, Color::Blue).y;

		// List recent hits and their delay
		Vector2 tableStart = textPos;
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
				text = Utility::Sprintf("[%d] %d", obj->button.index, (*it)->delta);
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

	void OnLaserSlamHit(LaserObjectState* object)
	{
		float slamSize = (object->points[1] - object->points[0]);
		float direction = Math::Sign(slamSize);
		slamSize = fabsf(slamSize);
		CameraShake shake(m_shakeDuration, powf(slamSize, 0.5f) * m_shakeAmount * -direction);
		m_camera.AddCameraShake(shake);
		m_slamSample->Play();


		if (object->spin.type != 0)
		{
			if (object->spin.type == SpinStruct::SpinType::Bounce)
				m_camera.SetXOffsetBounce(object->GetDirection(), object->spin.duration, object->spin.amplitude, object->spin.frequency, object->spin.duration, m_playback);
			else m_camera.SetSpin(object->GetDirection(), object->spin.duration, object->spin.type, m_playback);
		}


		float dir = Math::Sign(object->points[1] - object->points[0]);
		float laserPos = m_track->trackWidth * object->points[1] - m_track->trackWidth * 0.5f;
		Ref<ParticleEmitter> ex = CreateExplosionEmitter(m_track->laserColors[object->index], Vector3(dir, 0, 0));
		ex->position = Vector3(laserPos, 0.0f, -0.05f);
		ex->position = m_track->TransformPoint(ex->position);
	}
	void OnButtonHit(Input::Button button, ScoreHitRating rating, ObjectState* hitObject, MapTime delta)
	{
		ButtonObjectState* st = (ButtonObjectState*)hitObject;
		uint32 buttonIdx = (uint32)button;
		Color c = m_track->hitColors[(size_t)rating];

		// The color effect in the button lane
		m_track->AddEffect(new ButtonHitEffect(buttonIdx, c));

		if (st != nullptr && st->hasSample)
		{
			if (m_fxSamples[st->sampleIndex])
			{
				m_fxSamples[st->sampleIndex]->SetVolume(st->sampleVolume);
				m_fxSamples[st->sampleIndex]->Play();
			}
		}

		if(rating != ScoreHitRating::Idle)
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
					Logf("Lua error on calling near_hit: %s", Logger::Error, lua_tostring(m_lua, -1));
				}
			}

			//set button invisible
			if (st != nullptr)
				m_hiddenObjects.insert(hitObject);

			// Create hit effect particle
			Color hitColor = (buttonIdx < 4) ? Color::White : Color::FromHSV(20, 0.7f, 1.0f);
			float hitWidth = (buttonIdx < 4) ? m_track->buttonWidth : m_track->fxbuttonWidth;
			Ref<ParticleEmitter> emitter = CreateHitEmitter(hitColor, hitWidth);
			emitter->position.x = m_track->GetButtonPlacement(buttonIdx);
			emitter->position.z = -0.05f;
			emitter->position.y = 0.0f;
			emitter->position = m_track->TransformPoint(emitter->position);
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
				Logf("Lua error on calling button_hit: %s", Logger::Error, lua_tostring(m_lua, -1));
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
			m_track->AddEffect(new ButtonHitEffect(buttonIdx, c));
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
				Logf("Lua error on calling button_hit: %s", Logger::Error, lua_tostring(m_lua, -1));
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
			Logf("Lua error on calling update_combo: %s", Logger::Error, lua_tostring(m_lua, -1));
		}
	}
	void OnScoreChanged(uint32 newScore)
	{
		lua_getglobal(m_lua, "update_score");
		lua_pushinteger(m_lua, newScore);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on calling update_score: %s", Logger::Error, lua_tostring(m_lua, -1));
		}
	}

	// These functions control if FX button DSP's are muted or not
	void OnObjectHold(Input::Button, ObjectState* object)
	{
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
			m_camera.rollKeep = (data.rollVal & TrackRollBehaviour::Keep) == TrackRollBehaviour::Keep;
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
				//m_rollIntensity = m_rollIntensityBase + (float)(i - 1) * 0.0125f;
				m_rollIntensity = (14 * (1.0 + 0.5 * (i - 1))) / 360.0;
			}
		}
		else if(key == EventKey::SlamVolume)
		{
			m_slamSample->SetVolume(data.floatVal);
		}
		else if (key == EventKey::ChartEnd)
		{
			FinishGame();
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
				Logf("Lua error on calling laser_alert: %s", Logger::Error, lua_tostring(m_lua, -1));
			}
		}
	}

	virtual void OnKeyPressed(int32 key) override
	{
		if(key == SDLK_PAUSE && m_multiplayer == nullptr)
		{
			m_audioPlayback.TogglePause();
			m_paused = m_audioPlayback.IsPaused();
		}
		else if(key == SDLK_RETURN) // Skip intro
		{
			if(!SkipIntro())
				SkipOutro();
		}
		else if(key == SDLK_PAGEUP && m_multiplayer == nullptr)
		{
			m_audioPlayback.Advance(5000);
		}
		else if(key == SDLK_ESCAPE)
		{
			ObjectState *const* lastObj = &m_beatmap->GetLinearObjects().back();
			MapTime timePastEnd = m_lastMapTime - m_endTime;
			if (timePastEnd < 0)
				m_manualExit = true;
			FinishGame();
		}
		else if(key == SDLK_F5 && m_multiplayer == nullptr) // Restart map
		{
			// Restart
			Restart();
		}
		else if(key == SDLK_F8)
		{
			m_renderDebugHUD = !m_renderDebugHUD;
			//m_psi->visibility = m_renderDebugHUD ? Visibility::Collapsed : Visibility::Visible;
		}
		else if(key == SDLK_TAB)
		{
			//g_gameWindow->SetCursorVisible(!m_settingsBar->IsShown());
			//m_settingsBar->SetShow(!m_settingsBar->IsShown());
		}
		else if(key == SDLK_F9)
		{
			g_application->ReloadScript("gameplay", m_lua);
		}
	}
	void m_OnButtonPressed(Input::Button buttonCode)
	{
		if (buttonCode == Input::Button::BT_S)
		{
			if (g_input.Are3BTsHeld() && IsSuccessfullyInitialized())
			{
				ObjectState *const* lastObj = &m_beatmap->GetLinearObjects().back();
				MapTime timePastEnd = m_lastMapTime - (*lastObj)->time;
				if (timePastEnd < 0)
					m_manualExit = true;

				FinishGame();
			}
		}
	}
	int m_getClearState()
	{
		if (m_manualExit)
			return 0;
		ScoreIndex scoreData;
		scoreData.miss = m_scoring.categorizedHits[0];
		scoreData.almost = m_scoring.categorizedHits[1];
		scoreData.crit = m_scoring.categorizedHits[2];
		scoreData.gameflags = (uint32)m_flags;
		scoreData.gauge = m_scoring.currentGauge;
		scoreData.score = m_scoring.CalculateCurrentScore();
		return Scoring::CalculateBadge(scoreData);
	}

	void m_setLuaHolds()
	{
		//button
		lua_pushstring(m_lua, "noteHeld");
		lua_newtable(m_lua);
		for (size_t i = 0; i < 6; i++)
		{
			lua_pushnumber(m_lua, i + 1);
			lua_pushboolean(m_lua, m_scoring.IsObjectHeld(i));
			lua_settable(m_lua, -3);
		}
		lua_settable(m_lua, -3);

		//laser
		lua_pushstring(m_lua, "laserActive");
		lua_newtable(m_lua);
		for (size_t i = 0; i < 2; i++)
		{
			lua_pushnumber(m_lua, i + 1);
			lua_pushboolean(m_lua, m_scoring.IsObjectHeld(6 + i));
			lua_settable(m_lua, -3);
		}
		lua_settable(m_lua, -3);
	}

	// Skips ahead to the right before the first object in the map
	bool SkipIntro()
	{
		ObjectState *const* firstObj = &m_beatmap->GetLinearObjects().front();
		while((*firstObj)->type == ObjectType::Event && firstObj != &m_beatmap->GetLinearObjects().back())
		{
			firstObj++;
		}
		MapTime skipTime = (*firstObj)->time - 1000;
		if(skipTime > m_lastMapTime)
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
		if(m_beatmap->GetLinearObjects().empty())
		{
			FinishGame();
			return;
		}

		// Check if last object has passed
		ObjectState *const* lastObj = &m_beatmap->GetLinearObjects().back();
		MapTime timePastEnd = m_lastMapTime - (*lastObj)->time;
		if(timePastEnd > 250)
		{
			FinishGame();
		}
	}

	void MakeMultiplayer(MultiplayerScreen* multiplayer)
	{
		m_multiplayer = multiplayer;
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
	virtual float* GetGaugeSamples() override
	{
		return m_gaugeSamples;
	}
	virtual GameFlags GetFlags() override
	{
		return m_flags;
	}
	virtual bool GetManualExit() override
	{
		return m_manualExit;
	}
	virtual float GetPlaybackSpeed() override
	{
		return m_audioPlayback.GetPlaybackSpeed();
	}
	virtual const String& GetMapRootPath() const
	{
		return m_mapRootPath;
	}
	virtual const String& GetMapPath() const
	{
		return m_mapPath;
	}
	virtual const DifficultyIndex& GetDifficultyIndex() const
	{
		return m_diffIndex;
	}
	virtual void SetDemoMode(bool value)
	{
		m_demo = value;
	}
	virtual void SetSongDB(MapDatabase* db)
	{
		m_db = db;
	}
};

Game* Game::Create(const DifficultyIndex& difficulty, GameFlags flags)
{
	Game_Impl* impl = new Game_Impl(difficulty, flags);
	return impl;
}

Game* Game::Create(MultiplayerScreen* multiplayer, const DifficultyIndex& difficulty, GameFlags flags)
{
	Game_Impl* impl = new Game_Impl(difficulty, flags);
	impl->MakeMultiplayer(multiplayer);
	return impl;
}

Game* Game::Create(const String& difficulty, GameFlags flags)
{
	Game_Impl* impl = new Game_Impl(difficulty, flags);
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
