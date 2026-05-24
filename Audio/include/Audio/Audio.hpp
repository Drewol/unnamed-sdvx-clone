#pragma once
#include "AudioStream.hpp"
#include "Sample.hpp"

extern class Audio* g_audio;

/*
	Main audio manager
	keeps track of active samples and audio streams
	also handles mixing and DSP's on playing items
*/
class Audio : Unique
{ 
public:
	Audio();
	~Audio();
	// Initializes the audio device
	bool Init(bool exclusive);
	void SetGlobalVolume(float vol);

	// Opens a stream at path
	//	settings preload loads the whole file into memory before playing
	[[nodiscard]]
	Ref<AudioStream> CreateStream(const String& path, bool preload = false);
	// Open a wav file at path
	[[nodiscard]]
	Sample CreateSample(const String& path);

	// Target/Output sample rate
	[[nodiscard]]
	uint32 GetSampleRate() const;

	// Private
	[[nodiscard]]
	class Audio_Impl* GetImpl();

	// Calculated audio latency by the audio driver (currently unused)
	int64 audioLatency;

private:
	bool m_initialized = false;
};