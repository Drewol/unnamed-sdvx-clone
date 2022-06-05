#pragma once
#include "stdafx.h"
#include "AudioStreamBase.hpp"

class AudioStreamPcm : public AudioStreamBase
{
protected:
	float* m_pcm;
	uint32 m_sampleRate;
	int64 m_playPos;

	bool Init(Audio* audio, const String& path, bool preload) override;
	void SetPosition_Internal(int32 pos) override;
	int32 GetStreamPosition_Internal() override;
	int32 GetStreamRate_Internal() override;
	float* GetPCM_Internal() override;
	void PreRenderDSPs_Internal(Vector<DSP*>& DSPs) override;
	uint32 GetSampleRate_Internal() const override;
	uint64 GetSampleCount_Internal() const override;
	int32 DecodeData_Internal() override;

public:
	AudioStreamPcm() = default;
	~AudioStreamPcm();
	static Ref<AudioStream> Create(class Audio* audio, const Ref<AudioStream>& other);
};
