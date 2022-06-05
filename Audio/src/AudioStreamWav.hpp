#include "stdafx.h"
#include "AudioStreamBase.hpp"

class AudioStreamWav : public AudioStreamBase
{
private:
	struct WavHeader
	{
		char id[4];
		uint32 nLength;

		bool operator==(const char* rhs) const
		{
			return strncmp(id, rhs, 4) == 0;
		}
		bool operator!=(const char* rhs) const
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
	Buffer m_Internaldata;
	WavFormat m_format;
	Vector<float> m_pcm;
	int64 m_playbackPointer = 0;
	uint64 m_dataPosition = 0;
	uint32 m_decode_ms_adpcm(const Buffer& encoded, Buffer* decoded, uint64 pos);

protected:
	bool Init(Audio* audio, const String& path, bool preload) override;
	int32 GetStreamPosition_Internal() override;
	int32 GetStreamRate_Internal() override;
	void SetPosition_Internal(int32 pos) override;
	int32 DecodeData_Internal() override;
	float* GetPCM_Internal() override;
	uint32 GetSampleRate_Internal() const override;
	uint64 GetSampleCount_Internal() const override;

public:
	AudioStreamWav() = default;
	~AudioStreamWav();
	static Ref<AudioStream> Create(class Audio* audio, const String& path, bool preload);
};
