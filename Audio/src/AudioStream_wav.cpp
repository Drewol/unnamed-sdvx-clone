#include "stdafx.h"
#include "AudioStreamBase.hpp"

struct WavHeader
{
	char id[4];
	uint32 nLength;

	bool operator ==(const char* rhs) const
	{
		return strncmp(id, rhs, 4) == 0;
	}
	bool operator !=(const char* rhs) const
	{
		return !(*this == rhs);
	}
};


struct WavFormat
{
	uint16 nFormat;
	uint16 nChannels;
	uint32 nSampleRate;
	uint32 nByteRate;
	uint16 nBlockAlign;
	uint16 nBitsPerSample;
};

class AudioStreamWAV_Impl : public AudioStreamBase
{
private:
	Buffer m_Internaldata;
	WavFormat m_format = { 0 };


	uint64 m_playbackPointer = 0;

	uint32 m_decode_ms_adpcm(const Buffer& encoded, Buffer* decoded, uint64 pos)
	{
		int16* pcm = (int16*)decoded->data();

		int8* src = ((int8*)encoded.data()) + pos;
		int8 blockPredictors[] = { 0, 0 };
		int32 ideltas[] = { 0, 0 };
		int32 sample1[] = { 0, 0 };
		int32 sample2[] = { 0, 0 };

		blockPredictors[0] = *src++;
		blockPredictors[1] = *src++;
		assert(blockPredictors[0] >= 0 && blockPredictors[0] < 7);
		assert(blockPredictors[1] >= 0 && blockPredictors[0] < 7);

		int16* src_16 = (int16*)src;
		ideltas[0] = *src_16++;
		ideltas[1] = *src_16++;

		sample1[0] = *src_16++;
		sample1[1] = *src_16++;

		sample2[0] = *src_16++;
		sample2[1] = *src_16++;

		*pcm++ = sample2[0];
		*pcm++ = sample2[1];
		*pcm++ = sample1[0];
		*pcm++ = sample1[1];


		src = (int8*)src_16;
		uint32 decodedCount = 2;

		int AdaptationTable[] = {
			230, 230, 230, 230, 307, 409, 512, 614,
			768, 614, 512, 409, 307, 230, 230, 230
		};
		int AdaptCoeff1[] = { 256, 512, 0, 192, 240, 460, 392 };
		int AdaptCoeff2[] = { 0, -256, 0, 64, 0, -208, -232 };

		// Decode the rest of the data in the block
		int remainingInBlock = m_format.nBlockAlign - 14;
		while (remainingInBlock > 0)
		{
			int8 nibbleData = *src++;

			int8 nibbles[] = { 0, 0 };
			nibbles[0] = nibbleData >> 4;
			nibbles[0] &= 0x0F;
			nibbles[1] = nibbleData & 0x0F;

			int16 predictors[] = { 0, 0 };
			for (size_t i = 0; i < 2; i++)
			{


				predictors[i] = ((sample1[i] * AdaptCoeff1[blockPredictors[i]]) + (sample2[i] * AdaptCoeff2[blockPredictors[i]])) / 256;
				if (nibbles[i] & 0x08)
					predictors[i] += (nibbles[i] - 0x10) * ideltas[i];
				else
					predictors[i] += nibbles[i] * ideltas[i];
				*pcm++ = predictors[i];
				sample2[i] = sample1[i];
				sample1[i] = predictors[i];
				ideltas[i] = (AdaptationTable[nibbles[i]] * ideltas[i]) / 256;
				if (ideltas[i] < 16)
					ideltas[i] = 16;
			}
			decodedCount++;
			remainingInBlock--;
		}
		return decodedCount;
	}


public:
	~AudioStreamWAV_Impl()
	{
		Deregister();
	}

	bool Init(Audio* audio, const String& path, bool preload)
	{
		if (!AudioStreamBase::Init(audio, path, true)) // Always preload for now
			return false;

		WavHeader riff;
		m_memoryReader << riff;
		if (riff != "RIFF")
			return false;

		char riffType[4];
		m_memoryReader.SerializeObject(riffType);
		if (strncmp(riffType, "WAVE", 4) != 0)
			return false;

		while (m_memoryReader.Tell() < m_memoryReader.GetSize())
		{
			WavHeader chunkHdr;
			m_memoryReader << chunkHdr;
			if (chunkHdr == "fmt ")
			{
				m_memoryReader << m_format;
				String format = "Unknown";
				if (m_format.nFormat == 1)
					format = "PCM";
				else if (m_format.nFormat == 2)
					format = "MS_ADPCM";

				Logf("Sample format: %s", Logger::Info, format);
				Logf("Channels: %d", Logger::Info, m_format.nChannels);
				Logf("Sample rate: %d", Logger::Info, m_format.nSampleRate);
				Logf("Bps: %d", Logger::Info, m_format.nBitsPerSample);
				if (m_format.nFormat == 2)
				{
					uint16 cbSize;
					m_memoryReader << cbSize;
					m_memoryReader.Skip(cbSize);
				}
			}
			else if (chunkHdr == "fact")
			{
				uint32 fh;
				m_memoryReader << fh;
				m_samplesTotal = fh;
			}
			else if (chunkHdr == "data") // data Chunk
			{
				// validate header
				if (m_format.nFormat != 1 && m_format.nFormat != 2)
					return false;
				if (m_format.nChannels > 2 || m_format.nChannels == 0)
					return false;
				if (m_format.nBitsPerSample != 16 && m_format.nBitsPerSample != 4)
					return false;

				// Read data
				if (m_format.nFormat == 1)
				{
					m_samplesTotal = chunkHdr.nLength / sizeof(short);
					m_Internaldata.resize(chunkHdr.nLength);
					m_memoryReader.Serialize(m_Internaldata.data(), chunkHdr.nLength);
				}
				else if (m_format.nFormat == 2)
				{
					m_samplesTotal = chunkHdr.nLength;
					m_Internaldata.resize(m_samplesTotal * m_format.nChannels * sizeof(short));
					m_memoryReader.Serialize(m_Internaldata.data(), chunkHdr.nLength);
				}
			}
			else
			{
				m_memoryReader.Skip(chunkHdr.nLength);
			}
		}
		InitSampling(m_format.nSampleRate);

		m_playbackPointer = 0;
		return true;
	}
	virtual int32 GetStreamPosition_Internal()
	{
		return m_playbackPointer;
	}
	virtual int32 GetStreamRate_Internal()
	{
		if (m_format.nFormat == 2)
			return m_format.nByteRate;
		else
			return m_format.nSampleRate * m_format.nChannels;
	}
	virtual void SetPosition_Internal(int32 pos)
	{
		if(pos > 0)
			m_playbackPointer = pos;
		else
			m_playbackPointer = 0;
	}

	virtual int32 DecodeData_Internal()
	{
		if (m_format.nFormat == 1)
		{
			uint32 samplesPerRead = 128;

			if (m_format.nChannels == 2)
			{
				for (uint32 i = 0; i < samplesPerRead; i++)
				{
					if ((m_playbackPointer / m_format.nChannels) >= m_samplesTotal)
					{
						m_currentBufferSize = samplesPerRead;
						m_remainingBufferData = samplesPerRead;
						return i;
					}

					int16* src = ((int16*)m_Internaldata.data()) + m_playbackPointer;
					m_readBuffer[0][i] = (float)src[0] / (float)0x7FFF;
					m_readBuffer[1][i] = (float)src[1] / (float)0x7FFF;
					m_playbackPointer+=2;

				}
			}
			else
			{
				// Mix mono sample
				for (uint32 i = 0; i < samplesPerRead; i++)
				{
					if (m_playbackPointer >= m_samplesTotal)
					{
						m_currentBufferSize = samplesPerRead;
						m_remainingBufferData = samplesPerRead;
						return i;
					}

					int16* src = ((int16*)m_Internaldata.data()) + m_playbackPointer;
					m_readBuffer[0][i] = (float)src[0] / (float)0x7FFF;
					m_readBuffer[1][i] = (float)src[0] / (float)0x7FFF;
					m_playbackPointer++;
				}
			}
			m_currentBufferSize = samplesPerRead;
			m_remainingBufferData = samplesPerRead;
			return samplesPerRead;
		}
		else if (m_format.nFormat == 2)
		{
			Buffer decoded;
			m_playbackPointer -= m_playbackPointer % m_format.nBlockAlign;
			decoded.resize(m_format.nBlockAlign * m_format.nChannels * sizeof(short));
			uint32 decodedCount = m_decode_ms_adpcm(m_Internaldata, &decoded, m_playbackPointer);
			uint32 samplesInserted = 0;
			uint64 bufferOffset = 0;
			for (uint32 i = 0; i < decodedCount; i++)
			{
				int16* src = ((int16*)decoded.data()) + (i * 2);
				m_readBuffer[0][i] = (float)src[0] / (float)0x7FFF;
				m_readBuffer[1][i] = (float)src[1] / (float)0x7FFF;
			}
			
			m_playbackPointer += m_format.nBlockAlign;

			m_currentBufferSize = decodedCount;
			m_remainingBufferData = decodedCount;
			return decodedCount;

		}
		return 0;
	}

};

class AudioStreamRes* CreateAudioStream_wav(class Audio* audio, const String& path, bool preload)
{
	AudioStreamWAV_Impl* impl = new AudioStreamWAV_Impl();
	if (!impl->Init(audio, path, preload))
	{
		delete impl;
		impl = nullptr;
	}
	return impl;
}
