#pragma once
#include <Beatmap/Beatmap.hpp>
#include <Beatmap/AudioEffects.hpp>
#include <Audio/AudioStream.hpp>

/*
	Audio effect with customized parameters
*/
class AudioPlayback;
struct GameAudioEffect : public AudioEffect
{
public:
	GameAudioEffect() = default;
	GameAudioEffect(const AudioEffect& other);

	// Creates a DSP matching this effect
	DSP* CreateDSP(AudioPlayback& playback, uint32 sampleRate);
	// Applies the given parameters overriding some settings for this effect (depending on the effect)
	void SetParams(DSP* dsp, AudioPlayback& playback, HoldObjectState* object);
};

struct SwitchableAudio {
	bool m_enabled;
	Ref<AudioStream> m_audio;
};

/* 
	Handles playback of map audio
	keeps track of the state of sound effects
*/
class AudioPlayback : Unique
{
public:
	AudioPlayback();
	~AudioPlayback();
	// Loads audio for beatmap
	//	specify the root path for the map in order to let this class find the audio files
	bool Init(class BeatmapPlayback& playback, const String& mapRootPath);

	// Updates effects
	void Tick(float deltaTime);

	// Play from start or continue
	void Play();
	void Advance(MapTime ms);
	MapTime GetPosition() const;
	void SetPosition(MapTime time);

	// Pause the playback
	void TogglePause();
	void Pause();

	bool IsPaused() const { return m_paused; }
	bool HasEnded() const;

	// Sets either button effect 0 or 1
	void SetEffect(uint32 index, HoldObjectState* object, class BeatmapPlayback& playback);
	// Sets the hearability of the currently active button effect on 'index'
	void SetEffectEnabled(uint32 index, bool enabled);
	void ClearEffect(uint32 index, HoldObjectState* object);

	// Sets the effect to be used for lasers
	void SetLaserEffect(EffectType type);

	// The input which controls the laser filter amount
	void SetLaserFilterInput(float input, bool active = false);
	float GetLaserFilterInput() const;

	// Set the mix value of the effect on lasers
	void SetLaserEffectMix(float mix);
	float GetLaserEffectMix() const;

	// Toggle FX track or normal track
	// this is just to support maps that do actually have an FX track
	void SetFXTrackEnabled(bool enabled);
	
	// Switch audio track
	void SetSwitchableTrackEnabled(size_t index, bool enabled);
	void ResetSwitchableTracks();

	BeatmapPlayback& GetBeatmapPlayback();
	const Beatmap& GetBeatmap() const;
	const String& GetBeatmapRootPath() const;
	void SetPlaybackSpeed(float speed);
	float GetPlaybackSpeed() const;
	void SetVolume(float volume);

private:
	// Returns the track that should have effects applied to them
	Ref<AudioStream> m_GetDSPTrack();
	void m_CleanupDSP(class DSP*& ptr);
	void m_SetLaserEffectParameter(float input);

	// Map player
	class BeatmapPlayback* m_playback;
	const class Beatmap* m_beatmap;
	// Root path of where the map was loaded from
	String m_beatmapRootPath;

	Ref<AudioStream> m_music;
	Ref<AudioStream> m_fxtrack;
	Vector<SwitchableAudio> m_switchables;
	Vector<int32> m_enabledSwitchables;
	int32 m_laserSwitchable = -1;
	

	float m_musicVolume = 1.0f;

	bool m_paused = false;
	bool m_fxtrackEnabled = true;

	EffectType m_laserEffectType = EffectType::None;
	GameAudioEffect m_laserEffect;
	class DSP* m_laserDSP = nullptr;
	float m_laserEffectMix = 1.0f;
	float m_laserInput = 0.0f;

	GameAudioEffect m_buttonEffects[2];
	class DSP* m_buttonDSPs[2] = { nullptr };
	HoldObjectState* m_currentHoldEffects[2] = { nullptr };
	float m_effectMix[2] = { 0.0f };
};