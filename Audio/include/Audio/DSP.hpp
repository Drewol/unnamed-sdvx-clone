/*
	This file contains DSP's that can be applied to audio samples of streams to modify the output
*/
#pragma once
#include "AudioBase.hpp"
#include <Shared/Interpolation.hpp>

// Biquad Filter
// Thanks to https://www.youtube.com/watch?v=FnpkBE4kJ6Q&list=WL&index=8 for the explanation
// Also http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt for the coefficient formulas
class BQF
{
public:
	float b0 = 1.0f;
	float b1 = 0.0f;
	float b2 = 0.0f;
	float a0 = 1.0f;
	float a1 = 0.0f;
	float a2 = 0.0f;

	void SetLowPass(float q, float freq, float sampleRate);
	void SetHighPass(float q, float freq, float sampleRate);
	void SetAllPass(float q, float freq, float sampleRate);
	void SetPeaking(float q, float freq, float gain, float sampleRate);
	void SetHighShelf(float q, float freq, float gain, float sampleRate);
	float Update(float in);
private:
	// FIR Delay buffers
	float zb[2]{};
	// IIR Delay buffers
	float za[2]{};
};

class PanDSP : public DSP
{
public:
	// -1 to 1 LR pan value
	float panning = 0.0f;
	virtual void Process(float* out, uint32 numSamples);
	virtual const char* GetName() const { return "PanDSP"; }
};

class BQFDSP : public DSP
{
public:
	BQFDSP(uint32 sampleRate);

	virtual void Process(float* out, uint32 numSamples);
	virtual const char* GetName() const { return "BQFDSP"; }

	// Sets the filter parameters
	void SetPeaking(float q, float freq, float gain);
	void SetLowPass(float q, float freq);
	void SetHighPass(float q, float freq);

private:
	BQF m_filters[2];
};

// Combinded Low/High-pass and Peaking filter
class CombinedFilterDSP : public DSP
{
public:
	CombinedFilterDSP(uint32 sampleRate);
	void SetLowPass(float q, float freq, float peakQ, float peakGain);
	void SetHighPass(float q, float freq, float peakQ, float peakGain);
	virtual const char* GetName() const { return "CombinedFilterDSP"; }

	virtual void Process(float* out, uint32 numSamples);

private:
	BQFDSP a;
	BQFDSP peak;
};

// Basic limiter
class LimiterDSP : public DSP
{
public:
	LimiterDSP(uint32 sampleRate);

	float releaseTime = 0.1f;
	virtual void Process(float* out, uint32 numSamples);
	virtual const char* GetName() const { return "LimiterDSP"; }

private:
	float m_currentMaxVolume = 1.0f;
	float m_currentReleaseTimer = releaseTime;
};

class BitCrusherDSP : public DSP
{
public:
	BitCrusherDSP(uint32 sampleRate);

	// Duration of samples, <1 = disable
	void SetPeriod(float period = 0);
	virtual void Process(float* out, uint32 numSamples);
	virtual const char* GetName() const { return "BitCrusherDSP"; }

private:
	uint32 m_period = 1;
	uint32 m_increment = 0;
	float m_sampleBuffer[2] = { 0.0f };
	uint32 m_currentDuration = 0;
};

class GateDSP : public DSP
{
public:
	GateDSP(uint32 sampleRate);

	// The amount of time for a single cycle in samples
	void SetLength(double length);
	void SetGating(float gating);

	// Low volume
	float low = 0.1f;

	virtual void Process(float* out, uint32 numSamples);
	virtual const char* GetName() const { return "GateDSP"; }

private:
	float m_gating = 0.5f;
	uint32 m_length = 0;
	uint32 m_fadeIn = 0;  // Fade In mark
	uint32 m_fadeOut = 0; // Fade Out mark
	uint32 m_halfway{};	  // Halfway mark
	uint32 m_currentSample = 0;
};

class TapeStopDSP : public DSP
{
public:
	TapeStopDSP(uint32 sampleRate);

	void SetLength(double length);

	virtual void Process(float* out, uint32 numSamples);
	virtual const char* GetName() const { return "TapeStopDSP"; }

private:
	uint32 m_length = 0;
	Vector<float> m_sampleBuffer;
	float m_sampleIdx = 0.0f;
	uint32 m_currentSample = 0;
};

class RetriggerDSP : public DSP
{
public:
	RetriggerDSP(uint32 sampleRate);

	void SetLength(double length);
	void SetResetDuration(uint32 resetDuration);
	void SetGating(float gating);
	void SetMaxLength(uint32 length);

	virtual void Process(float* out, uint32 numSamples);
	virtual const char* GetName() const { return "RetriggerDSP"; }

private:
	float m_gating = 0.75f;
	uint32 m_length = 0;
	uint32 m_gateLength = 0;
	uint32 m_resetDuration = 0;
	Vector<float> m_sampleBuffer;
	uint32 m_currentSample = 0;
	bool m_bufferReserved = false;
};

class WobbleDSP : public DSP
{
public:
	WobbleDSP(uint32 sampleRate);

	void SetLength(double length);

	// Frequency range
	float fmin = 500.0f;
	float fmax = 20000.0f;
	float q = 1.414f;

	virtual void Process(float* out, uint32 numSamples);
	virtual const char* GetName() const { return "WobbleDSP"; }

private:
	BQF m_filters[2];
	uint32 m_length{};
	uint32 m_currentSample = 0;
};

class PhaserDSP : public DSP
{
public:
	PhaserDSP(uint32 sampleRate);

	// Frequency range
	float fmin = 1500.0f;
	float fmax = 20000.0f;
	float q = 0.707f;
	float feedback = 0.35f;
	float stereoWidth = 0.0f;
	float hiCutGain = -8.0f;

	void SetLength(double length);
	void SetStage(uint32 stage);

	virtual void Process(float* out, uint32 numSamples);
	virtual const char* GetName() const { return "PhaserDSP"; }

private:
	uint32 m_length = 0;
	uint32 m_stage = 6;
	BQF m_apf[12][2];
	BQF m_hiShelf[2];
	float za[2] = { 0.0f };
	uint32 m_currentSample = 0;
};

class FlangerDSP : public DSP
{
public:
	FlangerDSP(uint32 sampleRate);

	void SetLength(double length);
	void SetDelayRange(uint32 min, uint32 max);
	void SetFeedback(float feedback);
	void SetStereoWidth(float stereoWidth);
	void SetVolume(float volume);

	virtual void Process(float* out, uint32 numSamples);
	virtual const char* GetName() const { return "FlangerDSP"; }

private:
	uint32 m_length = 0;

	// Delay range
	uint32 m_min = 0;
	uint32 m_max = 0;

	float m_feedback = 0.f;
	float m_stereoWidth = 0.f;
	float m_volume = 0.f;

	Vector<float> m_sampleBuffer;
	uint32 m_time = 0;
	uint32 m_bufferLength = 0;
	size_t m_bufferOffset = 0;
};

class EchoDSP : public DSP
{
public:
	EchoDSP(uint32 sampleRate);

	void SetLength(double length);

	float feedback = 0.6f;

	virtual void Process(float* out, uint32 numSamples);
	virtual const char* GetName() const { return "EchoDSP"; }

private:
	uint32 m_bufferLength = 0;
	size_t m_bufferOffset = 0;
	uint32 m_numLoops = 0;
	Vector<float> m_sampleBuffer;
};

class SidechainDSP : public DSP
{
public:
	SidechainDSP(uint32 sampleRate);

	void SetLength(double length);
	void SetAttackTime(double length);
	void SetHoldTime(double length);
	void SetReleaseTime(double length);

	float ratio = 5.0f;

	virtual void Process(float* out, uint32 numSamples);
	virtual const char* GetName() const { return "SidechainDSP"; }

private:
	uint32 m_length = 0;
	uint32 m_attackTime = 0;
	uint32 m_holdTime = 0;
	uint32 m_releaseTime = 0;
	size_t m_time = 0;
};

class PitchShiftDSP : public DSP
{
public:
	PitchShiftDSP(uint32 sampleRate);

	// Pitch change amount
	float amount = 0.0f;

	~PitchShiftDSP();

	virtual void Process(float* out, uint32 numSamples);
	virtual const char* GetName() const { return "PitchShiftDSP"; }

private:
	class PitchShiftDSP_Impl* m_impl;
};
