/*
	Contains a list of game specific audio effect types
*/
#pragma once
#include <Shared/Enum.hpp>
#include <Shared/Interpolation.hpp>

// The types of effects that can be used on the effect buttons and on lasers
DefineEnum(EffectType,
		   None = 0,
		   Retrigger,
		   Flanger,
		   Phaser,
		   Gate,
		   TapeStop,
		   Bitcrush,
		   Wobble,
		   SideChain,
		   Echo,
		   Panning,
		   PitchShift,
		   LowPassFilter,
		   HighPassFilter,
		   PeakingFilter,
		   SwitchAudio,			// Not a real effect
		   UserDefined0 = 0x40, // This ID or higher is user for user defined effects inside map objects
		   UserDefined1,		// Keep this ID at least a few ID's away from the normal effect so more native effects can be added later
		   UserDefined2,
		   UserDefined3,
		   UserDefined4,
		   UserDefined5,
		   UserDefined6,
		   UserDefined7,
		   UserDefined8,
		   UserDefined9 // etc...
		   )

	/*
	Effect parameter that is used to define a certain time range/period/speed
*/
	class EffectDuration
{
public:
	EffectDuration() = default;
	// Duration in milliseconds
	EffectDuration(int32 duration);
	// Duration relative to whole note duration
	EffectDuration(float rate);

	static EffectDuration Lerp(const EffectDuration &lhs, const EffectDuration &rhs, float time);

	// Convert to ms duration
	// pass in the whole note duration
	uint32 Absolute(double noteDuration);

	// Either a float or integer value
	union
	{
		float rate;
		int32 duration;
	};

	// The type of timing that the value represents
	enum Type : uint8
	{
		Rate, // Relative (1/4, 1/2, 0.5, etc), all relative to whole notes
		Time, // Absolute, in milliseconds
	};
	Type type;
};

template <typename T>
T InterpolateEffectParamValue(T a, T b, float t)
{
	return T(a + (b - a) * t);
}
EffectDuration InterpolateEffectParamValue(EffectDuration a, EffectDuration b, float t);

/*
	Effect parameter that allows all the values which can be set for effects
*/
template <typename T>
class EffectParam
{
public:
	EffectParam() = default;
	EffectParam(T value)
	{
		values[0] = value;
		values[1] = value;
		isRange = false;
		timeFunction = Interpolation::Linear;
	}
	EffectParam(T valueA, T valueB, Interpolation::TimeFunction timeFunction = Interpolation::Linear)
	{
		values[0] = valueA;
		values[1] = valueB;
		this->timeFunction = timeFunction;
		isRange = true;
	}

	// Sample based on laser input, or without parameters for just the actual value
	T Sample(float t = 0.0f) const
	{
		t = Math::Clamp(timeFunction(t), 0.0f, 1.0f);
		return isRange ? InterpolateEffectParamValue(values[0], values[1], t) : values[0];
	}

	Interpolation::TimeFunction timeFunction;

	// Either 1 or 2 values based on if this value should be interpolated by laser input or not
	T values[2];

	// When set to true, this means the parameter is a range
	bool isRange;
};

struct AudioEffect
{
	// Use this to get default effect settings
	static const AudioEffect &GetDefault(EffectType type);
	static int GetDefaultEffectPriority(EffectType type);

	void SetDefaultEffectParams(int16 *params);

	// The effect type
	EffectType type = EffectType::None;

	// Timing division for time based effects
	// Wobble:		length of single cycle
	// Phaser:		length of single cycle
	// Flanger:		length of single cycle
	// Tapestop:	duration of stop
	// Gate:		length of a single
	// Sidechain:	duration before reset
	// Echo:		delay
	EffectParam<EffectDuration> duration = EffectDuration(0.25f); // 1/4

	// How much of the effect is mixed in with the source audio
	EffectParam<float> mix = 0.0f;

	union
	{
		struct
		{
			// Amount of gating on this effect (0-1)
			EffectParam<float> gate;
			// Duration after which the retriggered sample area resets
			// 0 for no reset
			// TODO: This parameter allows this effect to be merged with gate
			EffectParam<EffectDuration> reset;
		} retrigger;
		struct
		{
			// Amount of gating on this effect (0-1)
			EffectParam<float> gate;
		} gate;
		struct
		{
			// Number of samples that is offset from the source audio to create the flanging effect (Samples)
			EffectParam<int32> offset;
			// Depth of the effect (samples)
			EffectParam<int32> depth;
			// Feedback (0-1)
			EffectParam<float> feedback;
			// Stereo width (0-1)
			EffectParam<float> stereoWidth;
			// Volume of added source audio + delayed source audio (0-1)
			EffectParam<float> volume;
		} flanger;
		struct
		{
			// Number of stages
			EffectParam<int32> stage;
			// Minimum frequency (Hz)
			EffectParam<float> min;
			// Maximum frequency (Hz)
			EffectParam<float> max;
			// Q factor (0-1)
			EffectParam<float> q;
			// Feedback (0-1)
			EffectParam<float> feedback;
			// Stereo width (0-1)
			EffectParam<float> stereoWidth;
			// High cut gain (dB)
			EffectParam<float> hiCutGain;
		} phaser;
		struct
		{
			// The duration in samples of how long a sample in the source audio gets reduced (creating square waves) (samples)
			EffectParam<int32> reduction;
		} bitcrusher;
		struct
		{
			// Top frequency of the wobble (Hz)
			EffectParam<float> max;
			// Bottom frequency of the wobble (Hz)
			EffectParam<float> min;
			// Q factor for filter (>0)
			EffectParam<float> q;
		} wobble;
		struct
		{
			// Time to fully reduce volume
			EffectParam<EffectDuration> attackTime;
			// Time to keep volume at maximum reduced value
			EffectParam<EffectDuration> holdTime;
			// Time to restore volume
			EffectParam<EffectDuration> releaseTime;
			// Ratio to reduce volume by
			EffectParam<float> ratio;
		} sidechain;
		struct
		{
			// Ammount of echo (0-1)
			EffectParam<float> feedback;
		} echo;
		struct
		{
			// Panning position, 0 is center (-1-1)
			EffectParam<float> panning;
		} panning;
		struct
		{
			// Pitch shift amount, in semitones
			EffectParam<float> amount;
		} pitchshift;
		struct
		{
			// Q factor for filter (>0)
			EffectParam<float> q;
			// Cuttoff frequency (Hz)
			EffectParam<float> freq;
		} lpf;
		struct
		{
			// Q factor for filter (>0)
			EffectParam<float> q;
			// Cuttoff frequency (Hz)
			EffectParam<float> freq;
		} hpf;
		struct
		{
			// Peak amplification (>=0)
			EffectParam<float> gain;
			// Q factor for filter (>0)
			EffectParam<float> q;
			// Cuttoff frequency (Hz)
			EffectParam<float> freq;
		} peaking;
		struct
		{
			// Audio Index
			EffectParam<int32> index;
		} switchaudio;
	};
};
