#pragma once
#include "Audio.hpp"
#include "AudioStream.hpp"
#include "Audio_Impl.hpp"

class AudioStreamBase : public AudioStreamRes
{
protected:
	// Fixed point format for sample positions (used in resampling)
	static const uint64 fp_sampleStep;

	Audio* m_audio;
	File m_file;
	Buffer m_data;
	MemoryReader m_memoryReader;
	FileReader m_fileReader;
	bool m_preloaded = false;
	BinaryStream& Reader();

	std::mutex m_lock;

	float** m_readBuffer = nullptr;
	uint32 m_bufferSize = 4096;
	uint32 m_numChannels = 0;
	uint32 m_currentBufferSize = 0;
	uint32 m_remainingBufferData = 0;

	int64 m_samplePos = 0;
	uint64 m_samplesTotal = 0; // Total pcm length of audio stream

	// Resampling values
	uint64 m_sampleStep = 0;
	uint64 m_sampleStepIncrement = 0;

	Timer m_deltaTimer;
	Timer m_streamTimer;
	double m_streamTimeOffset = 0.0f;
	double m_offsetCorrection = 0.0f;

	double m_deltaSum = 0.0f;
	int32 m_deltaSamples = 0;

	bool m_paused = false;
	bool m_playing = false;
	bool m_ended = false;

	float m_volume = 0.8f;

public:
	virtual bool Init(Audio* audio, const String& path, bool preload);
	void InitSampling(uint32 sampleRate);

	virtual void Play() override;
	virtual void Pause() override;
	virtual bool HasEnded() const override;
	uint64 SecondsToSamples(double s) const;
	double SamplesToSeconds(int64 s) const;
	double GetPositionSeconds(bool allowFreezeSkip = true) const;
	virtual int32 GetPosition() const override;
	virtual void SetPosition(int32 pos) override;
	virtual float* GetPCM() override;
	virtual uint32 GetSampleRate() const override;
	void RestartTiming();
	virtual void Process(float* out, uint32 numSamples) override;

	// Implementation specific set position
	virtual void SetPosition_Internal(int32 pos) = 0;
	virtual int32 GetStreamPosition_Internal() = 0;
	virtual float* GetPCM_Internal() = 0;
	virtual uint32 GetSampleRate_Internal() const = 0;
	// Internal sample rate
	virtual int32 GetStreamRate_Internal() = 0;
	// Implementation specific decode
	// return negative for end of stream or failure
	virtual int32 DecodeData_Internal() = 0;
};