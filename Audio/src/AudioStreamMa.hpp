#include "stdafx.h"
#include "AudioStreamBase.hpp"
#include "miniaudio.h"

class AudioStreamMa : public AudioStreamBase
{
private:
	Buffer m_Internaldata;
	float* m_pcm = nullptr;
	int64 m_playbackPointer = 0;
	const int sample_rate = 48000;
	ma_decoder m_decoder = {  };
protected:
	bool Init(Audio* audio, const String& path, bool preload) override;
	int32 GetStreamPosition_Internal() override;
	int32 GetStreamRate_Internal() override;
	void SetPosition_Internal(int32 pos) override;
	int32 DecodeData_Internal() override;
	float* GetPCM_Internal() override;
	uint32 GetSampleRate_Internal() const override;
public:
	AudioStreamMa() = default;
	~AudioStreamMa();
	static Ref<AudioStream> Create(class Audio* audio, const String& path, bool preload);
};
