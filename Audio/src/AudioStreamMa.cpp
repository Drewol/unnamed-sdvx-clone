#include "stdafx.h"
#include "AudioStreamMa.hpp"

#define DR_WAV_IMPLEMENTATION
#include "extras/dr_wav.h" // Enables WAV decoding.
#define DR_FLAC_IMPLEMENTATION
#include "extras/dr_flac.h" // Enables FLAC decoding.
#define DR_MP3_IMPLEMENTATION
#include "extras/dr_mp3.h"   // Enables MP3 decoding.
#define STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"	// Enables Vorbis decoding.

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

AudioStreamMa::~AudioStreamMa()
{
	Deregister();
	if (m_preloaded)
	{
		if (m_pcm)
		{
			ma_free(m_pcm);
			m_pcm = nullptr;
		}
	}
	else
	{
		ma_decoder_uninit(&m_decoder);
	}

	for (size_t i = 0; i < m_numChannels; i++)
	{
		delete[] m_readBuffer[i];
	}
	delete[] m_readBuffer;
}

bool AudioStreamMa::Init(Audio* audio, const String& path, bool preload)
{
	if (!AudioStreamBase::Init(audio, path, preload)) // Always preload for now
		return false;

	ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 2, sample_rate);
	ma_result result;

	if (m_preloaded)
	{
		result = ma_decode_memory((void*)m_data.data(), m_file.GetSize(), &config, &m_samplesTotal, (void**)&m_pcm);
	}
	else
	{
		result = ma_decoder_init_file(*path, &config, &m_decoder);
	}

	if (result != MA_SUCCESS)
	{
		return false;
	}

	m_initSampling(sample_rate);
	return true;
}
int32 AudioStreamMa::GetStreamPosition_Internal()
{
	return m_playbackPointer;
}
int32 AudioStreamMa::GetStreamRate_Internal()
{
	return sample_rate;
}
void AudioStreamMa::SetPosition_Internal(int32 pos)
{
	if (pos < 0)
		m_playbackPointer = 0;
	else
		m_playbackPointer = pos;

	if (!m_preloaded)
	{
		//seek_to_pcm_frame is very slow
		//we want to use ma_decoder_seek_bytes_64
		//but we require some stuff to do that properly

		//ma_decoder_seek_to_pcm_frame(&m_decoder, pos);
		return;
	}
}
int32 AudioStreamMa::DecodeData_Internal()
{
	if (m_preloaded)
	{
		uint32 samplesPerRead = 128;
		int actualRead = 0;
		for (size_t i = 0; i < samplesPerRead; i++)
		{
			if (m_playbackPointer >= (int64)m_samplesTotal)
			{
				m_currentBufferSize = samplesPerRead;
				m_remainingBufferData = samplesPerRead;
				return i;
			}
			m_readBuffer[0][i] = m_pcm[m_playbackPointer * 2];
			m_readBuffer[1][i] = m_pcm[m_playbackPointer * 2 + 1];
			m_playbackPointer++;
			actualRead++;
		}
		m_currentBufferSize = samplesPerRead;
		m_remainingBufferData = samplesPerRead;
		return samplesPerRead;
	}
	else
	{
		uint32 samplesPerRead = 128;
		float decodeBuffer[256];

		int totalRead = ma_decoder_read_pcm_frames(&m_decoder, decodeBuffer, samplesPerRead);
		for (int i = 0; i < totalRead; i++)
		{
			m_readBuffer[0][i] = decodeBuffer[i * 2];
			m_readBuffer[1][i] = decodeBuffer[i * 2 + 1];
		}
		m_currentBufferSize = totalRead;
		m_remainingBufferData = totalRead;
		return totalRead;
	}
	return 0;
}
float* AudioStreamMa::GetPCM_Internal()
{
	return m_pcm;
}
uint32 AudioStreamMa::GetSampleRate_Internal() const
{
	return sample_rate;
}
uint64 AudioStreamMa::GetSampleCount_Internal() const
{
	if (m_preloaded)
	{
		return m_samplesTotal;
	}
	return 0;
}

Ref<AudioStream> AudioStreamMa::Create(class Audio* audio, const String& path, bool preload)
{
	AudioStreamMa* impl = new AudioStreamMa();
	if (!impl->Init(audio, path, preload))
	{
		delete impl;
		impl = nullptr;
	}
	return Utility::CastRef<AudioStreamMa, AudioStream>(Ref<AudioStreamMa>(impl));
}
