#include "stdafx.h"
#include "AudioStreamOgg.hpp"
#include <vorbis/vorbisfile.h>

AudioStreamOgg::~AudioStreamOgg()
{
	Deregister();

	for (size_t i = 0; i < m_numChannels; i++)
	{
		delete[] m_readBuffer[i];
	}
	delete[] m_readBuffer;

	if (!m_preloaded)
	{
		ov_clear(&m_ovf);
	}
}

bool AudioStreamOgg::Init(Audio* audio, const String& path, bool preload)
{
	if (!AudioStreamBase::Init(audio, path, preload))
		return false;

	int32 r = ov_open_callbacks(this, &m_ovf, 0, 0, {
														(decltype(ov_callbacks::read_func))&AudioStreamOgg::m_Read,
														(decltype(ov_callbacks::seek_func))&AudioStreamOgg::m_Seek,
														nullptr, // Close
														(decltype(ov_callbacks::tell_func))&AudioStreamOgg::m_Tell,
		});
	if (r != 0)
	{
		Logf("ov_open_callbacks failed with code %d", Logger::Severity::Error, r);
		return false;
	}

	vorbis_comment* comments = ov_comment(&m_ovf, 0);
	vorbis_info* infoPtr = ov_info(&m_ovf, 0);
	if (!infoPtr)
		return false;
	m_info = *infoPtr;
	m_samplesTotal = ov_pcm_total(&m_ovf, 0);

	if (preload)
	{
		float** readBuffer;
		int r;
		int totalRead = 0;
		while (true)
		{
			r = ov_read_float(&m_ovf, &readBuffer, 1024, 0);
			if (r < 0)
				continue;
			if (r == 0)
				break;
			else
			{
				if (m_info.channels == 2)
				{
					for (int i = 0; i < r; i++)
					{
						m_pcm.Add(readBuffer[0][i]);
						m_pcm.Add(readBuffer[1][i]);
					}
				}
				else if (m_info.channels == 1)
				{
					for (int i = 0; i < r; i++)
					{
						m_pcm.Add(readBuffer[0][i]);
						m_pcm.Add(readBuffer[0][i]);
					}
				}
				totalRead += r;
			}
		}
		m_samplesTotal = totalRead;
		ov_clear(&m_ovf);
		m_playPos = 0;
	}
	m_initSampling(m_info.rate);
	return true;
}

void AudioStreamOgg::SetPosition_Internal(int32 pos)
{
	if (m_preloaded)
	{
		if (pos < 0)
			m_playPos = 0;
		else
			m_playPos = pos;

		return;
	}
	ov_pcm_seek(&m_ovf, pos);
}

int32 AudioStreamOgg::GetStreamPosition_Internal()
{
	if (m_preloaded)
		return m_playPos;

	return (int32)ov_pcm_tell(&m_ovf);
}

int32 AudioStreamOgg::GetStreamRate_Internal()
{
	return (int32)m_info.rate;
}

float* AudioStreamOgg::GetPCM_Internal()
{
	if (m_preloaded)
		return &m_pcm.front();
	return nullptr;
}

uint64 AudioStreamOgg::GetSampleCount_Internal() const
{
	if (m_preloaded)
	{
		return m_samplesTotal;
	}
	return 0;
}

uint32 AudioStreamOgg::GetSampleRate_Internal() const
{
	return m_info.rate;
}

int32 AudioStreamOgg::DecodeData_Internal()
{
	if (m_preloaded)
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

	float** readBuffer;
	int32 r = ov_read_float(&m_ovf, &readBuffer, m_bufferSize, 0);
	if (r > 0)
	{
		if (m_info.channels == 1)
		{
			// Copy mono to read buffer
			for (int32 i = 0; i < r; i++)
			{
				m_readBuffer[0][i] = readBuffer[0][i];
				m_readBuffer[1][i] = readBuffer[0][i];
			}
		}
		else
		{
			// Copy data to read buffer
			for (int32 i = 0; i < r; i++)
			{
				m_readBuffer[0][i] = readBuffer[0][i];
				m_readBuffer[1][i] = readBuffer[1][i];
			}
		}
		m_currentBufferSize = r;
		m_remainingBufferData = r;
		return r;
	}
	else if (r == 0)
	{
		// EOF
		m_playing = false;
		return -1;
	}
	else
	{
		// Error
		m_playing = false;
		Logf("Ogg Stream error %d", Logger::Severity::Warning, r);
		return -1;
	}
}

size_t AudioStreamOgg::m_Read(void* ptr, size_t size, size_t nmemb, AudioStreamOgg* self)
{
	return self->m_reader().Serialize(ptr, nmemb * size);
}

int AudioStreamOgg::m_Seek(AudioStreamOgg* self, int64 offset, int whence)
{
	if (whence == SEEK_SET)
		self->m_reader().Seek((size_t)offset);
	else if (whence == SEEK_CUR)
		self->m_reader().Skip((size_t)offset);
	else if (whence == SEEK_END)
		self->m_reader().SeekReverse((size_t)offset);
	else
		assert(false);
	return 0;
}

long AudioStreamOgg::m_Tell(AudioStreamOgg* self)
{
	return (long)self->m_reader().Tell();
}

Ref<AudioStream> AudioStreamOgg::Create(class Audio* audio, const String& path, bool preload)
{
	AudioStreamOgg* impl = new AudioStreamOgg();
	if (!impl->Init(audio, path, preload))
	{
		delete impl;
		impl = nullptr;
		return Ref<AudioStream>();
	}
	return Ref<AudioStream>(impl);
}
