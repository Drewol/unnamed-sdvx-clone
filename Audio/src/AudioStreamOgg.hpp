#pragma once
#include "stdafx.h"
#include "AudioStreamBase.hpp"
#include <vorbis/vorbisfile.h>

class AudioStreamOgg : public AudioStreamBase
{
protected:
	OggVorbis_File m_ovf;
	vorbis_info m_info;
	Vector<float> m_pcm;
	int64 m_playPos;

	bool Init(Audio* audio, const String& path, bool preload) override;
	void SetPosition_Internal(int32 pos) override;
	int32 GetStreamPosition_Internal() override;
	int32 GetStreamRate_Internal() override;
	float* GetPCM_Internal() override;
	uint32 GetSampleRate_Internal() const override;
	uint64 GetSampleCount_Internal() const override;
	int32 DecodeData_Internal() override;

private:
	static size_t m_Read(void* ptr, size_t size, size_t nmemb, AudioStreamOgg* self);
	static int m_Seek(AudioStreamOgg* self, int64 offset, int whence);
	static long m_Tell(AudioStreamOgg* self);

public:
	AudioStreamOgg() = default;
	~AudioStreamOgg();
	static Ref<AudioStream> Create(class Audio* audio, const String& path, bool preload);
};
