#pragma once
#include "Audio.hpp"
#include "AudioStream.hpp"
#include "Audio_Impl.hpp"

class AudioStreamBase : public AudioStream
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
	BinaryStream& m_reader();

	mutex m_lock;

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
	void m_initSampling(uint32 sampleRate);
	uint64 m_secondsToSamples(double s) const;
	void m_restartTiming();
	double m_getPositionSeconds(bool allowFreezeSkip = true) const;

	// Implementation specific set position
	virtual void SetPosition_Internal(int32 pos) = 0;
	virtual int32 GetStreamPosition_Internal() = 0;
	virtual float* GetPCM_Internal() = 0;
	virtual uint32 GetSampleRate_Internal() const = 0;
	virtual uint64 GetSampleCount_Internal() const = 0;
	virtual void PreRenderDSPs_Internal(Vector<DSP*>& DSPs) {};
	// Internal sample rate
	virtual int32 GetStreamRate_Internal() = 0;
	// Implementation specific decode
	// return negative for end of stream or failure
	virtual int32 DecodeData_Internal() = 0;
	virtual bool Init(Audio* audio, const String& path, bool preload);

public:
	virtual void Play() override;
	virtual void Pause() override;
	virtual bool HasEnded() const override;
	double SamplesToSeconds(int64 s) const;
	virtual int32 GetPosition() const override;
	virtual uint64 GetSamplePos() const override;
	virtual void SetPosition(int32 pos) override;
	virtual float* GetPCM() override;
	virtual uint64 GetPCMCount() const override;
	virtual uint32 GetSampleRate() const override;
	virtual void PreRenderDSPs(Vector<DSP*>& DSPs) override;
	virtual void Process(float* out, uint32 numSamples) override;
};
