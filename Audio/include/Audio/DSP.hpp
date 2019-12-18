/*
	This file contains DSP's that can be applied to audio samples of streams to modify the output
*/
#pragma once
#include "AudioBase.hpp"
#include <Shared/Interpolation.hpp>

class PanDSP : public DSP
{
public:
	// -1 to 1 LR pan value
	float panning = 0.0f;
	virtual void Process(float* out, uint32 numSamples);
};

// Biquad Filter
// Thanks to https://www.youtube.com/watch?v=FnpkBE4kJ6Q&list=WL&index=8 for the explanation
// Also http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt for the coefficient formulas
class BQFDSP : public DSP
{
public:
	float b0 = 1.0f;
	float b1 = 0.0f;
	float b2 = 0.0f;
	float a0 = 1.0f;
	float a1 = 0.0f;
	float a2 = 0.0f;

	virtual void Process(float* out, uint32 numSamples);

	// Sets the filter parameters
	void SetPeaking(float q, float freq, float gain);
	void SetLowPass(float q, float freq);
	void SetHighPass(float q, float freq);

	void SetPeaking(float q, float freq, float gain, float sampleRate);
	void SetLowPass(float q, float freq, float sampleRate);
	void SetHighPass(float q, float freq, float sampleRate);
private:
	// Delayed samples
	static const uint32 order = 2;
	// FIR Delay buffers
	float zb[2][order];
	// IIR Delay buffers
	float za[2][order];
};

// Combinded Low/High-pass and Peaking filter
class CombinedFilterDSP : public DSP
{
public:
	void SetLowPass(float q, float freq, float peakQ, float peakGain);
	void SetHighPass(float q, float freq, float peakQ, float peakGain);

	virtual void Process(float* out, uint32 numSamples);
private:
	BQFDSP a;
	BQFDSP peak;
};

// Basic limiter
class LimiterDSP : public DSP
{
public:
	float releaseTime = 0.1f;
	virtual void Process(float* out, uint32 numSamples);
private:
	float m_currentMaxVolume = 1.0f;
	float m_currentReleaseTimer = releaseTime;
};

class BitCrusherDSP : public DSP
{
public:
	// Duration of samples, <1 = disable
	void SetPeriod(float period = 0);
	virtual void Process(float* out, uint32 numSamples);
private:
	uint32 m_period = 1;
	uint32 m_increment = 0;
	float m_sampleBuffer[2] = { 0.0f };
	uint32 m_currentDuration = 0;
};

class GateDSP : public DSP
{
public:
	// The amount of time for a single cycle in samples
	void SetLength(double length);
	void SetGating(float gating);

	// Low volume
	float low = 0.1f;

	virtual void Process(float* out, uint32 numSamples);
private:
	float m_gating = 0.5f;
	uint32 m_length = 0;
	uint32 m_fadeIn = 0; // Fade In mark
	uint32 m_fadeOut = 0; // Fade Out mark
	uint32 m_halfway; // Halfway mark
	uint32 m_currentSample = 0;
};

class TapeStopDSP : public DSP
{
public:
	void SetLength(double length);

	virtual void Process(float* out, uint32 numSamples);
private:
	uint32 m_length = 0;
	Vector<float> m_sampleBuffer;
	float m_sampleIdx = 0.0f;
	uint32 m_lastSample = 0;
	uint32 m_currentSample = 0;
};

class RetriggerDSP : public DSP
{
public:
	void SetLength(double length);
	void SetResetDuration(uint32 resetDuration);
	void SetGating(float gating);
	void SetMaxLength(uint32 length);

	virtual void Process(float* out, uint32 numSamples);
private:
	float m_gating = 0.75f;
	uint32 m_length = 0;
	uint32 m_gateLength = 0;
	uint32 m_resetDuration = 0;
	Vector<float> m_sampleBuffer;
	uint32 m_loops = 0;
	uint32 m_currentSample = 0;
	bool m_bufferReserved = false;
};

class WobbleDSP : public BQFDSP
{
public:
	void SetLength(double length);

	// Frequency range
	float fmin = 500.0f;
	float fmax = 20000.0f;
	float q = 1.414f;

	virtual void Process(float* out, uint32 numSamples);
private:
	uint32 m_length;
	uint32 m_currentSample = 0;
};

// Referenced http://www.musicdsp.org/files/phaser.cpp
class PhaserDSP : public DSP
{
public:
	uint32 time = 0;

	// Frequency range
	float dmin = 1000.0f;
	float dmax = 4000.0f;
	float fb = 0.2f; //feedback
	float lmix = 0.33f; //local mix

	void SetLength(double length);

	virtual void Process(float* out, uint32 numSamples);

private:
	uint32 m_length = 0;

	// All pass filter
	struct APF
	{
		float Update(float in);
		float a1 = 0.0f;
		float za = 0.0f;
	};

	APF filters[2][6];
	float za[2] = { 0.0f };
};

class FlangerDSP : public DSP
{
public:
	void SetLength(double length);
	void SetDelayRange(uint32 min, uint32 max);

	virtual void Process(float* out, uint32 numSamples);
private:
	uint32 m_length = 0;

	// Delay range
	uint32 m_min = 0;
	uint32 m_max = 0;

	Vector<float> m_sampleBuffer;
	uint32 m_time = 0;
	uint32 m_bufferLength = 0;
	size_t m_bufferOffset = 0;
};

class EchoDSP : public DSP
{
public:
	void SetLength(double length);

	float feedback = 0.6f;

	virtual void Process(float* out, uint32 numSamples);
private:
	uint32 m_bufferLength = 0;
	size_t m_bufferOffset = 0;
	uint32 m_numLoops = 0;
	Vector<float> m_sampleBuffer;
};


class SidechainDSP : public DSP
{
public:
	// Set sidechain length in samples
	void SetLength(double length);

	// Volume multiplier for the sidechaing
	float amount = 0.25f;

	Interpolation::CubicBezier curve;

	virtual void Process(float* out, uint32 numSamples);
private:
	uint32 m_length = 0;
	size_t m_time = 0;
};

class PitchShiftDSP : public DSP
{
public:
	// Pitch change amount
	float amount = 0.0f;

	PitchShiftDSP();
	~PitchShiftDSP();

	virtual void Process(float* out, uint32 numSamples);
private:
	class PitchShiftDSP_Impl* m_impl;
};