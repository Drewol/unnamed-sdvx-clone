#include "stdafx.h"
#include "AudioStreamPcm.hpp"

bool AudioStreamPcm::Init(Audio* audio, const String& path, bool preload)
{
	AudioStreamBase::Init(audio, path, preload);
	m_initSampling(m_sampleRate);
	return true;
}
void AudioStreamPcm::SetPosition_Internal(int32 pos)
{
	//negative pos is causing issues somewhere
	//TODO: Investigate more
	m_playPos = Math::Max(0, pos);
}
int32 AudioStreamPcm::GetStreamPosition_Internal()
{
	return m_playPos;
}
int32 AudioStreamPcm::GetStreamRate_Internal()
{
	return m_sampleRate;
}
float* AudioStreamPcm::GetPCM_Internal()
{
	return m_pcm;
}
uint32 AudioStreamPcm::GetSampleRate_Internal() const
{
	return m_sampleRate;
}
int32 AudioStreamPcm::DecodeData_Internal()
{
	uint32 samplesPerRead = 128;
	int32 retVal = samplesPerRead;
	bool earlyOut = false;
	for (size_t i = 0; i < samplesPerRead; i++)
	{
		if (m_playPos < 0)
		{
			m_readBuffer[0][i] = 0;
			m_readBuffer[1][i] = 0;
			m_playPos++;
			continue;
		}
		else if ((uint64)m_playPos >= m_samplesTotal)
		{
			m_currentBufferSize = m_bufferSize;
			m_remainingBufferData = m_bufferSize;
			m_readBuffer[0][i] = 0;
			m_readBuffer[1][i] = 0;
			if (!earlyOut)
			{
				retVal = i;
				earlyOut = true;
			}
			continue;
		}
		m_readBuffer[0][i] = m_pcm[m_playPos * 2];
		m_readBuffer[1][i] = m_pcm[m_playPos * 2 + 1];
		m_playPos++;
	}
	m_currentBufferSize = samplesPerRead;
	m_remainingBufferData = samplesPerRead;
	return retVal;
}

uint64 AudioStreamPcm::GetSampleCount_Internal() const
{
	return m_samplesTotal;
}
void AudioStreamPcm::PreRenderDSPs_Internal(Vector<DSP*>& DSPs)
{
	auto originalPlayPos = m_playPos;
	for (auto&& dsp : DSPs)
	{
		m_playPos = ((uint64)dsp->startTime * (uint64)m_sampleRate) / 1000;
		m_samplePos = m_playPos;
		int64 endSamplePos = ((uint64)dsp->endTime * (uint64)m_sampleRate) / 1000;
		endSamplePos = Math::Min(endSamplePos, (int64)m_samplesTotal);
		if (m_playPos >= endSamplePos)
		{
			Logf("Effect %s at %dms not rendered", Logger::Severity::Debug, dsp->GetName(), dsp->startTime);
			continue;
		}

		uint32 numSamples = endSamplePos - m_playPos;
		float* buffer = new float[numSamples * 2];
		memcpy(buffer, m_pcm + m_playPos * 2, numSamples * 2 * sizeof(float));
		dsp->Process(buffer, numSamples);
		memcpy(m_pcm + m_playPos * 2, buffer, numSamples * 2 * sizeof(float));
		Logf("Rendered %s at %dms with %d samples", Logger::Severity::Debug, dsp->GetName(), dsp->startTime, numSamples);
		delete[] buffer;
	}
	m_playPos = originalPlayPos;
	m_samplePos = m_playPos;
}

AudioStreamPcm::~AudioStreamPcm()
{
	Deregister();
	if (m_pcm)
	{
		delete m_pcm;
	}
}

Ref<AudioStream> AudioStreamPcm::Create(class Audio* audio, const Ref<AudioStream>& other)
{
	AudioStreamPcm* impl = new AudioStreamPcm();
	impl->m_pcm = nullptr;
	float* source = other->GetPCM();
	uint64 sampleCount = other->GetPCMCount();
	if (source == nullptr || sampleCount == 0)
	{
		delete impl;
		impl = nullptr;
	}
	else
	{
		impl->m_playPos = 0;
		impl->m_pcm = new float[sampleCount * 2];
		memcpy(impl->m_pcm, source, sampleCount * 2 * sizeof(float));
		impl->m_sampleRate = other->GetSampleRate();
		impl->m_samplesTotal = sampleCount;
		impl->Init(audio, "", false);
	}
	return Ref<AudioStream>(impl);
}
