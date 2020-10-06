#include "stdafx.h"
#include "AudioPlayback.hpp"
#include <Beatmap/BeatmapPlayback.hpp>
#include <Audio/DSP.hpp>
#include <Audio/Audio.hpp>

DSP* GameAudioEffect::CreateDSP(AudioPlayback& playback, uint32 sampleRate)
{
	DSP* ret = nullptr;

	const TimingPoint& tp = playback.GetBeatmapPlayback().GetCurrentTimingPoint();
	double noteDuration = tp.GetWholeNoteLength();

	float filterInput = playback.GetLaserFilterInput();
	uint32 actualLength = duration.Sample(filterInput).Absolute(noteDuration);
	uint32 maxLength = Math::Max(duration.Sample(0.f).Absolute(noteDuration), duration.Sample(1.f).Absolute(noteDuration));

	switch(type)
	{
	case EffectType::Bitcrush:
	{
		BitCrusherDSP* bcDSP = new BitCrusherDSP(sampleRate);
		bcDSP->SetPeriod((float)bitcrusher.reduction.Sample(filterInput));
		ret = bcDSP;
		break;
	}
	case EffectType::Echo:
	{
		EchoDSP* echoDSP = new EchoDSP(sampleRate);
		echoDSP->feedback = echo.feedback.Sample(filterInput) / 100.0f;
		echoDSP->SetLength(actualLength);
		ret = echoDSP;
		break;
	}
	case EffectType::PeakingFilter:
	case EffectType::LowPassFilter:
	case EffectType::HighPassFilter:
	{
		// Don't set anthing for biquad Filters
		BQFDSP* bqfDSP = new BQFDSP(sampleRate);
		ret = bqfDSP;
		break;
	}
	case EffectType::Gate:
	{
		GateDSP* gateDSP = new GateDSP(sampleRate);
		gateDSP->SetLength(actualLength);
		gateDSP->SetGating(gate.gate.Sample(filterInput));
		ret = gateDSP;
		break;
	}
	case EffectType::TapeStop:
	{
		TapeStopDSP* tapestopDSP = new TapeStopDSP(sampleRate);
		tapestopDSP->SetLength(actualLength);
		ret = tapestopDSP;
		break;
	}
	case EffectType::Retrigger:
	{
		RetriggerDSP* retriggerDSP = new RetriggerDSP(sampleRate);
		retriggerDSP->SetMaxLength(maxLength);
		retriggerDSP->SetLength(actualLength);
		retriggerDSP->SetGating(retrigger.gate.Sample(filterInput));
		retriggerDSP->SetResetDuration(retrigger.reset.Sample(filterInput).Absolute(noteDuration));
		ret = retriggerDSP;
		break;
	}
	case EffectType::Wobble:
	{
		WobbleDSP* wb = new WobbleDSP(sampleRate);
		wb->SetLength(actualLength);
		wb->q = wobble.q.Sample(filterInput);
		wb->fmax = wobble.max.Sample(filterInput);
		wb->fmin = wobble.min.Sample(filterInput);
		ret = wb;
		break;
	}
	case EffectType::Phaser:
	{
		PhaserDSP* phs = new PhaserDSP(sampleRate);
		phs->SetLength(actualLength);
		phs->dmin = phaser.min.Sample(filterInput);
		phs->dmax = phaser.max.Sample(filterInput);
		phs->fb = phaser.feedback.Sample(filterInput);
		ret = phs;
		break;
	}
	case EffectType::Flanger:
	{
		FlangerDSP* fl = new FlangerDSP(sampleRate);
		fl->SetLength(actualLength);
		fl->SetDelayRange(abs(flanger.offset.Sample(filterInput)),
			abs(flanger.depth.Sample(filterInput)));
		ret = fl;
		break;
	}
	case EffectType::SideChain:
	{
		SidechainDSP* sc = new SidechainDSP(sampleRate);
		sc->SetLength(actualLength);
		sc->amount = 1.0f;
		sc->curve = Interpolation::CubicBezier(0.39, 0.575, 0.565, 1);
		ret = sc;
		break;
	}
	case EffectType::PitchShift:
	{
		PitchShiftDSP* ps = new PitchShiftDSP(sampleRate);
		ps->amount = pitchshift.amount.Sample(filterInput);
		ret = ps;
		break;
	}
	default:
	break;
	}

	if(!ret)
	{
		Logf("Failed to create game audio effect for type \"%s\"", Logger::Severity::Warning, Enum_EffectType::ToString(type));
		return nullptr;
	}

	return ret;
}
void GameAudioEffect::SetParams(DSP* dsp, AudioPlayback& playback, HoldObjectState* object)
{
	const TimingPoint& tp = playback.GetBeatmapPlayback().GetCurrentTimingPoint();
	double noteDuration = tp.GetWholeNoteLength();

	switch(type)
	{
	case EffectType::Bitcrush:
	{
		BitCrusherDSP* bcDSP = (BitCrusherDSP*)dsp;
		bcDSP->SetPeriod((float)object->effectParams[0]);
		break;
	}
	case EffectType::Gate:
	{
		GateDSP* gateDSP = (GateDSP*)dsp;
		gateDSP->SetLength(noteDuration / object->effectParams[0]);
		gateDSP->SetGating(0.5f);
		break;
	}
	case EffectType::TapeStop:
	{
		TapeStopDSP* tapestopDSP = (TapeStopDSP*)dsp;
		tapestopDSP->SetLength((1000 * ((double)16 / Math::Max(object->effectParams[0], (int16)1))));
		break;
	}
	case EffectType::Retrigger:
	{
		RetriggerDSP* retriggerDSP = (RetriggerDSP*)dsp;
		retriggerDSP->SetLength(noteDuration / object->effectParams[0]);
		retriggerDSP->SetGating(0.65f);
		break;
	}
	case EffectType::Echo:
	{
		EchoDSP* echoDSP = (EchoDSP*)dsp;
		echoDSP->SetLength(noteDuration / object->effectParams[0]);
		echoDSP->feedback = object->effectParams[1] / 100.0f;
		break;
	}
	case EffectType::Wobble:
	{
		WobbleDSP* wb = (WobbleDSP*)dsp;
		wb->SetLength(noteDuration / object->effectParams[0]);
		break;
	}
	case EffectType::Phaser:
	{
		PhaserDSP* phs = (PhaserDSP*)dsp;
		phs->time = object->time;
		break;
	}
	case EffectType::Flanger:
	{
		FlangerDSP* fl = (FlangerDSP*)dsp;
		double delay = (noteDuration) / 1000.0;
		fl->SetDelayRange(10, 40);
		break;
	}
	case EffectType::PitchShift:
	{
		PitchShiftDSP* ps = (PitchShiftDSP*)dsp;
		ps->amount = (float)object->effectParams[0];
		break;
	}
	default:
	break;
	}
}
