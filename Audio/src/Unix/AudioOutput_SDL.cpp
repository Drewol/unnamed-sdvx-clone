#include "stdafx.h"
#include "AudioOutput.hpp"
#include <thread>
using std::this_thread::yield;

// This audio driver is an alternative for linux
#ifndef _WIN32
#include "SDL2/SDL.h"
#include "SDL2/SDL_audio.h"

/* SDL audio instance singleton*/
class SDLAudio
{
	SDLAudio()
	{
		SDL_SetMainReady();
		int r = SDL_InitSubSystem(SDL_INIT_AUDIO);
		assert(r == 0);
	}
public:
	~SDLAudio()
	{
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}
	static SDLAudio& Main()
	{
		static SDLAudio sdl;
		return sdl;
	}
};

class AudioOutput_Impl
{
public:
	SDL_AudioSpec m_audioSpec;
	SDL_AudioDeviceID m_deviceId = 0;
	IMixer* m_mixer = nullptr;
	volatile bool m_running = false;

public:
	AudioOutput_Impl()
	{
		int32 numAudioDrivers = SDL_GetNumAudioDrivers();
		for (int32 i = 0; i < numAudioDrivers; i++)
		{
			const char* drvName = SDL_GetAudioDriver(i);
			Logf("Audio driver [%d]: %s", Logger::Severity::Info, i, drvName);
		}

		SDLAudio::Main();
	}
	~AudioOutput_Impl()
	{
		CloseDevice();
	}
	void CloseDevice()
	{
		if (m_deviceId != 0)
			SDL_CloseAudioDevice(m_deviceId);
		m_deviceId = 0;
	}
	bool OpenDevice(const char* dev)
	{
		CloseDevice();

		SDL_AudioSpec desiredSpec;
		memset(&desiredSpec, 0, sizeof(SDL_AudioSpec));
		memset(&m_audioSpec, 0, sizeof(SDL_AudioSpec));
		desiredSpec.freq = 44100;
		desiredSpec.format = AUDIO_F32;
		desiredSpec.channels = 2;    /* 1 = mono, 2 = stereo */
		desiredSpec.samples = 1024;  /* Good low-latency value for callback */
		desiredSpec.callback = (SDL_AudioCallback)&AudioOutput_Impl::FillBuffer;
		desiredSpec.userdata = this;

		const char* audioDriverName = SDL_GetCurrentAudioDriver();
		Logf("Using audio driver: %s", Logger::Severity::Info, audioDriverName);

		int32 numAudioDevices = SDL_GetNumAudioDevices(0);
		for (int32 i = 0; i < numAudioDevices; i++)
		{
			const char* devName = SDL_GetAudioDeviceName(i, 0);
			Logf("Audio device [%d]: %s", Logger::Severity::Info, i, devName);
		}


		m_deviceId = SDL_OpenAudioDevice(dev, 0, &desiredSpec, &m_audioSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);
		if (m_deviceId == 0 || m_deviceId < 2)
		{
			const char* errMsg = SDL_GetError();
			Logf("Failed to open SDL audio device: %s", Logger::Severity::Error, errMsg);
			return false;
		}

		SDL_PauseAudioDevice(m_deviceId, 0);
		return true;
	}
	bool Init()
	{
		OpenDevice(nullptr);
		return true;
	}
	static void SDLCALL FillBuffer(AudioOutput_Impl* self, float* data, int len)
	{
		uint32 bufferSamples = (uint32)(len / (4 * self->m_audioSpec.channels));
		if (self->m_mixer)
			self->m_mixer->Mix(data, bufferSamples);
	}
};

AudioOutput::AudioOutput()
{
	m_impl = new AudioOutput_Impl();
}
AudioOutput::~AudioOutput()
{
	delete m_impl;
}
bool AudioOutput::Init(bool exclusive)
{
	return m_impl->Init();
}
uint32_t AudioOutput::GetNumChannels() const
{
	return m_impl->m_audioSpec.channels;
}
uint32_t AudioOutput::GetSampleRate() const
{
	return m_impl->m_audioSpec.freq;
}
double AudioOutput::GetBufferLength() const
{
	return 0;
}
void AudioOutput::Start(IMixer* mixer)
{
	m_impl->m_mixer = mixer;
}
void AudioOutput::Stop()
{
	m_impl->m_mixer = nullptr;
}
bool AudioOutput::IsIntegerFormat() const
{
	return false;
}
#endif
