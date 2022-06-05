#include "stdafx.h"
#include "AudioStreamMp3.hpp"

// https://en.wikipedia.org/wiki/Synchsafe
int AudioStreamMp3::m_unsynchsafe(int in)
{
	int out = 0, mask = 0x7F000000;

	while (mask)
	{
		out >>= 1;
		out |= in & mask;
		mask >>= 8;
	}

	return out;
}

int AudioStreamMp3::m_toLittleEndian(int num)
{
	return ((num >> 24) & 0xff) |	   // move byte 3 to byte 0
		((num << 8) & 0xff0000) |   // move byte 1 to byte 2
		((num >> 8) & 0xff00) |	   // move byte 2 to byte 1
		((num << 24) & 0xff000000); // byte 0 to byte 3
}
AudioStreamMp3::~AudioStreamMp3()
{
	Deregister();
	mp3_done(m_decoder);

	for (size_t i = 0; i < m_numChannels; i++)
	{
		delete[] m_readBuffer[i];
	}
	delete[] m_readBuffer;
}
bool AudioStreamMp3::Init(Audio* audio, const String& path, bool preload)
{
	///TODO: Write non-preload functions
	if (!AudioStreamBase::Init(audio, path, true)) // Always preload for now
		return false;

	// Always use preloaded data
	m_mp3dataLength = m_reader().GetSize();
	m_dataSource = m_data.data();
	int32 tagSize = 0;

	String tag = "tag";
	for (size_t i = 0; i < 3; i++)
	{
		tag[i] = m_dataSource[i];
	}
	while (tag == "ID3")
	{
		tagSize += m_unsynchsafe(m_toLittleEndian(*(int32*)(m_dataSource + 6 + tagSize))) + 10;
		for (size_t i = 0; i < 3; i++)
		{
			tag[i] = m_dataSource[i + tagSize];
		}
		if (tag == "3DI")
		{
			tagSize += 10;
		}
		for (size_t i = 0; i < 3; i++)
		{
			tag[i] = m_dataSource[i + tagSize];
		}
	}
	// Scan MP3 frame offsets
	uint32 sampleOffset = 0;
	for (size_t i = tagSize; i < m_mp3dataLength;)
	{
		if (m_dataSource[i] == 0xFF)
		{
			if (i + 1 > m_mp3dataLength)
				continue;
			if ((m_dataSource[i + 1] & 0xE0) == 0xE0) // Frame Sync
			{
				uint8 version = (m_dataSource[i + 1] & 0x18) >> 3;
				uint8 layer = (m_dataSource[i + 1] & 0x06) >> 1;
				bool crc = (m_dataSource[i + 1] & 0x01) != 0;
				uint8 bitrateIndex = (m_dataSource[i + 2] & 0xF0) >> 4;
				uint8 rateIndex = (m_dataSource[i + 2] & 0x0C) >> 2;
				bool paddingEnabled = ((m_dataSource[i + 2] & 0x02) >> 1) != 0;
				uint8 channelFlags = ((m_dataSource[i + 3] & 0xC0) >> 6);
				if (bitrateIndex == 0xF || rateIndex > 2) // bad
				{
					return false;
					i++;
					continue;
				}

				uint8 channels = ((channelFlags & 0x3) == 0x3) ? 1 : 2;

				uint32 linearVersion = version == 0x03 ? 0 : 1; // Version 1/2
				uint32 bitrate = mp3_bitrate_tab[linearVersion][bitrateIndex] * 1000;
				uint32 sampleRate = mp3_freq_tab[rateIndex];
				uint32 padding = paddingEnabled ? 1 : 0;

				uint32 frameLength = 144 * bitrate / sampleRate + padding;
				if (frameLength == 0)
				{
					return false;
					i++;
					continue;
				}

				i += frameLength;
				uint32 frameSamples = (linearVersion == 0) ? 1152 : 576;
				m_frameIndices.Add((int32)sampleOffset, i);
				sampleOffset += frameSamples;
				continue; // Skip header
			}
		}
		i++;
	}

	// No mp3 frames found
	if (m_frameIndices.empty())
	{
		Logf("No valid mp3 frames found in file \"%s\"", Logger::Severity::Warning, path);
		return false;
	}

	// Total sample
	m_samplesTotal = sampleOffset;

	m_decoder = (mp3_decoder_t*)mp3_create();
	m_preloaded = false;
	SetPosition_Internal(-400000);
	int32 r = DecodeData_Internal();
	if (r <= 0)
		return false;

	if (preload)
	{
		int totalSamples = 0;
		while (r > 0)
		{
			for (int32 i = 0; i < r; i++)
			{
				m_pcm.Add(m_readBuffer[0][i]);
				m_pcm.Add(m_readBuffer[1][i]);
			}
			totalSamples += r;
			r = DecodeData_Internal();
		}
		m_data.clear();
		m_dataSource = nullptr;
		m_samplesTotal = totalSamples;
	}
	m_preloaded = preload;
	m_playPos = 0;

	return true;
}
void AudioStreamMp3::SetPosition_Internal(int32 pos)
{
	if (m_preloaded)
	{
		if (pos < 0)
			m_playPos = 0;
		else
			m_playPos = pos;

		return;
	}

	auto it = m_frameIndices.lower_bound(pos);
	if (it == m_frameIndices.end())
	{
		--it; // Take the last frame
	}
	if (it != m_frameIndices.end())
	{
		m_mp3samplePosition = it->first;
		m_mp3dataOffset = it->second;
	}
}
int32 AudioStreamMp3::GetStreamPosition_Internal()
{
	if (m_preloaded)
		return m_playPos;

	return m_mp3samplePosition;
}
int32 AudioStreamMp3::GetStreamRate_Internal()
{
	return (int32)m_samplingRate;
}
uint32 AudioStreamMp3::GetSampleRate_Internal() const
{
	return m_samplingRate;
}
uint64 AudioStreamMp3::GetSampleCount_Internal() const
{
	if (m_preloaded)
	{
		return m_samplesTotal;
	}
	return 0;
}

float* AudioStreamMp3::GetPCM_Internal()
{
	if (m_preloaded)
		return &m_pcm.front();

	return nullptr;
}
int32 AudioStreamMp3::DecodeData_Internal()
{
	if (m_preloaded)
	{
		uint32 samplesPerRead = 128;

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
				m_currentBufferSize = samplesPerRead;
				m_remainingBufferData = samplesPerRead;
				m_playing = false;
				return i;
			}
			m_readBuffer[0][i] = m_pcm[m_playPos * 2];
			m_readBuffer[1][i] = m_pcm[m_playPos * 2 + 1];
			m_playPos++;
		}
		m_currentBufferSize = samplesPerRead;
		m_remainingBufferData = samplesPerRead;
		return samplesPerRead;
	}

	int16 buffer[MP3_MAX_SAMPLES_PER_FRAME];
	mp3_info_t info;
	int32 readData = 0;
	while (true)
	{
		readData = mp3_decode(m_decoder, (uint8*)m_dataSource + m_mp3dataOffset, (int)(m_mp3dataLength - m_mp3dataOffset), buffer, &info);
		m_mp3dataOffset += readData;
		if (m_mp3dataOffset >= m_mp3dataLength) // EOF
			return -1;
		if (readData <= 0)
			return -1;
		if (info.audio_bytes >= 0)
			break;
	}

	int32 samplesGotten = info.audio_bytes / (info.channels * sizeof(short));
	m_mp3samplePosition += samplesGotten;

	if (m_firstFrame)
	{
		m_bufferSize = MP3_MAX_SAMPLES_PER_FRAME / 2;
		m_initSampling(m_samplingRate = info.sample_rate);
		m_firstFrame = false;
	}

	// Copy data to read buffer
	for (int32 i = 0; i < samplesGotten; i++)
	{
		if (info.channels == 1)
		{
			m_readBuffer[0][i] = (float)buffer[i] / (float)0x7FFF;
			m_readBuffer[1][i] = m_readBuffer[0][i];
		}
		else if (info.channels == 2)
		{
			m_readBuffer[0][i] = (float)buffer[i * 2 + 0] / (float)0x7FFF;
			m_readBuffer[1][i] = (float)buffer[i * 2 + 1] / (float)0x7FFF;
		}
	}
	m_currentBufferSize = samplesGotten;
	m_remainingBufferData = samplesGotten;
	return samplesGotten;
}

Ref<AudioStream> AudioStreamMp3::Create(Audio* audio, const String& path, bool preload)
{
	AudioStreamMp3* impl = new AudioStreamMp3();
	if (!impl->Init(audio, path, preload))
	{
		delete impl;
		impl = nullptr;
		return Ref<AudioStream>();
	}
	return Ref<AudioStream>(impl);
}
