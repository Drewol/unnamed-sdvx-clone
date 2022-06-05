#include "stdafx.h"
#include "AudioBase.hpp"
#include "Audio.hpp"
#include "Audio_Impl.hpp"

uint32 DSP::GetStartSample() const
{
	return static_cast<uint32>(startTime * static_cast<double>(m_audioBase->GetSampleRate()) / 1000.0);
}

uint32 DSP::GetCurrentSample() const
{
	return static_cast<uint32>(m_audioBase->GetSamplePos());
}

DSP::~DSP()
{
	// Make sure this is removed from parent
	assert(!m_audioBase);
}

bool DSP::Sorter(DSP*& a, DSP*& b)
{
	if (a->priority == b->priority)
	{
		return a->startTime < b->startTime;
	}
	return a->priority < b->priority;
}

void DSP::SetAudioBase(class AudioBase* audioBase)
{
	if (!audioBase)
	{
		m_audioBase = nullptr;

		return;
	}

	m_audioBase = audioBase;
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
	for (DSP* dsp : DSPs)
	{
		dsp->Process(out, numSamples);
	}
}
void AudioBase::AddDSP(DSP* dsp)
{
	audio->lock.lock();
	DSPs.AddUnique(dsp);
	// Sort by priority
	DSPs.Sort([](DSP* l, DSP* r) {
		if (l->priority == r->priority)
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
	if (audio)
	{
		audio->Deregister(this);
	}

	// Unbind DSP's
	// It is safe to do here since the audio won't be rendered again after a call to deregister
	for (DSP* dsp : DSPs)
	{
		dsp->RemoveAudioBase();
	}
	DSPs.clear();
}
