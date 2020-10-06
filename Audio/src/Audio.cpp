#include "stdafx.h"
#include "Audio.hpp"
#include "AudioStream.hpp"
#include "Audio_Impl.hpp"
#include "AudioOutput.hpp"
#include "DSP.hpp"

Audio* g_audio = nullptr;
static Audio_Impl g_impl;

Audio_Impl::Audio_Impl()
{
#if _DEBUG
	InitMemoryGuard();
#endif
}

void Audio_Impl::Mix(void* data, uint32& numSamples)
{
	double adv = GetSecondsPerSample();

	uint32 outputChannels = this->output->GetNumChannels();
	if (output->IsIntegerFormat())
	{
		memset(data, 0, numSamples * sizeof(int16) * outputChannels);
	}
	else
	{
		memset(data, 0, numSamples * sizeof(float) * outputChannels);
	}

	uint32 currentNumberOfSamples = 0;
	while(currentNumberOfSamples < numSamples)
	{
		// Generate new sample
		if(m_remainingSamples <= 0)
		{
			// Clear sample buffer storing a fixed amount of samples
			m_sampleBuffer.fill(0);

			// Render items
			lock.lock();
			for(auto& item : itemsToRender)
			{
				// Clear per-channel data
				m_itemBuffer.fill(0);
				item->Process(m_itemBuffer.data(), m_sampleBufferLength);
#if _DEBUG
				CheckMemoryGuard();
#endif
				item->ProcessDSPs(m_itemBuffer.data(), m_sampleBufferLength);
#if _DEBUG
				CheckMemoryGuard();
#endif

				// Mix into buffer and apply volume scaling
				for(uint32 i = 0; i < m_sampleBufferLength; i++)
				{
					m_sampleBuffer[i * 2 + 0] += m_itemBuffer[i * 2] * item->GetVolume();
					m_sampleBuffer[i * 2 + 1] += m_itemBuffer[i * 2 + 1] * item->GetVolume();
				}
			}

			// Process global DSPs
			for(auto dsp : globalDSPs)
			{
				dsp->Process(m_sampleBuffer.data(), m_sampleBufferLength);
			}
			lock.unlock();

			// Apply volume levels
			for(uint32 i = 0; i < m_sampleBufferLength; i++)
			{
				m_sampleBuffer[i * 2 + 0] *= globalVolume;
				m_sampleBuffer[i * 2 + 1] *= globalVolume;
				// Safety clamp to [-1, 1] that should help protect speakers a bit in case of corruption
				// this will clip, but so will values outside [-1, 1] anyway
				m_sampleBuffer[i * 2 + 0] = fmin(fmax(m_sampleBuffer[i * 2 + 0], -1.f), 1.f);
				m_sampleBuffer[i * 2 + 1] = fmin(fmax(m_sampleBuffer[i * 2 + 1], -1.f), 1.f);
			}

			// Set new remaining buffer data
			m_remainingSamples = m_sampleBufferLength;
		}

		// Copy samples from sample buffer
		uint32 sampleOffset = m_sampleBufferLength - m_remainingSamples;
		uint32 maxSamples = Math::Min(numSamples - currentNumberOfSamples, m_remainingSamples);
		for(uint32 c = 0; c < outputChannels; c++)
		{
			if(c < 2)
			{
				for(uint32 i = 0; i < maxSamples; i++)
				{
					if (output->IsIntegerFormat())
					{
						((int16*)data)[(currentNumberOfSamples + i) * outputChannels + c] = (int16)(0x7FFF * Math::Clamp(m_sampleBuffer[(sampleOffset + i) * 2 + c],-1.f,1.f));
					}
					else
					{
						((float*)data)[(currentNumberOfSamples + i) * outputChannels + c] = m_sampleBuffer[(sampleOffset + i) * 2 + c];
					}
				}
			}
			// TODO: Mix to surround channels as well?
		}
		m_remainingSamples -= maxSamples;
		currentNumberOfSamples += maxSamples;
	}
}
void Audio_Impl::Start()
{
	limiter = new LimiterDSP(GetSampleRate());
	limiter->SetAudio(this);
	limiter->releaseTime = 0.2f;

	globalDSPs.Add(limiter);
	output->Start(this);
}
void Audio_Impl::Stop()
{
	output->Stop();
	globalDSPs.Remove(limiter);

	delete limiter;
	limiter = nullptr;
}
void Audio_Impl::Register(AudioBase* audio)
{
	if (audio)
	{
		lock.lock();
		itemsToRender.AddUnique(audio);
		audio->audio = this;
		lock.unlock();
	}
}
void Audio_Impl::Deregister(AudioBase* audio)
{
	lock.lock();
	itemsToRender.Remove(audio);
	audio->audio = nullptr;
	lock.unlock();
}
uint32 Audio_Impl::GetSampleRate() const
{
	return output->GetSampleRate();
}
double Audio_Impl::GetSecondsPerSample() const
{
	return 1.0 / (double)GetSampleRate();
}

Audio::Audio()
{
	// Enforce single instance
	assert(g_audio == nullptr);
	g_audio = this;
}
Audio::~Audio()
{
	if(m_initialized)
	{
		g_impl.Stop();
		delete g_impl.output;
		g_impl.output = nullptr;
	}

	assert(g_audio == this);
	g_audio = nullptr;
}
bool Audio::Init(bool exclusive)
{
	audioLatency = 0;

	g_impl.output = new AudioOutput();
	if(!g_impl.output->Init(exclusive))
	{
		delete g_impl.output;
		g_impl.output = nullptr;
		return false;
	}

	g_impl.Start();

	return m_initialized = true;
}
void Audio::SetGlobalVolume(float vol)
{
	g_impl.globalVolume = vol;
}
uint32 Audio::GetSampleRate() const
{
	return g_impl.output->GetSampleRate();
}
class Audio_Impl* Audio::GetImpl()
{
	return &g_impl;
}

Ref<AudioStream> Audio::CreateStream(const String& path, bool preload)
{
	return AudioStream::Create(this, path, preload);
}
Sample Audio::CreateSample(const String& path)
{
	return SampleRes::Create(this, path);
}

#if _DEBUG
void Audio_Impl::InitMemoryGuard()
{
	m_guard.fill(0);
}
void Audio_Impl::CheckMemoryGuard()
{
	for (auto x : m_guard) assert(x == 0);
}
#endif