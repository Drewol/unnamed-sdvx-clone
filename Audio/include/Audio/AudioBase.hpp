#pragma once

/*
	Base class for Digital Signal Processors
*/
class DSP
{
protected:
	DSP() = default; // Abstract
	DSP(const DSP&) = delete;

	uint32 GetStartSample() const;
	uint32 GetCurrentSample() const;

	// Smpling rate of m_audio (not m_audioBase)
	// Only use this for initializing parameters
	uint32 m_sampleRate = 0;

	class AudioBase* m_audioBase = nullptr;

public:
	virtual ~DSP();
	static bool Sorter(DSP*& a, DSP*& b);

	void SetAudioBase(class AudioBase* audioBase);
	inline void RemoveAudioBase() { m_audioBase = nullptr; }
	inline void SetSampleRate(uint32 sampleRate) { m_sampleRate = sampleRate; }

	// Process <numSamples> amount of samples in stereo float format
	virtual void Process(float* out, uint32 numSamples) = 0;
	virtual const char* GetName() const = 0;

	float mix = 1.0f;
	uint32 priority = 0;
	uint32 startTime = 0;
	uint32 endTime = 0;
	int32 chartOffset = 0;
	int32 lastTimingPoint = 0;
};

/*
	Base class for things that generate sound
*/
class AudioBase
{
public:
	virtual ~AudioBase();
	// Process <numSamples> amount of samples in stereo float format
	virtual void Process(float* out, uint32 numSamples) = 0;

	// Gets the playback position in millisecond
	virtual int32 GetPosition() const = 0;

	// Get the sample rate of this audio stream
	virtual uint32 GetSampleRate() const = 0;

	// Get the exact playback position in samples
	virtual uint64 GetSamplePos() const = 0;

	// Get the sample rate of the audio connected to this
	uint32 GetAudioSampleRate() const;

	// Gets pcm data from a decoded stream, nullptr if not available
	virtual float* GetPCM() = 0;
	// Gets pcm sample count
	virtual uint64 GetPCMCount() const = 0;
	virtual void PreRenderDSPs(Vector<DSP*>& DSPs) = 0;

	void ProcessDSPs(float* out, uint32 numSamples);
	// Adds a signal processor to the audio
	void AddDSP(DSP* dsp);
	// Removes a signal processor from the audio
	void RemoveDSP(DSP* dsp);

	void Deregister();

	// Stream volume from 0-1
	void SetVolume(float volume)
	{
		m_volume = volume;
	}
	float GetVolume() const
	{
		return m_volume;
	}

	Vector<DSP*> DSPs;
	float PlaybackSpeed = 1.0;
	class Audio_Impl* audio = nullptr;

private:
	float m_volume = 1.0f;
};
