#include "stdafx.h"
#include "DSP.hpp"
#include "AudioOutput.hpp"
#include "Audio_Impl.hpp"
#include <Shared/Interpolation.hpp>

void PanDSP::Process(float* out, uint32 numSamples)
{
	for(uint32 i = 0; i < numSamples; i++)
	{
		if(panning > 0)
			out[i * 2 + 0] = (out[i * 2 + 0] * (1.0f - panning)) * mix + out[i * 2 + 0] * (1 - mix);
		if(panning < 0)
			out[i * 2 + 1] = (out[i * 2 + 1] * (1.0f + panning)) * mix + out[i * 2 + 1] * (1 - mix);
	}
}

BQFDSP::BQFDSP(uint32 sampleRate) : DSP()
{
	SetSampleRate(sampleRate);

	for (size_t i = 0; i < 2; i++)
	{
		for (size_t j = 0; j < order; j++)
		{
			za[i][j] = 0.f;
			zb[i][j] = 0.f;
		}
		
	}
	
}
void BQFDSP::Process(float* out, uint32 numSamples)
{
	for(uint32 c = 0; c < 2; c++)
	{
		for(uint32 i = 0; i < numSamples; i++)
		{
			float& sample = out[i * 2 + c];
			float src = sample;

			float filtered = 
				(b0 / a0) * src + 
				(b1 / a0) * zb[c][0] + 
				(b2 / a0) * zb[c][1] - 
				(a1 / a0) * za[c][0] - 
				(a2 / a0) * za[c][1];

			// Shift delay buffers
			zb[c][1] = zb[c][0];
			zb[c][0] = src;

			// Feedback the calculated value into the IIR delay buffers
			za[c][1] = za[c][0];
			za[c][0] = filtered;

			sample = filtered;
		}
	}
}
void BQFDSP::SetLowPass(float q, float freq, float sampleRate)
{
	// Limit q
	q = Math::Max(q, 0.01f);

	// Sampling frequency
	double w0 = (2 * Math::pi * freq) / sampleRate;
	double cw0 = cos(w0);
	float alpha = (float)(sin(w0) / (2 * q));

	b0 = (float)((1 - cw0) / 2);
	b1 = (float)(1 - cw0);
	b2 = (float)((1 - cw0) / 2);
	a0 = 1 + alpha;
	a1 = (float)(-2 * cw0);
	a2 = 1 - alpha;
}
void BQFDSP::SetLowPass(float q, float freq)
{
	SetLowPass(q, freq, (float) m_sampleRate);
}
void BQFDSP::SetHighPass(float q, float freq, float sampleRate)
{
	// Limit q
	q = Math::Max(q, 0.01f);

	assert(freq < sampleRate);
	double w0 = (2 * Math::pi * freq) / sampleRate;
	double cw0 = cos(w0);
	float alpha = (float)(sin(w0) / (2 * q));

	b0 = (float)((1 + cw0) / 2);
	b1 = (float)-(1 + cw0);
	b2 = float((1 + cw0) / 2);
	a0 = 1 + alpha;
	a1 = (float)(-2 * cw0);
	a2 = 1 - alpha;
}
void BQFDSP::SetHighPass(float q, float freq)
{
	SetHighPass(q, freq, (float) m_sampleRate);
}
void BQFDSP::SetPeaking(float q, float freq, float gain, float sampleRate)
{
	// Limit q
	q = Math::Max(q, 0.01f);

	double w0 = (2 * Math::pi * freq) / sampleRate;
	double cw0 = cos(w0);
	float alpha = (float)(sin(w0) / (2 * q));
	double A = pow(10, (gain / 40));

	b0 = 1 + (float)(alpha * A);
	b1 = -2 * (float)cw0;
	b2 = 1 - (float)(alpha*A);
	a0 = 1 + (float)(alpha / A);
	a1 = -2 * (float)cw0;
	a2 = 1 - (float)(alpha / A);
}
void BQFDSP::SetPeaking(float q, float freq, float gain)
{
	SetPeaking(q, freq, gain, (float) m_sampleRate);
}

CombinedFilterDSP::CombinedFilterDSP(uint32 sampleRate) : DSP(), a(sampleRate), peak(sampleRate)
{
	SetSampleRate(sampleRate);
}
void CombinedFilterDSP::SetLowPass(float q, float freq, float peakQ, float peakGain)
{
	a.SetLowPass(q, freq);
	peak.SetPeaking(peakQ, freq, peakGain);
}
void CombinedFilterDSP::SetHighPass(float q, float freq, float peakQ, float peakGain)
{
	a.SetHighPass(q, freq);
	peak.SetPeaking(peakQ, freq, peakGain);
}
void CombinedFilterDSP::Process(float* out, uint32 numSamples)
{
	a.mix = mix;
	peak.mix = mix;
	a.Process(out, numSamples);
	peak.Process(out, numSamples);
}

LimiterDSP::LimiterDSP(uint32 sampleRate) : DSP()
{
	SetSampleRate(sampleRate);
}
void LimiterDSP::Process(float* out, uint32 numSamples)
{
	const float secondsPerSample = (float) m_audio->GetSecondsPerSample();
	for(uint32 i = 0; i < numSamples; i++)
	{
		float currentGain = 1.0f;
		if(m_currentReleaseTimer < releaseTime)
		{
			float t = (1.0f - m_currentReleaseTimer / releaseTime);
			currentGain = (1.0f / m_currentMaxVolume) * t + (1.0f - t);
		}
	
		float maxVolume = Math::Max(abs(out[i*2]), abs(out[i * 2 + 1]));
		out[i * 2] *= currentGain * 0.9f;
		out[i * 2 + 1] *= currentGain * 0.9f;
	
		float currentMax = 1.0f / currentGain;
		if(maxVolume > currentMax)
		{
			m_currentMaxVolume = maxVolume;
			m_currentReleaseTimer = 0.0f;
		}
		else
		{
			m_currentReleaseTimer += secondsPerSample;
		}
	}
}

BitCrusherDSP::BitCrusherDSP(uint32 sampleRate) : DSP()
{
	SetSampleRate(sampleRate);
}
void BitCrusherDSP::SetPeriod(float period /*= 0*/)
{
	// Scale period with sample rate
	assert(m_sampleRate > 0);
	double f = m_sampleRate / 44100.0;

	m_increment = (uint32)((double)(1 << 16));
	m_period = (uint32)(f * period * (double)(1 << 16));
}
void BitCrusherDSP::Process(float* out, uint32 numSamples)
{
	for(uint32 i = 0; i < numSamples; i++)
	{
		m_currentDuration += m_increment;
		if(m_currentDuration > m_period)
		{
			m_sampleBuffer[0] = out[i * 2];
			m_sampleBuffer[1] = out[i*2+1];
			m_currentDuration -= m_period;
		}

		out[i * 2] = m_sampleBuffer[0] * mix + out[i * 2] * (1.0f - mix);
		out[i * 2 + 1] = m_sampleBuffer[1] * mix + out[i * 2+1] * (1.0f - mix);
	}
}

GateDSP::GateDSP(uint32 sampleRate) : DSP()
{
	SetSampleRate(sampleRate);
}
void GateDSP::SetLength(double length)
{
	double flength = length / 1000.0 * m_sampleRate;
	m_length = (uint32)flength;
	SetGating(m_gating);
}
void GateDSP::SetGating(float gating)
{
	float flength = (float)m_length;
	m_gating = gating;
	m_halfway = (uint32)(flength * gating);
	const float fadeDuration = Math::Min(0.05f, gating * 0.5f);
	m_fadeIn = (uint32)((float)m_halfway * fadeDuration);
	m_fadeOut = (uint32)((float)m_halfway * (1.0f - fadeDuration));
	m_currentSample = 0;
}
void GateDSP::Process(float* out, uint32 numSamples)
{
	if(m_length < 2)
		return;

	const uint32 startSample = GetStartSample();
	const uint32 currentSample = GetCurrentSample();

	for(uint32 i = 0; i < numSamples; i++)
	{
		if(currentSample + i < startSample)
		{
			continue;
		}
		float c = 1.0f;
		if(m_currentSample < m_halfway)
		{
			// Fade out before silence
			if(m_currentSample > m_fadeOut)
				c = 1-(float)(m_currentSample - m_fadeOut) / (float)m_fadeIn;
		}
		else
		{
			uint32 t = m_currentSample - m_halfway;
			// Fade in again
			if(t > m_fadeOut)
				c = (float)(t - m_fadeOut) / (float)m_fadeIn;
			else
				c = 0.0f;
		}

		// Multiply volume
		c = (c * (1 - low) + low); // Range [low, 1]
		c = c * mix + (1.0f-mix);
		out[i * 2] *= c;
		out[i * 2 + 1] *= c;

		m_currentSample++;
		m_currentSample %= m_length;
	}
}

TapeStopDSP::TapeStopDSP(uint32 sampleRate) : DSP()
{
	SetSampleRate(sampleRate);
}
void TapeStopDSP::SetLength(double length)
{
	assert(m_sampleRate > 0);
	double flength = length / 1000.0 * m_sampleRate;
	m_length = (uint32)flength;
	m_sampleBuffer.clear();
	m_sampleBuffer.reserve(2 * m_length + 100);
}
void TapeStopDSP::Process(float* out, uint32 numSamples)
{
	const uint32 startSample = GetStartSample();
	const uint32 currentSample = GetCurrentSample();

	for(uint32 i = 0; i < numSamples; i++)
	{
		if(currentSample + i < startSample)
		{
			continue;
		}
		float sampleRate = 1.0f - (float)m_currentSample / (float)m_length;
		if(sampleRate == 0.0f)
		{
			// Mute
			out[i * 2] = 0.0f;
			out[i * 2+1] = 0.0f;
			continue;
		}
		// Store samples for later
		m_sampleBuffer.Add(out[i*2]);
		m_sampleBuffer.Add(out[i*2+1]);

		// The sample index into the buffer
		uint32 i2 = (uint32)floor(m_sampleIdx);
		out[i * 2] = m_sampleBuffer[i2 * 2] * mix + out[i * 2] * (1 - mix);
		out[i * 2 + 1] = m_sampleBuffer[i2 * 2 + 1] * mix + out[i * 2+1] * (1 - mix);

		// Increase index
		m_sampleIdx += sampleRate;
		m_currentSample++;
	}
}

RetriggerDSP::RetriggerDSP(uint32 sampleRate) : DSP()
{
	SetSampleRate(sampleRate);
}
void RetriggerDSP::SetLength(double length)
{
	double flength = length / 1000.0 * m_sampleRate;
	m_length = (uint32)flength;
	SetGating(m_gating);
	if (!m_bufferReserved)
	{
		m_sampleBuffer.reserve(2 * m_length + 100);
		m_bufferReserved = true;
	}
}
void RetriggerDSP::SetResetDuration(uint32 resetDuration)
{
	float flength = (float)resetDuration / 1000.0f * (float) m_sampleRate;
	m_resetDuration = (uint32)flength;
}
void RetriggerDSP::SetGating(float gating)
{
	m_gating = gating;
	m_gateLength = (uint32)((float)m_length * gating);
}
void RetriggerDSP::SetMaxLength(uint32 length)
{
	float flength = (float)length / 1000.0f * (float) m_sampleRate;
	if (!m_bufferReserved)
	{
		m_sampleBuffer.reserve(2 * (uint32_t)flength + 100);
		m_bufferReserved = true;
	}
}
void RetriggerDSP::Process(float* out, uint32 numSamples)
{
	if (m_length == 0)
		return;

	const uint32 startSample = GetStartSample();
	const uint32 nowSample = GetCurrentSample();

	float* pcmSource = m_audioBase->GetPCM();
	double rateMult = (double)m_audioBase->GetSampleRate() / m_audio->GetSampleRate();
	uint32 pcmStartSample = static_cast<uint32>(lastTimingPoint * ((double)m_audioBase->GetSampleRate() / 1000.0));
	uint32 baseStartRepeat = static_cast<uint32>(lastTimingPoint * ((double)m_audio->GetSampleRate() / 1000.0));

	for(uint32 i = 0; i < numSamples; i++)
	{
		if(nowSample + i < startSample)
		{
			continue;
		}

		int startOffset = 0;
		if (m_resetDuration > 0)
		{
			startOffset = (nowSample + i - baseStartRepeat) / (int)m_resetDuration;
			startOffset = static_cast<int>(startOffset * m_resetDuration * rateMult);
		}
		else
		{
			startOffset = static_cast<int>((startSample - baseStartRepeat) * rateMult);
		}

		int pcmSample = static_cast<int>(pcmStartSample) + startOffset + static_cast<int>(m_currentSample * rateMult);
		float gating = 1.0f;
		if (m_currentSample > m_gateLength)
			gating = 0;
		// Sample from buffer
		out[i * 2] = gating * pcmSource[pcmSample * 2] * mix + out[i * 2] * (1 - mix);
		out[i * 2 + 1] = gating * pcmSource[pcmSample * 2 + 1] * mix + out[i * 2 + 1] * (1 - mix);
		
		// Increase index
		m_currentSample = (m_currentSample + 1) % m_length;

	}
}

WobbleDSP::WobbleDSP(uint32 sampleRate) : BQFDSP(sampleRate)
{
}
void WobbleDSP::SetLength(double length)
{
	double flength = length / 1000.0 * m_sampleRate;
	m_length = (uint32)flength;
}
void WobbleDSP::Process(float* out, uint32 numSamples)
{
	if (m_length == 0)
		return;

	static Interpolation::CubicBezier easing(Interpolation::EaseInExpo);
	const uint32 startSample = GetStartSample();
	const uint32 currentSample = GetCurrentSample();

	for(uint32 i = 0; i < numSamples; i++)
	{
		if(currentSample + i < startSample)
		{
			continue;
		}
		float f = abs(2.0f * ((float)m_currentSample / (float)m_length) - 1.0f);
		f = easing.Sample(f);
		float freq = fmin + (fmax - fmin) * f;
		SetLowPass(q, freq);

		float s[2] = { out[i * 2], out[i * 2 + 1] };

		BQFDSP::Process(&out[i * 2], 1);

		// Apply slight mixing
		float mix = 0.5f;
		out[i * 2 + 0] = out[i * 2 + 0] * mix + s[0] * (1.0f - mix);
		out[i * 2 + 1] = out[i * 2 + 1] * mix + s[1] * (1.0f - mix);

		m_currentSample++;
		m_currentSample %= m_length;
	}
}

PhaserDSP::PhaserDSP(uint32 sampleRate) : DSP()
{
	SetSampleRate(sampleRate);
}
void PhaserDSP::SetLength(double length)
{
	double flength = length / 1000.0 * m_sampleRate;
	m_length = (uint32)flength;
}
void PhaserDSP::Process(float* out, uint32 numSamples)
{
	if (m_length == 0)
		return;

	const uint32 startSample = GetStartSample();
	const uint32 currentSample = GetCurrentSample();

	for(uint32 i = 0; i < numSamples; i++)
	{
		if(currentSample + i < startSample)
		{
			continue;
		}
		//float f = ((float)time / (float)m_length) * Math::pi * 2.0f;
		float f = abs(2.0f * ((float)time / (float)m_length) - 1.0f);

		//calculate and update phaser sweep lfo...
		//float d = dmin + (dmax - dmin) * ((sin(f) + 1.0f) / 2.0f);
		float d = dmin + (dmax - dmin) * f;
		d /= (float) m_audio->GetSampleRate();

		//calculate output per channel
		for(uint32 c = 0; c < 2; c++)
		{
			APF* filters1 = filters[c];

			//update filter coeffs
			float a1 = (1.f - d) / (1.f + d);
			for(int i = 0; i < 6; i++)
				filters1[i].a1 = a1;

			float filtered = filters1[0].Update(
				filters1[1].Update(
					filters1[2].Update(
						filters1[3].Update(
							filters1[4].Update(
								filters1[5].Update(out[i * 2 + c] + za[c] * fb))))));
			// Store filter feedback
			za[c] = filtered;

			// Final sample
			out[i * 2 + c] = out[i * 2 + c] * (1.f - lmix) + filtered * mix * lmix;
		}

		time++;
		time %= m_length;
	}
}
float PhaserDSP::APF::Update(float in)
{
	float y = in * -a1 + za;
	za = y * a1 + in;
	return y;
}

FlangerDSP::FlangerDSP(uint32 sampleRate) : DSP()
{
	SetSampleRate(sampleRate);
}
void FlangerDSP::SetLength(double length) {
	double flength = length / 1000.0 * m_sampleRate;
	m_length = (uint32)flength;
}
void FlangerDSP::SetDelayRange(uint32 offset, uint32 depth)
{
	// Assuming 44100hz is the base sample rate
	const uint32 max = offset + depth;
	const uint32 min = offset;

	const float mult = (float) m_sampleRate / 44100.f;
	m_min = static_cast<uint32>(min * mult);
	m_max = static_cast<uint32>(max * mult);
	m_bufferLength = m_max * 2;
	m_sampleBuffer.resize(m_bufferLength);
}
void FlangerDSP::Process(float* out, uint32 numSamples)
{
	if (m_bufferLength <= 0)
		return;
	float* data = m_sampleBuffer.data();
	if (data == nullptr)
		return;

	const uint32 startSample = GetStartSample();
	const uint32 currentSample = GetCurrentSample();

	for(uint32 i = 0; i < numSamples; i++)
	{
		if (currentSample + i < startSample)
		{
			continue;
		}
		// Determine where we want to sample past samples
		float f = fmodf(((float)m_time / (float)m_length), 1.f);
		f = fabsf(f * 2 - 1);
		uint32 d = (uint32)(m_min + ((m_max - 1) - m_min) * (f));

		// TODO: clean up?
		int32 samplePos = ((int)m_bufferOffset - (int)d * 2) % (int)m_bufferLength;
		if (samplePos < 0)
			samplePos = m_bufferLength + samplePos;

		// Inject new sample
		data[m_bufferOffset + 0] = out[i * 2];
		data[m_bufferOffset + 1] = out[i * 2 + 1];

		// Apply delay
		out[i * 2] = (data[samplePos] + out[i * 2]) * 0.5f * mix +
			out[i * 2] * (1 - mix);
		out[i * 2 + 1] = (data[samplePos + 1] + out[i * 2 + 1]) * 0.5f * mix +
			out[i * 2 + 1] * (1 - mix);

		m_bufferOffset += 2;
		if (m_bufferOffset >= m_bufferLength)
			m_bufferOffset = 0;
		m_time++;
	}
}

EchoDSP::EchoDSP(uint32 sampleRate) : DSP()
{
	SetSampleRate(sampleRate);
}
void EchoDSP::SetLength(double length)
{
	double flength = length / 1000.0 * m_sampleRate;
	m_sampleBuffer.clear();
	m_bufferLength = (uint32)(flength * 2);
	m_sampleBuffer.resize(m_bufferLength);
	memset(m_sampleBuffer.data(), 0, sizeof(float) * m_bufferLength);
	m_numLoops = 0;
}
void EchoDSP::Process(float* out, uint32 numSamples)
{
	float* data = m_sampleBuffer.data();
	if (!data)
		return;

	const uint32 startSample = GetStartSample();
	const uint32 currentSample = GetCurrentSample();

	for(uint32 i = 0; i < numSamples; i++)
	{
		if(currentSample + i < startSample)
		{
			continue;
		}

		float l0 = data[m_bufferOffset + 0];
		float l1 = data[m_bufferOffset + 1];

		if(m_numLoops > 0)
		{
			// Send echo to output
			out[i * 2] = l0 * mix;
			out[i * 2 + 1] = l1 * mix;
		}

		// Inject new sample
		data[m_bufferOffset + 0] = out[i * 2] * feedback;
		data[m_bufferOffset + 1] = out[i * 2 + 1] * feedback;

		m_bufferOffset += 2;
		if(m_bufferOffset >= m_bufferLength)
		{
			m_bufferOffset = 0;
			m_numLoops++;
		}
	}
}

SidechainDSP::SidechainDSP(uint32 sampleRate) : DSP()
{
	SetSampleRate(sampleRate);
}
void SidechainDSP::SetLength(double length)
{
	double flength = length / 1000.0 * m_sampleRate;
	m_length = (uint32)flength;
	m_time = 0;
}
void SidechainDSP::Process(float* out, uint32 numSamples)
{
	if(m_length == 0)
		return;

	const uint32 startSample = GetStartSample();
	const uint32 currentSample = GetCurrentSample();

	for(uint32 i = 0; i < numSamples; i++)
	{
		if(currentSample + i < startSample)
		{
			continue;
		}
		float r = (float)m_time / (float)m_length;
		// FadeIn
		const float fadeIn = 0.08f;
		if(r < fadeIn)
			r = 1.0f - r / fadeIn;
		else
			r = curve((r- fadeIn) / (1.0f- fadeIn));
		float sampleGain = 1.0f - amount * (1.0f- r);
		out[i * 2 + 0] *= sampleGain;
		out[i * 2 + 1] *= sampleGain;
		if(++m_time > m_length)
		{
			m_time = 0;
		}
	}
}

#include "SoundTouch.h"
using namespace soundtouch;

class PitchShiftDSP_Impl
{
public:
	float pitch = 0.0f;
	bool init = false;

private:
	SoundTouch m_soundtouch;
	Vector<float> m_receiveBuffer;

public:
	PitchShiftDSP_Impl()
	{
	}
	~PitchShiftDSP_Impl()
	{
	}
	void Init(Audio_Impl* audio)
	{
		m_soundtouch.setChannels(2);
		m_soundtouch.setSampleRate(audio->GetSampleRate());
		m_soundtouch.setSetting(SETTING_USE_AA_FILTER, 0);
		m_soundtouch.setSetting(SETTING_SEQUENCE_MS, 5);
		//m_soundtouch.setSetting(SETTING_SEEKWINDOW_MS, 10);
		//m_soundtouch.setSetting(SETTING_OVERLAP_MS, 10);
	}
	void Process(float* out, uint32 numSamples)
	{
		m_receiveBuffer.resize(numSamples*2);
		m_soundtouch.setPitchSemiTones(pitch);
		m_soundtouch.putSamples(out, numSamples);
		uint32 receivedSamples = m_soundtouch.receiveSamples(m_receiveBuffer.data(), numSamples);
		if(receivedSamples > 0)
		{
			memcpy(out, m_receiveBuffer.data(), receivedSamples * sizeof(float) * 2);
		}
	}
};

PitchShiftDSP::PitchShiftDSP(uint32 sampleRate) : DSP()
{
	SetSampleRate(sampleRate);
	m_impl = new PitchShiftDSP_Impl();
}
PitchShiftDSP::~PitchShiftDSP()
{
	delete m_impl;
}
void PitchShiftDSP::Process(float* out, uint32 numSamples)
{
	m_impl->pitch = Math::Clamp(amount, -12.0f, 12.0f);
	if(!m_impl->init)
		m_impl->Init(m_audio);
	m_impl->Process(out, numSamples);
}
