#include "stdafx.h"
#include "Game.hpp"
#include "Application.hpp"
#include <Beatmap/BeatmapPlayback.hpp>
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
#include "GameConfig.hpp"

#ifdef _WIN32
#include"SDL_keycode.h"
#else
#include "SDL2/SDL_keycode.h"
#endif

#include "GUI/GUI.hpp"
#include "GUI/HealthGauge.hpp"
#include "GUI/SettingsBar.hpp"

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
	bool m_paused = false;
	bool m_ended = false;

	bool m_renderDebugHUD = false;

	// Map object approach speed, scaled by BPM
	float m_hispeed = 1.0f;

	// Current lane toggle status
	bool m_hideLane = false;

    // Use m-mod and what m-mod speed
    bool m_usemMod = false;
    bool m_usecMod = false;
    float m_modSpeed = 400;

	// Game Canvas
	Ref<Canvas> m_canvas;
	Ref<HealthGauge> m_scoringGauge;
	Ref<SettingsBar> m_settingsBar;
	Ref<CommonGUIStyle> m_guiStyle;
	Ref<Label> m_scoreText;

	Graphics::Font m_fontDivlit;

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

	// Currently active timing point
	const TimingPoint* m_currentTiming;
	// Currently visible gameplay objects
	Vector<ObjectState*> m_currentObjectSet;
	MapTime m_lastMapTime;

	// Rate to sample gauge;
	MapTime m_gaugeSampleRate;
	float m_gaugeSamples[256] = { 0.0f };


	// Combo gain animation
	Timer m_comboAnimation;

	Sample m_slamSample;
	Sample m_clickSamples[2];
	Sample* m_fxSamples;

	// Roll intensity, default = 1
	const float m_rollIntensityBase = 0.03f;
	float m_rollIntensity = m_rollIntensityBase;

	// Particle effects
	Material particleMaterial;
	Texture basicParticleTexture;
	Texture squareParticleTexture;
	ParticleSystem m_particleSystem;
	Ref<ParticleEmitter> m_laserFollowEmitters[2];
	Ref<ParticleEmitter> m_holdEmitters[6];
public:
	Game_Impl(const String& mapPath)
	{
		// Store path to map
		m_mapPath = Path::Normalize(mapPath);
		// Get Parent path
		m_mapRootPath = Path::RemoveLast(m_mapPath, nullptr);

		m_diffIndex.id = -1;
		m_diffIndex.mapId = -1;

		m_hispeed = g_gameConfig.GetFloat(GameConfigKeys::HiSpeed);
		m_usemMod = g_gameConfig.GetBool(GameConfigKeys::UseMMod);
		m_usecMod = g_gameConfig.GetBool(GameConfigKeys::UseCMod);
		m_modSpeed = g_gameConfig.GetFloat(GameConfigKeys::ModSpeed);
	}

	Game_Impl(const DifficultyIndex& difficulty)
	{
		// Store path to map
		m_mapPath = Path::Normalize(difficulty.path);
		m_diffIndex = difficulty;
		// Get Parent path
		m_mapRootPath = Path::RemoveLast(m_mapPath, nullptr);

		m_hispeed = g_gameConfig.GetFloat(GameConfigKeys::HiSpeed);
        m_usemMod = g_gameConfig.GetBool(GameConfigKeys::UseMMod);
        m_usecMod = g_gameConfig.GetBool(GameConfigKeys::UseCMod);
        m_modSpeed = g_gameConfig.GetFloat(GameConfigKeys::ModSpeed);
	}
	~Game_Impl()
	{
		if(m_track)
			delete m_track;
		if(m_background)
			delete m_background;


		// Save hispeed
		g_gameConfig.Set(GameConfigKeys::HiSpeed, m_hispeed);

		g_rootCanvas->Remove(m_canvas.As<GUIElementBase>()); 

		// In case the cursor was still hidden
		g_gameWindow->SetCursorVisible(true);
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
			Logf("Failed to load map", Logger::Warning);
			return false;
		}

		// Enable debug functionality
		if(g_application->GetAppCommandLine().Contains("-debug"))
		{
			m_renderDebugHUD = true;
		}

		const BeatmapSettings& mapSettings = m_beatmap->GetMapSettings();

		// Try to load beatmap jacket image
		String jacketPath = m_mapRootPath + "/" + mapSettings.jacketPath;
		m_jacketImage = ImageRes::Create(jacketPath);


		m_gaugeSamples[256] = { 0.0f };
		MapTime firstObjectTime = m_beatmap->GetLinearObjects().front()->time;
		ObjectState *const* lastObj = &m_beatmap->GetLinearObjects().back();
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
		

		m_gaugeSampleRate = lastObjectTime / 256;



        // Move this somewhere else?
        // Set hi-speed for m-Mod
        // Uses the "mode" of BPMs in the chart, should use median?
        if(m_usemMod)
        {
            Map<double, MapTime> bpmDurations;
            const Vector<TimingPoint*>& timingPoints = m_beatmap->GetLinearTimingPoints();
            MapTime lastMT = 0;
            MapTime largestMT = -1;
            double useBPM = -1;
            double lastBPM = -1;
            for (TimingPoint* tp : timingPoints)
            {
                double thisBPM = tp->GetBPM();
                if (!bpmDurations.count(thisBPM))
                {
                    bpmDurations[thisBPM] = 0;
                }
                MapTime timeSinceLastTP = tp->time - lastMT;
                bpmDurations[thisBPM] += timeSinceLastTP;
                if (bpmDurations[thisBPM] > largestMT)
                {
                    useBPM = thisBPM;
                    largestMT = bpmDurations[thisBPM];
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

		if(!InitSFX())
			return false;

		// Intialize track graphics
		m_track = new Track();
		loader.AddLoadable(*m_track, "Track");

		// Load particle textures
		loader.AddTexture(basicParticleTexture, "particle_flare.png");
		loader.AddTexture(squareParticleTexture, "particle_square.png");

		if(!InitHUD())
			return false;

		if(!loader.Load())
			return false;

		return true;
	}
	virtual bool AsyncFinalize() override
	{
		if(m_jacketImage)
		{
			m_jacketTexture = TextureRes::Create(g_gl, m_jacketImage);
		}

		if(!loader.Finalize())
			return false;

		m_scoringGauge->fillMaterial->opaque = false;

		// Load particle material
		m_particleSystem = ParticleSystemRes::Create(g_gl);
		CheckedLoad(particleMaterial = g_application->LoadMaterial("particle"));
		particleMaterial->blendMode = MaterialBlendMode::Additive;
		particleMaterial->opaque = false;

		// Background 
		/// TODO: Load this async
		CheckedLoad(m_background = CreateBackground(this));

		// Do this here so we don't get input events while still loading
		m_scoring.SetPlayback(m_playback);
		m_scoring.SetInput(&g_input);
		m_scoring.Reset(); // Initialize

		return true;
	}
	virtual bool Init() override
	{
		// Add to root canvas to be rendered (this makes the HUD visible)
		Canvas::Slot* rootSlot = g_rootCanvas->Add(m_canvas.As<GUIElementBase>());
		rootSlot->anchor = Anchors::Full;
		return true;
	}

	// Restart map
	virtual void Restart()
	{
		m_camera = Camera();

		bool audioReinit = m_audioPlayback.Init(m_playback, m_mapRootPath);
		assert(audioReinit);

		// Audio leadin
		ApplyAudioLeadin();

		m_paused = false;
		m_started = false;
		m_ended = false;
		m_hideLane = false;
		m_playback.Reset(m_lastMapTime);
		m_scoring.Reset();

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
		m_track->ClearEffects();
		m_particleSystem->Reset();
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
	}
	virtual void Render(float deltaTime) override
	{
		// Magic number to make the resulting HiSpeed numbers somewhat match the resulting speeds in the other games
		m_track->SetViewRange(7.4f / m_hispeed); 

		m_track->Tick(m_playback, deltaTime);

		// Get render state from the camera
		float rollA = m_scoring.GetLaserRollOutput(0);
		float rollB = m_scoring.GetLaserRollOutput(1);
		m_camera.SetTargetRoll(rollA + rollB);
		m_camera.SetRollIntensity(m_rollIntensity);

		// Set track zoom
		if(!m_settingsBar->IsShown()) // Overridden settings?
		{
			m_camera.zoomBottom = m_playback.GetZoom(0);
			m_camera.zoomTop = m_playback.GetZoom(1);
		}
		m_camera.track = m_track;
		m_camera.Tick(deltaTime,m_playback);
		RenderState rs = m_camera.CreateRenderState(true);

		// Draw BG first
		m_background->Render(deltaTime);

		// Main render queue
		RenderQueue renderQueue(g_gl, rs);

		// Get objects in range
		MapTime msViewRange = m_playback.ViewDistanceToDuration(m_track->GetViewRange());
		m_currentObjectSet = m_playback.GetObjectsInRange(msViewRange);

		// Draw the base track + time division ticks
		m_track->DrawBase(renderQueue);

		// Sort objects to draw
		m_currentObjectSet.Sort([](const TObjectState<void>* a, const TObjectState<void>* b)
		{
			auto ObjectRenderPriorty = [](const TObjectState<void>* a)
			{
				if(a->type == ObjectType::Single || a->type == ObjectType::Hold)
					return (((ButtonObjectState*)a)->index < 4) ? 1 : 0;
				else
					return 2;
			};
			uint32 renderPriorityA = ObjectRenderPriorty(a);
			uint32 renderPriorityB = ObjectRenderPriorty(b);
			return renderPriorityA < renderPriorityB;
		});

		for(auto& object : m_currentObjectSet)
		{
			m_track->DrawObjectState(renderQueue, m_playback, object, m_scoring.IsObjectHeld(object));
		}

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
		m_track->DrawCombo(scoringRq, m_scoring.currentComboCounter, Color::White, 1.0f + comboZoom);

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

				m_laserFollowEmitters[i]->position.x = m_track->trackWidth * followPos - m_track->trackWidth * 0.5f;
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
					m_holdEmitters[i]->position.x = m_track->GetButtonPlacement(i);
				}
			}
			else
			{
				if(m_holdEmitters[i])
				{
					m_holdEmitters[i].Release();
				}
			}

		}

		// Render particle effects last
		RenderParticles(rs, deltaTime);

		// Render debug hud if enabled
		if(m_renderDebugHUD)
		{
			RenderDebugHUD(deltaTime);
		}
	}

	// Initialize HUD elements/layout
	bool InitHUD()
	{
		CheckedLoad(m_fontDivlit = FontRes::Create(g_gl, "fonts/divlit_custom.ttf"));
		m_guiStyle = g_commonGUIStyle;

		// Game GUI canvas
		m_canvas = Utility::MakeRef(new Canvas());

		{
			// Gauge
			m_scoringGauge = Utility::MakeRef(new HealthGauge());
			loader.AddTexture(m_scoringGauge->fillTexture, "gauge_fill.png");
			loader.AddTexture(m_scoringGauge->frameTexture, "gauge_frame.png");
			loader.AddTexture(m_scoringGauge->bgTexture, "gauge_bg.png");
			loader.AddMaterial(m_scoringGauge->fillMaterial, "gauge");
			m_scoringGauge->barMargin = Margin(36, 34, 33, 34);

			Canvas::Slot* slot = m_canvas->Add(m_scoringGauge.As<GUIElementBase>());
			slot->anchor = Anchor(0.0f, 0.5f);
			slot->alignment = Vector2(0.0f, 0.5f);
			slot->autoSizeX = true;
			slot->autoSizeY = true;
		}

		// Setting bar
		{
			SettingsBar* sb = new SettingsBar(m_guiStyle);
			m_settingsBar = Ref<SettingsBar>(sb);
			sb->AddSetting(&m_camera.zoomBottom, -1.0f, 1.0f, "Bottom Zoom");
			sb->AddSetting(&m_camera.zoomTop, -1.0f, 1.0f, "Top Zoom");
			sb->AddSetting(&m_camera.cameraNearBase, 0.01f, 1.0f, "Camera Near Base");
			sb->AddSetting(&m_camera.cameraNearMult, 0.0f, 2.0f, "Camera Near Mult");
			sb->AddSetting(&m_camera.cameraHeightBase, 0.01f, 1.0f, "Camera Height Base");
			sb->AddSetting(&m_camera.cameraHeightMult, 0.0f, 2.0f, "Camera Height Mult");
			sb->AddSetting(&m_hispeed, 0.25f, 16.0f, "HiSpeed multiplier");
			sb->AddSetting(&m_scoring.laserDistanceLeniency, 1.0f/32.0f, 1.0f, "Laser Distance Leniency");
			m_settingsBar->SetShow(false);

			Canvas::Slot* settingsSlot = m_canvas->Add(sb->MakeShared());
			settingsSlot->anchor = Anchor(0.75f, 0.0f, 1.0f, 1.0f);
			settingsSlot->autoSizeX = false;
			settingsSlot->autoSizeY = false;
			settingsSlot->SetZOrder(2);
		}

		// Score
		{
			Panel* scorePanel = new Panel();
			loader.AddTexture(scorePanel->texture, "scoring_base.png");
			scorePanel->color = Color::White;

			Canvas::Slot* scoreSlot = m_canvas->Add(scorePanel->MakeShared());
			scoreSlot->anchor = Anchors::TopsRight;
			scoreSlot->alignment = Vector2(1.0f, 0.0f);
			scoreSlot->autoSizeX = true;
			scoreSlot->autoSizeY = true;

			m_scoreText = Ref<Label>(new Label());
			m_scoreText->SetFontSize(75);
			m_scoreText->SetText(Utility::WSprintf(L"%08d", 0));
			m_scoreText->SetFont(m_fontDivlit);
			m_scoreText->SetTextOptions(FontRes::Monospace);
			// Padding for this specific font
			Margin textPadding = Margin(0, -20, 0, 0);

			Panel::Slot* slot = scorePanel->SetContent(m_scoreText.As<GUIElementBase>());
			slot->padding = Margin(30, 0, 10, 30) + textPadding;
			slot->alignment = Vector2(0.5f, 0.5f);
		}

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
		if(firstObjectTime < 1000)
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

		auto samples = m_beatmap->GetSamplePaths();
		m_fxSamples = new Sample[samples.size()];
		for (int i = 0; i < samples.size(); i++)
		{
			CheckedLoad(m_fxSamples[i] = g_application->LoadSample(m_mapRootPath + "/" + samples[i], true));
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

        // If c-mod is used
        if (m_usecMod)
        {
            m_playback.OnTimingPointChanged.Add(this, &Game_Impl::OnTimingPointChanged);
        }
		// Register input bindings
		m_scoring.OnButtonMiss.Add(this, &Game_Impl::OnButtonMiss);
		m_scoring.OnLaserSlamHit.Add(this, &Game_Impl::OnLaserSlamHit);
		m_scoring.OnButtonHit.Add(this, &Game_Impl::OnButtonHit);
		m_scoring.OnComboChanged.Add(this, &Game_Impl::OnComboChanged);
		m_scoring.OnObjectHold.Add(this, &Game_Impl::OnObjectHold);
		m_scoring.OnObjectReleased.Add(this, &Game_Impl::OnObjectReleased);
		m_scoring.OnScoreChanged.Add(this, &Game_Impl::OnScoreChanged);

		m_playback.hittableObjectTreshold = Scoring::goodHitTime;

		if(g_application->GetAppCommandLine().Contains("-autobuttons"))
		{
			m_scoring.autoplayButtons = true;
		}

		return true;
	}
	// Processes input and Updates scoring, also handles audio timing management
	void TickGameplay(float deltaTime)
	{
		if(!m_started)
		{
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

		// Link FX track to combo counter for now
		m_audioPlayback.SetFXTrackEnabled(m_scoring.currentComboCounter > 0);

		// Update scoring
		m_scoring.Tick(deltaTime);

		// Update scoring gauge
		m_scoringGauge->rate = m_scoring.currentGauge;

		int32 gaugeSampleSlot = playbackPositionMs;
		gaugeSampleSlot /= m_gaugeSampleRate;
		gaugeSampleSlot = Math::Clamp(gaugeSampleSlot, (int32)0, (int32)255);
		m_gaugeSamples[gaugeSampleSlot] = m_scoring.currentGauge;

		// Get the current timing point
		m_currentTiming = &m_playback.GetCurrentTimingPoint();

        // Update hispeed
        if (g_input.GetButton(Input::Button::BT_S))
        {
            for (int i = 0; i < 2; i++)
            {
				m_hispeed += g_input.GetInputLaserDir(i);
				m_hispeed = Math::Clamp(m_hispeed, 0.1f, 16.f);
				if (m_usecMod || m_usemMod)
				{
					g_gameConfig.Set(GameConfigKeys::ModSpeed, m_hispeed * (float)m_currentTiming->GetBPM());
				}
            }
        }



		m_lastMapTime = playbackPositionMs;
		
		if(m_audioPlayback.HasEnded())
		{
			FinishGame();
		}
	}

	// Called when game is finished and the score screen should show up
	void FinishGame()
	{
		if(m_ended)
			return;

		// Transition to score screen
		TransitionScreen* transition = TransitionScreen::Create(ScoreScreen::Create(this));
		transition->OnLoadingComplete.Add(this, &Game_Impl::OnScoreScreenLoaded);
		g_application->AddTickable(transition);

		m_ended = true;
	}
	void OnScoreScreenLoaded(IAsyncLoadableApplicationTickable* tickable)
	{
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
		emitter->SetStartVelocity(PPConstant<Vector3>({ 0, 0.0f, 2.0f }));
		emitter->SetSpawnVelocityScale(PPRandomRange<float>(0.9f, 2));
		emitter->SetStartColor(PPConstant<Color>((Color)(color * 0.7f)));
		emitter->SetGravity(PPConstant<Vector3>(Vector3(0.0f, 0.0f, -9.81f)));
		emitter->position.y = 0.0f;
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
		emitter->position.y = 0.01f;
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
		return emitter;
	}

	// Main GUI/HUD Rendering loop
	virtual void RenderDebugHUD(float deltaTime)
	{
		// Render debug overlay elements
		RenderQueue& debugRq = g_guiRenderer->Begin();
		auto RenderText = [&](const String& text, const Vector2& pos, const Color& color = Color::White)
		{
			return g_guiRenderer->RenderText(text, pos, color);
		};

		const BeatmapSettings& bms = m_beatmap->GetMapSettings();
		const TimingPoint& tp = m_playback.GetCurrentTimingPoint();
		Vector2 textPos = Vector2(10 + (float)m_scoringGauge->frameTexture->GetSize().x, 10);
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

		textPos.y += RenderText(Utility::Sprintf("Track Zoom Top: %f", m_camera.zoomTop), textPos).y;
		textPos.y += RenderText(Utility::Sprintf("Track Zoom Bottom: %f", m_camera.zoomBottom), textPos).y;

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

		g_guiRenderer->End();
	}

	void OnLaserSlamHit(LaserObjectState* object)
	{
		CameraShake shake(0.2f, 0.5f, 170.0f);
		shake.amplitude = Vector3(0.02f, 0.01f, 0.0f); // Mainly x-axis
		m_camera.AddCameraShake(shake);
		m_slamSample->Play();


		if (object->spin.type != 0)
		{
			m_camera.SetSpin(object->GetDirection(), object->spin.duration, object->spin.type, m_playback);
		}


		float dir = Math::Sign(object->points[1] - object->points[0]);
		float laserPos = m_track->trackWidth * object->points[1] - m_track->trackWidth * 0.5f;
		Ref<ParticleEmitter> ex = CreateExplosionEmitter(m_track->laserColors[object->index], Vector3(dir, 0, 0));
		ex->position = Vector3(laserPos, 0.0f, -0.05f);
	}
	void OnButtonHit(Input::Button button, ScoreHitRating rating, ObjectState* hitObject)
	{
		ButtonObjectState* st = (ButtonObjectState*)hitObject;
		uint32 buttonIdx = (uint32)button;
		Color c = m_track->hitColors[(size_t)rating];

		// The color effect in the button lane
		m_track->AddEffect(new ButtonHitEffect(buttonIdx, c));

		if (st != nullptr && st->hasSample)
		{
			m_fxSamples[st->sampleIndex]->Play();
		}

		if(rating != ScoreHitRating::Idle)
		{
			// Floating text effect
			m_track->AddEffect(new ButtonHitRatingEffect(buttonIdx, rating));

			// Create hit effect particle
			Color hitColor = (buttonIdx < 4) ? Color::White : Color::FromHSV(20, 0.7f, 1.0f);
			float hitWidth = (buttonIdx < 4) ? m_track->buttonWidth : m_track->fxbuttonWidth;
			Ref<ParticleEmitter> emitter = CreateHitEmitter(hitColor, hitWidth);
			emitter->position.x = m_track->GetButtonPlacement(buttonIdx);
			emitter->position.z = -0.05f;
			emitter->position.y = 0.0f;
		}

	}
	void OnButtonMiss(Input::Button button)
	{
		uint32 buttonIdx = (uint32)button;
		m_track->AddEffect(new ButtonHitRatingEffect(buttonIdx, ScoreHitRating::Miss));
	}
	void OnComboChanged(uint32 newCombo)
	{
		m_comboAnimation.Restart();
	}
	void OnScoreChanged(uint32 newScore)
	{
		// Update score text
		if(m_scoreText)
		{
			m_scoreText->SetText(Utility::WSprintf(L"%08d", newScore));
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
			int32 i = (uint8)data.rollVal & 0x3;
			if(i == 0)
				m_rollIntensity = 0;
			else
			{
				m_rollIntensity = m_rollIntensityBase + (float)(i - 1) * 0.0125f;
			}
		}
		else if(key == EventKey::SlamVolume)
		{
			m_slamSample->SetVolume(data.floatVal);
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
		}
	}

	virtual void OnKeyPressed(int32 key) override
	{
		if(key == SDLK_PAUSE)
		{
			m_audioPlayback.TogglePause();
			m_paused = m_audioPlayback.IsPaused();
		}
		else if(key == SDLK_RETURN) // Skip intro
		{
			if(!SkipIntro())
				SkipOutro();
		}
		else if(key == SDLK_PAGEUP)
		{
			m_audioPlayback.Advance(5000);
		}
		else if(key == SDLK_ESCAPE)
		{
			FinishGame();
		}
		else if(key == SDLK_F5) // Restart map
		{
			// Restart
			Restart();
		}
		else if(key == SDLK_F8)
		{
			m_renderDebugHUD = !m_renderDebugHUD;
		}
		else if(key == SDLK_TAB)
		{
			m_settingsBar->SetShow(!m_settingsBar->IsShown());
		}
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

};

Game* Game::Create(const DifficultyIndex& difficulty)
{
	Game_Impl* impl = new Game_Impl(difficulty);
	return impl;
}

Game* Game::Create(const String& difficulty)
{
	Game_Impl* impl = new Game_Impl(difficulty);
	return impl;
}
