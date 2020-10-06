#include "stdafx.h"
#include "Sample.hpp"
#include "Audio_Impl.hpp"
#include "Audio.hpp"

#include "extras/dr_wav.h"   // Enables WAV decoding.
#include "extras/dr_flac.h"  // Enables FLAC decoding.
#include "extras/dr_mp3.h"   // Enables MP3 decoding.
#include "extras/stb_vorbis.c"


#include "miniaudio.h"


class Sample_Impl : public SampleRes
{
public:
	Buffer m_data;
	Audio* m_audio;
	float* m_pcm = nullptr;

	mutex m_lock;

	uint64 m_playbackPointer = 0;
	uint64 m_length = 0;
	bool m_playing = false;
	bool m_looping = false;

public:
	~Sample_Impl()
	{
		Deregister();
		if (m_pcm)
		{
			ma_free(m_pcm);
		}
	}
	virtual void Play(bool looping) override
	{
		m_lock.lock();
		m_playing = true;
		m_playbackPointer = 0;
		m_looping = looping;
		m_lock.unlock();	
	}
	virtual void Stop() override
	{
		m_lock.lock();
		m_playing = false;
		m_looping = false;
		m_lock.unlock();
	}
	bool Init(const String& path)
	{

		ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 2, g_audio->GetSampleRate());
		ma_result result;
		result = ma_decode_file( *path, &config, &m_length, (void**)&m_pcm);

		if (result != MA_SUCCESS)
			return false;

		return true;
	}
	virtual void Process(float* out, uint32 numSamples) override
	{
		if(!m_playing)
			return;

		m_lock.lock();

		for(uint32 i = 0; i < numSamples; i++)
		{
			if(m_playbackPointer >= m_length)
			{
				if(m_looping)
				{
					m_playbackPointer = 0;
				}
				else
				{
					// Playback ended
					m_playing = false;
					break;
				}
			}

			out[i * 2] = m_pcm[m_playbackPointer * 2];
			out[i * 2 + 1] = m_pcm[m_playbackPointer * 2 + 1];
			m_playbackPointer++;
		}
		m_lock.unlock();
	}
	const Buffer& GetData() const override
	{
		return m_data;
	}
	uint32 GetBitsPerSample() const override
	{
		return 32;
	}
	uint32 GetNumChannels() const override
	{
		return 2;
	}
	int32 GetPosition() const override
	{
		return m_playbackPointer;
	}
	float* GetPCM() override
	{
		return nullptr;
	}
	uint32 GetSampleRate() const override
	{
		return g_audio->GetSampleRate();
	}
	bool IsPlaying() const override
	{
		return m_playing;
	}

};

Sample SampleRes::Create(Audio* audio, const String& path)
{
	Sample_Impl* res = new Sample_Impl();
	res->m_audio = audio;

	if(!res->Init(path))
	{
		delete res;
		return Sample();
	}

	audio->GetImpl()->Register(res);

	return Sample(res);
}