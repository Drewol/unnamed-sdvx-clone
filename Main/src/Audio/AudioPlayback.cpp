#include "stdafx.h"
#include "AudioPlayback.hpp"
#include <Beatmap/BeatmapPlayback.hpp>
#include <Beatmap/Beatmap.hpp>
#include <Audio/Audio.hpp>
#include <Audio/DSP.hpp>
#include <Shared/Profiling.hpp>

AudioPlayback::AudioPlayback()
{
}
AudioPlayback::~AudioPlayback()
{
	m_CleanupDSP(m_buttonDSPs[0]);
	m_CleanupDSP(m_buttonDSPs[1]);
	m_CleanupDSP(m_laserDSP);
}
bool AudioPlayback::Init(class BeatmapPlayback &playback, const String &mapRootPath, bool preRender)
{
	// Cleanup exising DSP's
	m_currentHoldEffects[0] = nullptr;
	m_currentHoldEffects[1] = nullptr;
	m_CleanupDSP(m_buttonDSPs[0]);
	m_CleanupDSP(m_buttonDSPs[1]);
	m_CleanupDSP(m_laserDSP);

	m_playback = &playback;
	m_beatmap = &playback.GetBeatmap();
	m_beatmapRootPath = mapRootPath;
	assert(m_beatmap != nullptr);

	// Set default effect type
	SetLaserEffect(EffectType::PeakingFilter);

	const BeatmapSettings &mapSettings = m_beatmap->GetMapSettings();
	String audioPath = Path::Normalize(m_beatmapRootPath + Path::sep + mapSettings.audioNoFX);
	audioPath.TrimBack(' ');
	if (!Path::FileExists(audioPath))
	{
		Logf("Audio file for beatmap does not exists at: \"%s\"", Logger::Severity::Error, audioPath);
		return false;
	}
	m_music = g_audio->CreateStream(audioPath, true);
	if (!m_music)
	{
		Logf("Failed to load any audio for beatmap \"%s\"", Logger::Severity::Error, audioPath);
		return false;
	}

	m_musicVolume = mapSettings.musicVolume;
	m_music->SetVolume(m_musicVolume);

	// Load FX track
	audioPath = Path::Normalize(m_beatmapRootPath + Path::sep + mapSettings.audioFX);
	audioPath.TrimBack(' ');
	if (!audioPath.empty())
	{
		if (!Path::FileExists(audioPath) || Path::IsDirectory(audioPath))
		{
			Logf("FX audio for for beatmap does not exists at: \"%s\" Using real-time effects instead.", Logger::Severity::Warning, audioPath);
		}
		else
		{
			m_fxtrack = g_audio->CreateStream(audioPath, true);
			if (m_fxtrack)
			{
				// Initially mute normal track if fx is enabled
				m_music->SetVolume(0.0f);
			}
		}
	}

	if (m_fxtrack)
	{
		// Prevent loading switchables if fx track is in use.
		return true;
	}

	if (preRender)
	{
		m_fxtrack = AudioStream::Clone(g_audio, m_music);
		assert(m_fxtrack);
		m_PreRenderDSPTrack();
		return true;
	}


	auto switchablePaths = m_beatmap->GetSwitchablePaths();

	// Load switchable audio tracks
	for (auto it = switchablePaths.begin(); it != switchablePaths.end(); ++it)
	{
		audioPath = Path::Normalize(m_beatmapRootPath + Path::sep + *it);
		audioPath.TrimBack(' ');

		SwitchableAudio switchable;
		switchable.m_enabled = false;
		if (!audioPath.empty())
		{
			if (!Path::FileExists(audioPath))
			{
				Logf("Audio for a SwitchAudio effect does not exists at: \"%s\"", Logger::Severity::Warning, audioPath);
			}
			else
			{
				switchable.m_audio = g_audio->CreateStream(audioPath, true);
				if (switchable.m_audio)
				{
					// Mute all switchable audio by default
					switchable.m_audio->SetVolume(0.0f);
				}
			}
		}
		m_switchables.Add(switchable);
	}


	return true;
}
void AudioPlayback::Tick(float deltaTime)
{
}
void AudioPlayback::Play()
{
	m_music->Play();
	if (m_fxtrack)
		m_fxtrack->Play();
	for (auto it = m_switchables.begin(); it != m_switchables.end(); ++it)
		if (it->m_audio)
			it->m_audio->Play();

	m_paused = false;
}
void AudioPlayback::Advance(MapTime ms)
{
	SetPosition(GetPosition() + ms);
}
MapTime AudioPlayback::GetPosition() const
{
	return m_music->GetPosition();
}
void AudioPlayback::SetPosition(MapTime time)
{
	m_music->SetPosition(time);
	if (m_fxtrack)
		m_fxtrack->SetPosition(time);
	for (auto it = m_switchables.begin(); it != m_switchables.end(); ++it)
		if (it->m_audio)
			it->m_audio->SetPosition(time);
}
void AudioPlayback::TogglePause()
{
	if (m_paused)
		Play();
	else
		Pause();
}
void AudioPlayback::Pause()
{
	if (m_paused)
		return;

	m_music->Pause();
	if (m_fxtrack)
		m_fxtrack->Pause();
	for (auto it = m_switchables.begin(); it != m_switchables.end(); ++it)
		if (it->m_audio)
			it->m_audio->Pause();

	m_paused = true;
}
bool AudioPlayback::HasEnded() const
{
	return m_music->HasEnded();
}
void AudioPlayback::SetEffect(uint32 index, HoldObjectState *object, class BeatmapPlayback &playback)
{
	// Don't use effects when using an FX track
	if (m_fxtrack)
		return;

	assert(index <= 1);
	m_CleanupDSP(m_buttonDSPs[index]);
	m_currentHoldEffects[index] = object;

	// For Time based effects
	const TimingPoint *timingPoint = playback.GetTimingPointAt(object->time);
	// Duration of a single bar
	double barDelay = timingPoint->numerator * timingPoint->beatDuration;

	DSP *&dsp = m_buttonDSPs[index];

	m_buttonEffects[index] = m_beatmap->GetEffect(object->effectType);

	// Do not create DSP for SwitchAudio effect
	if (m_buttonEffects[index].type == EffectType::SwitchAudio)
		return;

	Ref<AudioStream> audioTrack = m_GetDSPTrack();

	dsp = m_buttonEffects[index].CreateDSP(m_playback->GetCurrentTimingPoint(),
										   GetLaserFilterInput(),
										   audioTrack->GetAudioSampleRate());
	if (!dsp)
		return;

	Logf("Set effect: %s", Logger::Severity::Debug, dsp->GetName());

	m_buttonEffects[index].SetParams(dsp, *this, object);

	// Initialize mix value to previous value
	dsp->mix = m_effectMix[index];
	dsp->startTime = object->time;
	dsp->chartOffset = playback.GetBeatmap().GetMapSettings().offset;
	dsp->lastTimingPoint = playback.GetCurrentTimingPoint().time;

	// Add the DSP to the track
	audioTrack->AddDSP(dsp);
}
void AudioPlayback::SetEffectEnabled(uint32 index, bool enabled)
{
	assert(index <= 1);
	m_effectMix[index] = enabled ? m_buttonEffects[index].mix.Sample() : 0.0f;

	if (m_buttonEffects[index].type == EffectType::SwitchAudio)
	{
		SetSwitchableTrackEnabled(m_buttonEffects[index].switchaudio.index.Sample(), enabled);
		return;
	}

	if (m_buttonDSPs[index])
	{
		m_buttonDSPs[index]->mix = m_effectMix[index];
	}
}
void AudioPlayback::ClearEffect(uint32 index, HoldObjectState *object)
{
	assert(index <= 1);
	if (m_currentHoldEffects[index] == object)
	{
		m_CleanupDSP(m_buttonDSPs[index]);
		m_currentHoldEffects[index] = nullptr;
	}
}
void AudioPlayback::SetLaserEffect(EffectType type)
{
	if (type != m_laserEffectType)
	{
		m_CleanupDSP(m_laserDSP);
		m_laserEffectType = type;
		m_laserEffect = m_beatmap->GetFilter(type);
	}
}
bool AudioPlayback::m_SkipEffectIfInputIsZero()
{
	return m_laserEffect.type == EffectType::PeakingFilter || m_laserEffect.type == EffectType::HighPassFilter
		|| m_laserEffect.type == EffectType::LowPassFilter || m_laserEffect.type == EffectType::PitchShift
		|| m_laserEffect.type == EffectType::Bitcrush;
}
void AudioPlayback::SetLaserFilterInput(float input, bool active)
{
	if (m_laserEffect.type != EffectType::None && (active && (input != 0.0f || !m_SkipEffectIfInputIsZero())))
	{
		if (m_laserEffect.type == EffectType::SwitchAudio)
		{
			m_laserSwitchable = m_laserEffect.switchaudio.index.Sample();
			SetSwitchableTrackEnabled(m_laserSwitchable, true);
			return;
		}

		// SwitchAudio transition into other filters
		if (m_laserSwitchable > 0)
		{
			SetSwitchableTrackEnabled(m_laserSwitchable, false);
			m_laserSwitchable = -1;
		}

		// Create DSP
		if (!m_laserDSP)
		{
			// Don't use Bitcrush effects over FX track
			if (m_fxtrack && m_laserEffectType == EffectType::Bitcrush)
				return;

			Ref<AudioStream> audioTrack = m_GetDSPTrack();

			m_laserDSP = m_laserEffect.CreateDSP(m_playback->GetCurrentTimingPoint(),
												 GetLaserFilterInput(),
												 audioTrack->GetAudioSampleRate());
			if (!m_laserDSP)
			{
				Logf("Failed to create laser DSP with type %d", Logger::Severity::Warning, m_laserEffect.type);
				return;
			}

			audioTrack->AddDSP(m_laserDSP);
		}

		// Set params
		m_SetLaserEffectParameter(input);
		m_laserDSP->mix = Math::Clamp(m_laserDSP->mix, 0.f, 1.f);
		m_laserInput = input;
	}
	else
	{
		if (m_laserSwitchable > 0)
			SetSwitchableTrackEnabled(m_laserSwitchable, true);

		m_laserSwitchable = -1;
		m_CleanupDSP(m_laserDSP);
		m_laserInput = 0.0f;
	}
}
float AudioPlayback::GetLaserFilterInput() const
{
	return m_laserInput;
}
void AudioPlayback::SetLaserEffectMix(float mix)
{
	m_laserEffectMix = mix;
}
float AudioPlayback::GetLaserEffectMix() const
{
	return m_laserEffectMix;
}
Ref<AudioStream> AudioPlayback::m_GetDSPTrack()
{
	if (m_fxtrack)
		return m_fxtrack;
	return m_music;
}
void AudioPlayback::SetFXTrackEnabled(bool enabled)
{
	if (!m_fxtrack)
		return;
	if (m_fxtrackEnabled != enabled)
	{
		if (enabled)
		{
			m_fxtrack->SetVolume(m_musicVolume);
			m_music->SetVolume(0.0f);
		}
		else
		{
			m_fxtrack->SetVolume(0.0f);
			m_music->SetVolume(m_musicVolume);
		}
	}
	m_fxtrackEnabled = enabled;
}
void AudioPlayback::SetSwitchableTrackEnabled(size_t index, bool enabled)
{
	if (m_fxtrack)
		return;

	assert(index < m_switchables.size());

	int32 disableTrack = -1;
	int32 enableTrack = -1;

	if (!enabled)
	{
		disableTrack = index;
		m_enabledSwitchables.Remove(index);
		enableTrack = m_enabledSwitchables.size() ? m_enabledSwitchables.back() : -2;
	}
	else
	{
		disableTrack = m_enabledSwitchables.size() ? m_enabledSwitchables.back() : -2;
		m_enabledSwitchables.AddUnique(index);
		enableTrack = m_enabledSwitchables.size() ? m_enabledSwitchables.back() : -2;
	}

	if (disableTrack != -1)
	{
		if (disableTrack == -2)
			m_music->SetVolume(0.0f);
		else if (m_switchables[disableTrack].m_audio)
			m_switchables[disableTrack].m_audio->SetVolume(0.0f);
	}

	if (enableTrack != -1)
	{
		if (enableTrack == -2)
			m_music->SetVolume(m_musicVolume);
		else if (m_switchables[enableTrack].m_audio)
			m_switchables[enableTrack].m_audio->SetVolume(m_musicVolume);
	}
}

void AudioPlayback::ResetSwitchableTracks()
{
	for (size_t i = 0; i < m_switchables.size(); ++i)
	{
		if (m_switchables[i].m_audio)
			m_switchables[i].m_audio->SetVolume(0.0f);
	}
	m_music->SetVolume(m_musicVolume);
}

BeatmapPlayback &AudioPlayback::GetBeatmapPlayback()
{
	return *m_playback;
}
const Beatmap &AudioPlayback::GetBeatmap() const
{
	return *m_beatmap;
}
const String &AudioPlayback::GetBeatmapRootPath() const
{
	return m_beatmapRootPath;
}
void AudioPlayback::SetPlaybackSpeed(float speed)
{
	m_music->PlaybackSpeed = speed;
	if (m_fxtrack)
		m_fxtrack->PlaybackSpeed = speed;
	for (auto it = m_switchables.begin(); it != m_switchables.end(); ++it)
		if (it->m_audio)
			it->m_audio->PlaybackSpeed = speed;
}
float AudioPlayback::GetPlaybackSpeed() const
{
	return m_music->PlaybackSpeed;
}
void AudioPlayback::SetVolume(float volume)
{
	m_music->SetVolume(volume);
	if (m_fxtrack)
		m_fxtrack->SetVolume(volume);
	for (auto it = m_switchables.begin(); it != m_switchables.end(); ++it)
		if (it->m_audio)
			it->m_audio->SetVolume(volume);
}
void AudioPlayback::m_CleanupDSP(DSP *&ptr)
{
	if (ptr)
	{
		m_GetDSPTrack()->RemoveDSP(ptr);
		delete ptr;
		ptr = nullptr;
	}
}
void AudioPlayback::m_SetLaserEffectParameter(float input)
{
	assert(input >= 0.0f && input <= 1.0f);

	// Mix float biquad filters, these are applied manualy by changing the filter parameters (gain,q,freq,etc.)
	float mix = m_laserEffectMix;
	double noteDuration = m_playback->GetCurrentTimingPoint().GetWholeNoteLength();
	uint32 actualLength = m_laserEffect.duration.Sample(input).Absolute(noteDuration);

	if (input < 0.1f)
		mix *= input / 0.1f;

	switch (m_laserEffect.type)
	{
	case EffectType::Bitcrush:
	{
		m_laserDSP->mix = m_laserEffect.mix.Sample(input);
		auto *bcDSP = (BitCrusherDSP *)m_laserDSP;
		bcDSP->SetPeriod((float)m_laserEffect.bitcrusher.reduction.Sample(input));
		break;
	}
	case EffectType::Echo:
	{
		m_laserDSP->mix = m_laserEffect.mix.Sample(input);
		auto *echoDSP = (EchoDSP *)m_laserDSP;
		echoDSP->feedback = m_laserEffect.echo.feedback.Sample(input);
		break;
	}
	case EffectType::PeakingFilter:
	{
		m_laserDSP->mix = m_laserEffectMix;
		if (input > 0.8f)
			mix *= 1.0f - (input - 0.8f) / 0.2f;

		auto *bqfDSP = (BQFDSP *)m_laserDSP;
		bqfDSP->SetPeaking(m_laserEffect.peaking.q.Sample(input), m_laserEffect.peaking.freq.Sample(input), m_laserEffect.peaking.gain.Sample(input) * mix);
		break;
	}
	case EffectType::LowPassFilter:
	{
		m_laserDSP->mix = m_laserEffectMix;
		auto *bqfDSP = (BQFDSP *)m_laserDSP;
		bqfDSP->SetLowPass(m_laserEffect.lpf.q.Sample(input) * mix + 0.1f, m_laserEffect.lpf.freq.Sample(input));
		break;
	}
	case EffectType::HighPassFilter:
	{
		m_laserDSP->mix = m_laserEffectMix;
		auto *bqfDSP = (BQFDSP *)m_laserDSP;
		bqfDSP->SetHighPass(m_laserEffect.hpf.q.Sample(input) * mix + 0.1f, m_laserEffect.hpf.freq.Sample(input));
		break;
	}
	case EffectType::PitchShift:
	{
		m_laserDSP->mix = m_laserEffect.mix.Sample(input);
		auto *ps = (PitchShiftDSP *)m_laserDSP;
		ps->amount = m_laserEffect.pitchshift.amount.Sample(input);
		break;
	}
	case EffectType::Gate:
	{
		m_laserDSP->mix = m_laserEffect.mix.Sample(input);
		break;
	}
	case EffectType::Retrigger:
	{
		m_laserDSP->mix = m_laserEffect.mix.Sample(input);
		auto *rt = (RetriggerDSP *)m_laserDSP;
		rt->SetLength(actualLength);
		break;
	}
	default:
		break;
	}
}

void AudioPlayback::m_PreRenderDSPTrack()
{
	ProfilerScope $("Pre-rendering FX effects");
	Vector<DSP *> DSPs;
	for (const auto& chartObj : m_playback->GetBeatmap().GetObjectStates())
	{
		if (chartObj->type == ObjectType::Hold)
		{
			HoldObjectState *holdObj = (HoldObjectState *) chartObj.get();
			if (holdObj->effectType != EffectType::None)
			{
				//Add DSP
				GameAudioEffect effect = m_beatmap->GetEffect(holdObj->effectType);
				DSP *dsp = effect.CreateDSP(*m_playback->GetTimingPointAt(chartObj->time),
											1.0f,
											m_fxtrack->GetSampleRate());
				if (dsp != nullptr)
				{
					effect.SetParams(dsp, *this, holdObj);
					dsp->startTime = chartObj->time;
					dsp->endTime = dsp->startTime + holdObj->duration;
					dsp->SetAudioBase(m_fxtrack.get());
					dsp->priority = GameAudioEffect::GetDefaultEffectPriority(effect.type);
					Logf("Added %s at %dms", Logger::Severity::Debug, dsp->GetName(), dsp->startTime);
					DSPs.Add(dsp);
				}
			}
		}
	}
	DSPs.Sort(DSP::Sorter);
	m_fxtrack->PreRenderDSPs(DSPs);
}

GameAudioEffect::GameAudioEffect(const AudioEffect &other)
{
	*((AudioEffect *)this) = other;
}
