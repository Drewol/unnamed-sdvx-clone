#include "stdafx.h"
#include "AudioBase.hpp"
#include "Audio.hpp"
#include "Audio_Impl.hpp"

uint32 DSP::GetStartSample() const
{
	return static_cast<uint32>(startTime * static_cast<double>(m_audio->GetSampleRate()) / 1000.0);
}

uint32 DSP::GetCurrentSample() const
{
	return static_cast<uint32>(m_audioBase->GetPosition() * static_cast<double>(m_audio->GetSampleRate()) / 1000.0);
}

DSP::~DSP()
{
	// Make sure this is removed from parent
	assert(!m_audioBase);
}

void DSP::SetAudio(Audio_Impl* audio)
{
	assert(!m_audio && !m_audioBase);

	m_audio = audio;
	m_audioBase = nullptr;
}

void DSP::SetAudioBase(class AudioBase* audioBase)
{
	if (!audioBase)
	{
		m_audioBase = nullptr;
		m_audio = nullptr;

		return;
	}

	assert(!m_audio && !m_audioBase);

	m_audioBase = audioBase;
	m_audio = audioBase->audio;

	assert(m_sampleRate == m_audio->GetSampleRate());
}

AudioBase::~AudioBase()
{
	// Check this to make sure the audio is not being destroyed while it is still registered
	assert(!audio);
	assert(DSPs.empty());
}
uint32 AudioBase::GetAudioSampleRate() const
{
	return audio->GetSampleRate();
}
void AudioBase::ProcessDSPs(float* out, uint32 numSamples)
{
	for(DSP* dsp : DSPs)
	{
		dsp->Process(out, numSamples);
	}
}
void AudioBase::AddDSP(DSP* dsp)
{
	audio->lock.lock();
	DSPs.AddUnique(dsp);
	// Sort by priority
	DSPs.Sort([](DSP* l, DSP* r)
	{
		if(l->priority == r->priority)
			return l < r;
		return l->priority < r->priority;
	});
	dsp->SetAudioBase(this);
	audio->lock.unlock();
}
void AudioBase::RemoveDSP(DSP* dsp)
{
	assert(DSPs.Contains(dsp));

	audio->lock.lock();
	DSPs.Remove(dsp);
	dsp->SetAudioBase(nullptr);
	audio->lock.unlock();
}

void AudioBase::Deregister()
{
	// Remove from audio manager
	if(audio)
	{
		audio->Deregister(this);
	}

	// Unbind DSP's
	// It is safe to do here since the audio won't be rendered again after a call to deregister
	for(DSP* dsp : DSPs)
	{
		dsp->RemoveAudioBase();
	}
	DSPs.clear();
}
