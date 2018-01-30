#include "stdafx.h"
#include "Editor.hpp"
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
#include "GUI/PlayingSongInfo.hpp"

// TODO(local): TryLoadMap taken from Game.cpp, might, want to find a better place to put it then.

// Try load map helper
Ref<Beatmap> TryLoadChart(const String& path)
{
	// Load map file
	Beatmap* newMap = new Beatmap();
	File mapFile;
	if (!mapFile.OpenRead(path))
	{
		delete newMap;
		return Ref<Beatmap>();
	}
	FileReader reader(mapFile);
	if (!newMap->Load(reader))
	{
		delete newMap;
		return Ref<Beatmap>();
	}
	return Ref<Beatmap>(newMap);
}

class Editor_Impl : public Editor
{
public:
	String m_chartRootPath;
	String m_chartPath;
	DifficultyIndex m_diffIndex;

private:
	bool m_playing = true;
	bool m_started = false;
	bool m_paused = false;
	bool m_ended = false;

	Ref<Canvas> m_canvas;

	// The beatmap
	Ref<Beatmap> m_chart;
	// Beatmap playback manager (object and timing point selector)
	BeatmapPlayback m_playback;
	// Audio playback manager (music and FX))
	AudioPlayback m_audioPlayback;
	// Applied audio offset
	int32 m_audioOffset = 0;

	Sample m_slamSample;
	Sample m_clickSamples[2];
	Sample* m_fxSamples;

public:
	Editor_Impl(const String& chartPath)
	{
		// Store path to map
		m_chartPath = Path::Normalize(chartPath);
		// Get Parent path
		m_chartRootPath = Path::RemoveLast(m_chartPath, nullptr);

		m_diffIndex.id = -1;
		m_diffIndex.mapId = -1;
	}

	Editor_Impl(const DifficultyIndex& difficulty)
	{
		// Store path to map
		m_chartPath = Path::Normalize(difficulty.path);
		m_diffIndex = difficulty;
		// Get Parent path
		m_chartRootPath = Path::RemoveLast(m_chartPath, nullptr);
	}

	~Editor_Impl()
	{
		g_rootCanvas->Remove(m_canvas.As<GUIElementBase>());
	}

	AsyncAssetLoader loader;
	virtual bool AsyncLoad() override
	{
		ProfilerScope $("AsyncLoad Editor");

		if (!Path::FileExists(m_chartPath))
		{
			Logf("Couldn't find chart at %s", Logger::Error, m_chartPath);
			return false;
		}

		m_chart = TryLoadChart(m_chartPath);

		// Check failure of above loading attempts
		if (!m_chart)
		{
			Logf("Failed to load chart", Logger::Warning);
			return false;
		}

		const BeatmapSettings& chartSettings = m_chart->GetMapSettings();

		// Load beatmap audio
		if (!m_audioPlayback.Init(m_playback, m_chartRootPath))
			return false;

		// Load audio offset
		m_audioOffset = g_gameConfig.GetInt(GameConfigKeys::GlobalOffset);
		m_playback.audioOffset = m_audioOffset;

		/// TODO: Check if debugmute is enabled
		g_audio->SetGlobalVolume(g_gameConfig.GetFloat(GameConfigKeys::MasterVolume));

		if (!InitSFX())
			return false;

		// TODO(local): This is where the game untializes track graphics,
		// so do something else here
		//m_track = new Track();
		//loader.AddLoadable(*m_track, "Track");

		// I guess something as simple as this for the time being is good enough
		//loader.AddTexture(squareParticleTexture, "particle_square.png");

		if (!loader.Load())
			return false;

		return true;
	}

	virtual bool AsyncFinalize() override
	{
		if (!loader.Finalize())
			return false;

		return true;
	}

	virtual bool Init() override
	{
		// Add to root canvas to be rendered (this makes the HUD visible)
		Canvas::Slot* rootSlot = g_rootCanvas->Add(m_canvas.As<GUIElementBase>());
		if (g_aspectRatio < 640.f / 480.f)
		{
			Vector2 canvasRes = GUISlotBase::ApplyFill(FillMode::Fit, Vector2(640, 480), Rect(0, 0, g_resolution.x, g_resolution.y)).size;

			Vector2 topLeft = Vector2(g_resolution / 2 - canvasRes / 2);

			Vector2 bottomRight = topLeft + canvasRes;
			rootSlot->allowOverflow = true;
			topLeft /= g_resolution;
			bottomRight /= g_resolution;

			rootSlot->anchor = Anchor(topLeft.x, Math::Min(topLeft.y, 0.20f), bottomRight.x, bottomRight.y);
		}
		else
			rootSlot->anchor = Anchors::Full;
		return true;
	}

	// Loads sound effects
	bool InitSFX()
	{
		CheckedLoad(m_slamSample = g_application->LoadSample("laser_slam"));
		CheckedLoad(m_clickSamples[0] = g_application->LoadSample("click-01"));
		CheckedLoad(m_clickSamples[1] = g_application->LoadSample("click-02"));

		auto samples = m_chart->GetSamplePaths();
		m_fxSamples = new Sample[samples.size()];
		for (int i = 0; i < samples.size(); i++)
		{
			CheckedLoad(m_fxSamples[i] = g_application->LoadSample(m_chartRootPath + "/" + samples[i], true));
		}

		return true;
	}

	bool InitEditor()
	{
		m_playback = BeatmapPlayback(*m_chart);
		m_playback.OnEventChanged.Add(this, &Editor_Impl::OnEventChanged);
		m_playback.OnFXBegin.Add(this, &Editor_Impl::OnFXBegin);
		m_playback.OnFXEnd.Add(this, &Editor_Impl::OnFXEnd);
		m_playback.Reset();

		return true;
	}

	void OnLaserSlamHit(LaserObjectState* object)
	{
		m_slamSample->Play();
	}

	void OnEventChanged(EventKey key, EventData data)
	{
		if (key == EventKey::LaserEffectType)
		{
			m_audioPlayback.SetLaserEffect(data.effectVal);
		}
		else if (key == EventKey::LaserEffectMix)
		{
			m_audioPlayback.SetLaserEffectMix(data.floatVal);
		}
		else if (key == EventKey::SlamVolume)
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

	virtual void OnKeyPressed(int32 key) override
	{
		if (key == SDLK_PAUSE || key == SDLK_RETURN)
		{
			// TODO(local): sync to quantize
			m_audioPlayback.TogglePause();
			m_paused = m_audioPlayback.IsPaused();
		}
		else if (key == SDLK_PAGEUP)
		{
			// TODO(local): Skip by step and sync to quantize
			m_audioPlayback.Advance(5000);
		}
		else if (key == SDLK_ESCAPE)
		{
			// TODO(local): menu or something
		}
		else if (key == SDLK_F5)
		{
			// TODO(local): test chart
		}
		else if (key == SDLK_TAB)
		{
			//g_gameWindow->SetCursorVisible(!m_settingsBar->IsShown());
			//m_settingsBar->SetShow(!m_settingsBar->IsShown());
		}
	}

	virtual bool IsPlaying() const override
	{
		return m_playing;
	}

	virtual Ref<Beatmap> GetBeatmap() override
	{
		return m_chart;
	}
	virtual class BeatmapPlayback& GetPlayback() override
	{
		return m_playback;
	}

	virtual const String& GetMapRootPath() const
	{
		return m_chartRootPath;
	}
	virtual const String& GetMapPath() const
	{
		return m_chartPath;
	}
	virtual const DifficultyIndex& GetDifficultyIndex() const
	{
		return m_diffIndex;
	}
};

Editor* Editor::Create(const DifficultyIndex & difficulty)
{
	Editor_Impl* impl = new Editor_Impl(difficulty);
	return impl;
}

Editor* Editor::Create(const String & chartPath)
{
	Editor_Impl* impl = new Editor_Impl(chartPath);
	return impl;
}
