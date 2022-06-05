#pragma once
#include "AudioBase.hpp"

class Audio;

/*
	Audio stream object, currently only supports .ogg format
	The data is pre-loaded into memory and streamed from there
*/
class AudioStream : public AudioBase
{
public:
	static Ref<AudioStream> Create(Audio* audio, const String& path, bool preload);
	static Ref<AudioStream> Clone(Audio* audio, Ref<AudioStream> source);
	virtual ~AudioStream() = default;
	// Starts playback of the stream or continues a paused stream
	virtual void Play() = 0;
	virtual void Pause() = 0;
	virtual bool HasEnded() const = 0;
	// Sets the playback position in milliseconds
	// negative time alowed, which will produce no audio for a certain amount of time
	virtual void SetPosition(int32 pos) = 0;
};
