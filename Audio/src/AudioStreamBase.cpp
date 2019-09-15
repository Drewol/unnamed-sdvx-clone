#include "stdafx.h"
#include "Shared/Profiling.hpp"
#include "AudioStreamBase.hpp"

// Fixed point format for resampling
const uint64 AudioStreamBase::fp_sampleStep = 1ull << 48;

BinaryStream& AudioStreamBase::Reader()
{
	return m_preloaded ? (BinaryStream&)m_memoryReader : (BinaryStream&)m_fileReader;
}
bool AudioStreamBase::Init(Audio* audio, const String& path, bool preload)
{
	m_audio = audio;

	if(!m_file.OpenRead(path))
		return false;

	if(preload)
	{
		m_data.resize(m_file.GetSize());
		m_file.Read(m_data.data(), m_data.size());
		m_memoryReader = MemoryReader(m_data);
		m_preloaded = preload;
	}
	else
	{
		m_fileReader = FileReader(m_file);
		m_preloaded = false;
	}

	return true;
}
void AudioStreamBase::InitSampling(uint32 sampleRate)
{
	// Calculate the sample step if the rate is not the same as the output rate
	double sampleStep = (double)sampleRate / (double)m_audio->GetSampleRate();
	m_sampleStepIncrement = (uint64)(sampleStep * (double)fp_sampleStep);
	double stepCheck = (double)m_sampleStepIncrement / (double)fp_sampleStep;
	// TODO: for practice mode: m_sampleStepIncrement *= playback_speed;
	m_numChannels = 2;
	m_readBuffer = new float*[m_numChannels];
	for(uint32 c = 0; c < m_numChannels; c++)
	{
		m_readBuffer[c] = new float[m_bufferSize];
	}
}

void AudioStreamBase::Play()
{
	if(!m_playing)
	{
		m_playing = true;
	}
	if(m_paused)
	{
		m_paused = false;
		RestartTiming();
	}
}
void AudioStreamBase::Pause()
{
	if(!m_paused)
	{
		// Store time the stream was paused
		m_streamTimeOffset = m_streamTimer.SecondsAsDouble();
		m_paused = true;
	}
	else
	{
		m_paused = false;
		RestartTiming();
	}
}
bool AudioStreamBase::HasEnded() const
{
	return m_ended;
}
uint64 AudioStreamBase::SecondsToSamples(double s) const
{
	return (uint64)(s * (double)const_cast<AudioStreamBase*>(this)->GetStreamRate_Internal());
}
double AudioStreamBase::SamplesToSeconds(int64 s) const
{
	return (double)s / (double)const_cast<AudioStreamBase*>(this)->GetStreamRate_Internal();
}
double AudioStreamBase::GetPositionSeconds(bool allowFreezeSkip /*= true*/) const
{
	double samplePosTime = SamplesToSeconds(m_samplePos);
	if(m_paused || m_samplePos < 0)
		return samplePosTime;
	else
	{
		double ret = m_streamTimeOffset + m_streamTimer.SecondsAsDouble() - m_offsetCorrection;
		if(allowFreezeSkip && (ret - samplePosTime) > 0.2f) // Prevent time from running of when the application freezes
			return samplePosTime;
		return ret;
	}
}
int32 AudioStreamBase::GetPosition() const
{
	return (int32)(GetPositionSeconds() * 1000.0);
}
void AudioStreamBase::SetPosition(int32 pos)
{
	m_lock.lock();
	m_remainingBufferData = 0;
	m_samplePos = SecondsToSamples((double)pos / 1000.0);
	SetPosition_Internal((int32)m_samplePos);
	m_ended = false;
	m_lock.unlock();
}
float* AudioStreamBase::GetPCM()
{
	return GetPCM_Internal();
}
uint32 AudioStreamBase::GetSampleRate() const
{
	return GetSampleRate_Internal();
}
void AudioStreamBase::RestartTiming()
{
	m_streamTimeOffset = SamplesToSeconds(m_samplePos); // Add audio latency to this offset
	m_samplePos = 0;
	m_streamTimer.Restart();
	m_offsetCorrection = 0.0f;
	m_deltaSum = 0;
	m_deltaSamples = 0;
}
void AudioStreamBase::Process(float* out, uint32 numSamples)
{
	if(!m_playing || m_paused)
		return;

	m_lock.lock();

	uint32 outCount = 0;
	while(outCount < numSamples)
	{
		if(m_remainingBufferData > 0)
		{
			uint32 idxStart = (m_currentBufferSize - m_remainingBufferData);
			uint32 readOffset = 0; // Offset from the start to read from
			for(uint32 i = 0; outCount < numSamples && readOffset < m_remainingBufferData; i++)
			{
				if(m_samplePos < 0)
				{
					out[outCount * 2] = 0.0f;
					out[outCount * 2 + 1] = 0.0f;
				}
				else
				{
					out[outCount * 2] = m_readBuffer[0][idxStart + readOffset];
					out[outCount * 2 + 1] = m_readBuffer[1][idxStart + readOffset];
				}
				outCount++;

				// Increment source sample with resampling
				m_sampleStep += m_sampleStepIncrement * PlaybackSpeed;
				while(m_sampleStep >= fp_sampleStep)
				{
					m_sampleStep -= fp_sampleStep;
					if(m_samplePos >= 0)
						readOffset++;
					m_samplePos++;
				}
			}
			m_remainingBufferData -= readOffset;
		}

		if(outCount >= numSamples)
			break;

		// Read more data
		if(DecodeData_Internal() <= 0)
		{
			// Ended
			Logf("Audio stream ended", Logger::Info);
			m_ended = true;
			m_playing = false;
			break;
		}
	}

	// Store timing info
	if (m_samplePos > 0)
	{
		m_samplePos = GetStreamPosition_Internal() - (int64)m_remainingBufferData;
	}

	if(m_samplePos > 0)
	{
		if(m_samplePos >= m_samplesTotal)
		{
			if(!m_ended)
			{
				// Ended
				Logf("Audio stream ended", Logger::Info);
				m_ended = true;
			}
		}

		double timingDelta = GetPositionSeconds(false) - SamplesToSeconds(m_samplePos);
		m_deltaSum += timingDelta;
		m_deltaSamples += 1;

		double avgDelta = m_deltaSum / (double)m_deltaSamples;
		if(abs(timingDelta - avgDelta) > 0.2)
		{
			Logf("Timing restart, delta = %f", Logger::Info, avgDelta);
			RestartTiming();
		}
		else
		{
			if(fabs(avgDelta) > 0.001f)
			{
				// Fine tune timing
				double step = abs(avgDelta) * 0.1f;
				step = Math::Min(step, fabs(timingDelta)) * Math::Sign(timingDelta);
				m_offsetCorrection += step;
			}
		}
	}

	m_lock.unlock();
}
