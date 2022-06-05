#pragma once
#include "AudioBase.hpp"

/*
	Audio sample, only supports wav files in signed 16 bit stereo or mono
*/
class SampleRes : public AudioBase
{
public:
	static Ref<SampleRes> Create(class Audio* audio, const String& path);
	virtual ~SampleRes() = default;

public:
	virtual const Buffer& GetData() const = 0;
	virtual uint32 GetBitsPerSample() const = 0;
	virtual uint32 GetNumChannels() const = 0;

	// Plays this sample from the start
	virtual void Play(bool looping = false) = 0;
	virtual void Stop() = 0;
	virtual bool IsPlaying() const = 0;
};

typedef Ref<SampleRes> Sample;
