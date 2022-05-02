#include "stdafx.h"
#include "Beatmap.hpp"
#include "KShootMap.hpp"

// Temporary object to keep track if a button is a hold button
struct TempButtonState
{
	TempButtonState(uint32 startTick)
		: startTick(startTick)
	{
	}
	uint32 startTick;
	uint32 numTicks = 0;
	EffectType effectType = EffectType::None;
	uint16 effectParams[2] = {0};
	// If using the smalles grid to indicate hold note duration
	bool fineSnap = false;
	// Set for hold continuations, this is where there is a hold right after an existing one but with different effects
	HoldObjectState *lastHoldObject = nullptr;

	uint8 sampleIndex = 0xFF;
	bool usingSample = false;
	float sampleVolume = 1.0f;
};
struct TempLaserState
{
	TempLaserState(uint32 startTick, uint32 absoluteStartTick, uint32 effectType)
		: startTick(startTick), effectType(effectType), absoluteStartTick(absoluteStartTick)
	{
	}
	uint32 startTick;
	uint32 absoluteStartTick;
	uint32 numTicks = 0;
	uint32 effectType = 0;
	bool spinIsBounce = false;
	char spinType = 0;
	uint32 spinDuration = 0;
	uint32 spinBounceAmplitude = 0;
	uint32 spinBounceFrequency = 0;
	uint32 spinBounceDecay = 0;
	uint8 effectParams = 0;
	float startPosition; // Entry position
	// Previous segment
	LaserObjectState *last = nullptr;
};

class EffectTypeMap
{
	// Custom effect types (1.60)
	uint16 m_customEffectTypeID = (uint16)EffectType::UserDefined0;

public:
	EffectTypeMap()
	{
		// Add common effect types
		effectTypes["None"] = EffectType::None;
		effectTypes["Retrigger"] = EffectType::Retrigger;
		effectTypes["Flanger"] = EffectType::Flanger;
		effectTypes["Phaser"] = EffectType::Phaser;
		effectTypes["Gate"] = EffectType::Gate;
		effectTypes["TapeStop"] = EffectType::TapeStop;
		effectTypes["BitCrusher"] = EffectType::Bitcrush;
		effectTypes["Wobble"] = EffectType::Wobble;
		effectTypes["SideChain"] = EffectType::SideChain;
		effectTypes["Echo"] = EffectType::Echo;
		effectTypes["Panning"] = EffectType::Panning;
		effectTypes["PitchShift"] = EffectType::PitchShift;
		effectTypes["LPF"] = EffectType::LowPassFilter;
		effectTypes["HPF"] = EffectType::HighPassFilter;
		effectTypes["PEAK"] = EffectType::PeakingFilter;
		effectTypes["SwitchAudio"] = EffectType::SwitchAudio;
	}

	// Only checks if a mapping exists and returns this, or None
	const EffectType *FindEffectType(const String &name) const
	{
		return effectTypes.Find(name);
	}

	// Adds or returns the enum value mapping to this effect
	EffectType FindOrAddEffectType(const String &name)
	{
		EffectType *id = effectTypes.Find(name);
		if (!id)
			return effectTypes.Add(name, (EffectType)m_customEffectTypeID++);
		return *id;
	};

	Map<String, EffectType> effectTypes;
};

template <typename T>
void AssignAudioEffectParameter(EffectParam<T> &param, const String &paramName, Map<String, float> &floatParams, Map<String, int> &intParams)
{
	float *fval = floatParams.Find(paramName);
	if (fval)
	{
		param = *fval;
		return;
	}
	int32 *ival = intParams.Find(paramName);
	if (ival)
	{
		param = *ival;
		return;
	}
}

struct MultiParam
{
	enum Type
	{
		Float,
		Samples,
		Milliseconds,
		Int,
	};
	Type type;
	union {
		float fval;
		int32 ival;
	};
};
struct MultiParamRange
{
	MultiParamRange() = default;
	MultiParamRange(const MultiParam &a)
	{
		params[0] = a;
	}
	MultiParamRange(const MultiParam &a, const MultiParam &b)
	{
		params[0] = a;
		params[1] = b;
		isRange = true;
	}
	EffectParam<float> ToFloatParam()
	{
		auto r = params[0].type == MultiParam::Float ? EffectParam<float>(params[0].fval, params[1].fval) : EffectParam<float>((float)params[0].ival, (float)params[1].ival);
		r.isRange = isRange;
		return r;
	}
	EffectParam<EffectDuration> ToDurationParam()
	{
		EffectParam<EffectDuration> r;
		if (params[0].type == MultiParam::Milliseconds)
		{
			r = EffectParam<EffectDuration>(params[0].ival, params[1].ival);
		}
		else if (params[0].type == MultiParam::Float)
		{
			r = EffectParam<EffectDuration>(params[0].fval, params[1].fval);
		}
		else
		{
			r = EffectParam<EffectDuration>((float)params[0].ival, (float)params[1].ival);
		}
		r.isRange = isRange;
		return r;
	}
	EffectParam<int32> ToSamplesParam()
	{
		EffectParam<int32> r;
		if (params[0].type == MultiParam::Int || params[0].type == MultiParam::Samples)
			r = EffectParam<int32>(params[0].ival, params[1].ival);
		r.isRange = isRange;
		return r;
	}
	MultiParam params[2];
	bool isRange = false;
};
static MultiParam ParseParam(const String &in)
{
	MultiParam ret;
	if (in.find('/') != -1)
	{
		ret.type = MultiParam::Float;
		String a, b;
		in.Split("/", &a, &b);
		ret.fval = (float)(atof(*a) / atof(*b));
	}
	else if (in.find("samples") != -1)
	{
		ret.type = MultiParam::Samples;
		sscanf(*in, "%i", &ret.ival);
	}
	else if (in.find("ms") != -1)
	{
		ret.type = MultiParam::Milliseconds;
		float milliseconds = 0;
		sscanf(*in, "%f", &milliseconds);
		ret.ival = (int)(milliseconds);
	}
	else if (in.find("s") != -1)
	{
		ret.type = MultiParam::Milliseconds;
		float seconds = 0;
		sscanf(*in, "%f", &seconds);
		ret.ival = (int)(seconds * 1000.0);
	}
	else if (in.find("%") != -1)
	{
		ret.type = MultiParam::Float;
		int percentage = 0;
		sscanf(*in, "%i", &percentage);
		ret.fval = percentage / 100.0f;
	}
	else if (in.find('.') != -1)
	{
		ret.type = MultiParam::Float;
		sscanf(*in, "%f", &ret.fval);
	}
	else
	{
		ret.type = MultiParam::Int;
		sscanf(*in, "%i", &ret.ival);
	}
	return ret;
}
AudioEffect ParseCustomEffect(const KShootEffectDefinition &def, Vector<String> &switchablePaths)
{
	static EffectTypeMap defaultEffects;
	AudioEffect effect;
	bool typeSet = false;

	Map<String, MultiParamRange> params;
	for (const auto& s : def.parameters)
	{
		// This one is easy
		if (s.first == "type")
		{
			// Get the default effect for this name
			const EffectType *type = defaultEffects.FindEffectType(s.second);
			if (!type)
			{
				Logf("Unknown base effect type for custom effect type: %s", Logger::Severity::Warning, s.second);
				continue;
			}
			effect = AudioEffect::GetDefault(*type);
			typeSet = true;
		}
		else
		{
			// Special case for SwitchAudio effect
			if (s.first == "fileName")
			{
				MultiParam switchableIndex;
				switchableIndex.type = MultiParam::Int;

				auto it = std::find(switchablePaths.begin(), switchablePaths.end(), s.second);
				if (it == switchablePaths.end())
				{
					switchableIndex.ival = static_cast<int32>(switchablePaths.size());
					switchablePaths.Add(s.second);
				}
				else
				{
					switchableIndex.ival = static_cast<int32>(std::distance(switchablePaths.begin(), it));
				}

				params.Add("index", switchableIndex);
				continue;
			}

			size_t splitArrow = s.second.find('>', 1);
			String param;
			if (splitArrow != -1)
			{
				param = s.second.substr(splitArrow + 1);
			}
			else
			{
				param = s.second;
			}
			size_t split = param.find('-', 1);
			if (split != -1)
			{
				String a, b;
				a = param.substr(0, split);
				b = param.substr(split + 1);

				MultiParamRange pr = {ParseParam(a), ParseParam(b)};
				if (pr.params[0].type != pr.params[1].type)
				{
					Logf("Non matching parameters types \"[%s, %s]\" for key: %s", Logger::Severity::Warning, s.first, param, s.first);
					continue;
				}
				params.Add(s.first, pr);
			}
			else
			{
				params.Add(s.first, ParseParam(param));
			}
		}
	}

	if (!typeSet)
	{
		Logf("Type not set for custom effect type: %s", Logger::Severity::Warning, def.typeName);
		return effect;
	}

	auto AssignFloatIfSet = [&](EffectParam<float> &target, const String &name) {
		auto *param = params.Find(name);
		if (param)
		{
			target = param->ToFloatParam();
		}
	};
	auto AssignDurationIfSet = [&](EffectParam<EffectDuration> &target, const String &name) {
		auto *param = params.Find(name);
		if (param)
		{
			target = param->ToDurationParam();
		}
	};
	auto AssignSamplesIfSet = [&](EffectParam<int32> &target, const String &name) {
		auto *param = params.Find(name);
		if (param && param->params[0].type == MultiParam::Samples)
		{
			target = param->ToSamplesParam();
		}
	};
	auto AssignIntIfSet = [&](EffectParam<int32> &target, const String &name) {
		auto *param = params.Find(name);
		if (param)
		{
			target = param->ToSamplesParam();
		}
	};

	AssignFloatIfSet(effect.mix, "mix");

	// Set individual parameters per effect based on if they are specified or not
	// if they are not set the defaults will be kept (as aquired above)
	switch (effect.type)
	{
	case EffectType::PitchShift:
		AssignFloatIfSet(effect.pitchshift.amount, "pitch");
		break;
	case EffectType::Bitcrush:
		AssignSamplesIfSet(effect.bitcrusher.reduction, "amount");
		break;
	case EffectType::Echo:
		AssignDurationIfSet(effect.duration, "waveLength");
		AssignFloatIfSet(effect.echo.feedback, "feedbackLevel");
		break;
	case EffectType::Phaser:
		AssignDurationIfSet(effect.duration, "period");
		AssignIntIfSet(effect.phaser.stage, "stage");
		AssignFloatIfSet(effect.phaser.min, "loFreq");
		AssignFloatIfSet(effect.phaser.max, "hiFreq");
		AssignFloatIfSet(effect.phaser.q, "Q");
		AssignFloatIfSet(effect.phaser.feedback, "feedback");
		AssignFloatIfSet(effect.phaser.stereoWidth, "stereoWidth");
		AssignFloatIfSet(effect.phaser.hiCutGain, "hiCutGain");
		break;
	case EffectType::Flanger:
		AssignDurationIfSet(effect.duration, "period");
		AssignIntIfSet(effect.flanger.depth, "depth");
		AssignIntIfSet(effect.flanger.offset, "delay");
		AssignFloatIfSet(effect.flanger.feedback, "feedback");
		AssignFloatIfSet(effect.flanger.stereoWidth, "stereoWidth");
		AssignFloatIfSet(effect.flanger.volume, "volume");
		break;
	case EffectType::Gate:
		AssignDurationIfSet(effect.duration, "waveLength");
		AssignFloatIfSet(effect.gate.gate, "rate");
		break;
	case EffectType::Retrigger:
		AssignDurationIfSet(effect.duration, "waveLength");
		AssignFloatIfSet(effect.retrigger.gate, "rate");
		AssignDurationIfSet(effect.retrigger.reset, "updatePeriod");
		break;
	case EffectType::Wobble:
		AssignDurationIfSet(effect.duration, "waveLength");
		AssignFloatIfSet(effect.wobble.min, "loFreq");
		AssignFloatIfSet(effect.wobble.max, "hiFreq");
		AssignFloatIfSet(effect.wobble.q, "Q");
		break;
	case EffectType::SideChain:
		AssignDurationIfSet(effect.duration, "period");
		AssignDurationIfSet(effect.sidechain.attackTime, "attackTime");
		AssignDurationIfSet(effect.sidechain.holdTime, "holdTime");
		AssignDurationIfSet(effect.sidechain.releaseTime, "releaseTime");
		AssignFloatIfSet(effect.sidechain.ratio, "ratio");
		break;
	case EffectType::TapeStop:
		AssignDurationIfSet(effect.duration, "speed");
		break;
	case EffectType::SwitchAudio:
		AssignIntIfSet(effect.switchaudio.index, "index");
		break;
	}

	return effect;
};

bool Beatmap::m_ProcessKShootMap(BinaryStream &input, bool metadataOnly)
{
	KShootMap kshootMap;
	if (!kshootMap.Init(input, metadataOnly))
		return false;

	EffectTypeMap effectTypeMap;
	EffectTypeMap filterTypeMap;
	Map<EffectType, int16> defaultEffectParams;

	// Set defaults
	{
		defaultEffectParams[EffectType::Bitcrush] = 4;
		defaultEffectParams[EffectType::Gate] = 8;
		defaultEffectParams[EffectType::Retrigger] = 8;
		defaultEffectParams[EffectType::Phaser] = 2000;
		defaultEffectParams[EffectType::Flanger] = 2000;
		defaultEffectParams[EffectType::Wobble] = 12;
		defaultEffectParams[EffectType::SideChain] = 8;
		defaultEffectParams[EffectType::TapeStop] = 50;
	}

	// Add all the custom effect types
	for (auto it = kshootMap.fxDefines.begin(); it != kshootMap.fxDefines.end(); it++)
	{
		EffectType type = effectTypeMap.FindOrAddEffectType(it->first);
		if (m_customAudioEffects.Contains(type))
			continue;
		m_customAudioEffects.Add(type, ParseCustomEffect(it->second, m_switchablePaths));
	}
	for (auto it = kshootMap.filterDefines.begin(); it != kshootMap.filterDefines.end(); it++)
	{
		EffectType type = filterTypeMap.FindOrAddEffectType(it->first);
		if (m_customAudioFilters.Contains(type))
			continue;
		m_customAudioFilters.Add(type, ParseCustomEffect(it->second, m_switchablePaths));
	}

	auto ParseFilterType = [&](const String &str) {
		EffectType type = EffectType::None;
		if (str == "hpf1")
		{
			type = EffectType::HighPassFilter;
		}
		else if (str == "lpf1")
		{
			type = EffectType::LowPassFilter;
		}
		else if (str == "fx;bitc" || str == "bitc")
		{
			type = EffectType::Bitcrush;
		}
		else if (str == "peak")
		{
			type = EffectType::PeakingFilter;
		}
		else
		{
			const EffectType *foundType = filterTypeMap.FindEffectType(str);
			if (foundType)
				type = *foundType;
			else
				Logf("[KSH]Unknown filter type: %s", Logger::Severity::Warning, str);
		}
		return type;
	};

	// Process map settings
	m_settings.previewOffset = 0;
	m_settings.previewDuration = 0;
	for (auto &s : kshootMap.settings)
	{
		if (s.first == "title")
			m_settings.title = s.second;
		else if (s.first == "artist")
			m_settings.artist = s.second;
		else if (s.first == "effect")
			m_settings.effector = s.second;
		else if (s.first == "illustrator")
			m_settings.illustrator = s.second;
		else if (s.first == "t")
			m_settings.bpm = s.second;
		else if (s.first == "jacket")
			m_settings.jacketPath = s.second;
		else if (s.first == "bg")
			m_settings.backgroundPath = s.second;
		else if (s.first == "layer")
			m_settings.foregroundPath = s.second;
		else if (s.first == "m")
		{
			if (s.second.find(';') != -1)
			{
				String audioFX, audioNoFX;
				s.second.Split(";", &audioNoFX, &audioFX);
				size_t splitMore = audioFX.find(';');
				if (splitMore != -1)
					audioFX = audioFX.substr(0, splitMore);
				m_settings.audioFX = audioFX;
				m_settings.audioNoFX = audioNoFX;
			}
			else
			{
				m_settings.audioNoFX = s.second;
			}
		}
		else if (s.first == "o")
		{
			m_settings.offset = atol(*s.second);
		}
		// TODO: Move initial laser effect settings to an event instead
		else if (s.first == "filtertype")
		{
			m_settings.laserEffectType = ParseFilterType(s.second);
		}
		else if (s.first == "pfiltergain")
		{
			m_settings.laserEffectMix = (float)atol(*s.second) / 100.0f;
		}
		else if (s.first == "chokkakuvol")
		{
			m_settings.slamVolume = (float)atol(*s.second) / 100.0f;
		}
		// end TODO
		else if (s.first == "level")
		{
			m_settings.level = atoi(*s.second);
		}
		else if (s.first == "difficulty")
		{
			m_settings.difficulty = 0;
			if (s.second == "challenge")
			{
				m_settings.difficulty = 1;
			}
			else if (s.second == "extended")
			{
				m_settings.difficulty = 2;
			}
			else if (s.second == "infinite")
			{
				m_settings.difficulty = 3;
			}
		}
		else if (s.first == "po")
		{
			m_settings.previewOffset = atoi(*s.second);
		}
		else if (s.first == "plength")
		{
			m_settings.previewDuration = atoi(*s.second);
		}
		else if (s.first == "total")
		{
			m_settings.total = atoi(*s.second);
		}
		else if (s.first == "mvol")
		{
			m_settings.musicVolume = (float)atoi(*s.second) / 100.0f;
		}
		else if (s.first == "to")
		{
			m_settings.speedBpm = atof(*s.second);
		}
	}

	const static int tickResolution = 240;

	const TimingPoint* currTimingPoint = nullptr;

	/// For more accurate tracking of ticks for each timing point
	size_t refTimingPointInd = 0;
	double refTimingPointTime = 0.0;

	Vector<uint32> timingPointTicks = {0};

	auto TickToMapTime = [&](uint32 tick) {
		if (tick < timingPointTicks[refTimingPointInd])
		{
			refTimingPointInd = 0;
			refTimingPointTime = static_cast<double>(m_timingPoints[refTimingPointInd].time);
		}
		while (refTimingPointInd + 1 < timingPointTicks.size() && timingPointTicks[refTimingPointInd + 1] <= tick)
		{
			const MapTime timeDiff = timingPointTicks[refTimingPointInd+1] - timingPointTicks[refTimingPointInd];
			refTimingPointTime += Math::MSFromTicks((double) timeDiff, m_timingPoints[refTimingPointInd].GetBPM(), static_cast<double>(tickResolution));
			++refTimingPointInd;
		}

		const uint32 tickDiff = tick - timingPointTicks[refTimingPointInd];

		double mapTime = refTimingPointTime;
		mapTime += Math::MSFromTicks((double) tickDiff, m_timingPoints[refTimingPointInd].GetBPM(), static_cast<double>(tickResolution));
		return Math::RoundToInt(mapTime);
	};

	{
		TimingPoint firstTimingPoint;
		firstTimingPoint.numerator = 4;
		firstTimingPoint.denominator = 4;

		firstTimingPoint.time = atol(*kshootMap.settings["o"]);
		refTimingPointTime = static_cast<double>(firstTimingPoint.time);

		const double bpm = atof(*kshootMap.settings["t"]);
		firstTimingPoint.beatDuration = 60000.0 / bpm;

		currTimingPoint = &(m_timingPoints.Add(std::move(firstTimingPoint)));
	}

	// Add First Lane Toggle Point
	{
		LaneHideTogglePoint startLaneTogglePoint;
		startLaneTogglePoint.time = 0;
		startLaneTogglePoint.duration = 1;
		m_laneTogglePoints.Add(std::move(startLaneTogglePoint));
	}

	// Stop here if we're only going for metadata
	if (metadataOnly)
	{
		return true;
	}

	// Button hold states
	TempButtonState *buttonStates[6] = {nullptr};
	// Laser segment states
	TempLaserState *laserStates[2] = {nullptr};

	EffectType currentButtonEffectTypes[2] = {EffectType::None};
	// 2 per button
	int16 currentButtonEffectParams[4] = {-1};
	const uint32 maxEffectParamsPerButtons = 2;
	float laserRanges[2] = {1.0f, 1.0f};
	MapTime lastLaserPointTime[2] = {0, 0};

	// Stops will be applied after the scroll speed graph is constructed.
	// Tuple of (stopBegin, stopEnd, isOverlappingStop)
	Vector<std::tuple<MapTime, MapTime, bool>> stops;

	MapTime lastMapTime = 0;
	uint32 currentTick = 0;

	bool isManualTilt = false;
	for (KShootMap::TickIterator it(kshootMap); it; ++it)
	{
		const KShootBlock &block = it.GetCurrentBlock();
		KShootTime time = it.GetTime();
		const KShootTick &tick = *it;
		float fxSampleVolume[2] = {1.0, 1.0};
		bool useFxSample[2] = {false, false};
		uint8 fxSampleIndex[2] = {0, 0};
		MapTime mapTime = TickToMapTime(currentTick);
		bool lastTick = &block == &kshootMap.blocks.back() &&
						&tick == &block.ticks.back();

		// flag set when a new effect parameter is set and a new hold notes should be created
		bool splitupHoldNotes[2] = {false, false};

		uint32 tickSettingIndex = 0;
		// Process settings
		for (auto &p : tick.settings)
		{
			// Functions that adds a new timing point at current location if it's not yet there
			auto AddTimingPoint = [&](double newDuration, uint32 newNum, uint32 newDenom, int8 tickrateOffset) {
				if (currTimingPoint->time != mapTime)
				{
					TimingPoint newTimingPoint = TimingPoint(*currTimingPoint);
					newTimingPoint.time = mapTime;

					currTimingPoint = &(m_timingPoints.Add(std::move(newTimingPoint)));
					timingPointTicks.Add(currentTick);
				}

				TimingPoint& lastTimingPoint = *m_timingPoints.rbegin();

				lastTimingPoint.numerator = newNum;
				lastTimingPoint.denominator = newDenom;
				lastTimingPoint.beatDuration = newDuration;
				lastTimingPoint.tickrateOffset = tickrateOffset;
			};

			// Parser the effect and parameters of an FX button (1.60)
			auto ParseFXAndParameters = [&](String in, int16 *paramsOut) {
				// Clear parameters
				memset(paramsOut, -1, sizeof(uint16) * maxEffectParamsPerButtons);

				String effectName = in;
				size_t paramSplit = in.find_first_of(';');
				if (paramSplit != -1)
					effectName = effectName.substr(0, paramSplit);
				effectName.Trim();

				// Clear effect instead?
				if (effectName.empty())
					return EffectType::None;

				const EffectType *type = effectTypeMap.FindEffectType(effectName);
				if (type == nullptr)
				{
					Logf("Invalid custom effect name in ksh map: %s", Logger::Severity::Warning, effectName);
					return EffectType::None;
				}

				if (paramSplit != -1)
				{
					String paramA, paramB;
					String effectParams = p.second.substr(paramSplit + 1);
					if (effectParams.Split(";", &paramA, &paramB))
					{
						paramsOut[0] = atoi(*paramA);
						paramsOut[1] = atoi(*paramB);
					}
					else
						paramsOut[0] = atoi(*effectParams);
				}
				else //set default params
				{
					if (*type < EffectType::UserDefined0) {
						switch (*type)
						{
						case EffectType::Flanger:
							paramsOut[0] = 45;
							paramsOut[1] = 15;
							break;

						default:
							break;
						}
					}
					else {
						m_customAudioEffects.at(*type).SetDefaultEffectParams(paramsOut);
					}
				}

				return *type;
			};

			if (p.first == "beat")
			{
				String n, d;
				if (!p.second.Split("/", &n, &d))
					assert(false);

				uint32 num = atol(*n);
				uint32 denom = atol(*d);

				AddTimingPoint(currTimingPoint->beatDuration, num, denom, currTimingPoint->tickrateOffset);
			}
			else if (p.first == "t")
			{
				const double bpm = atof(*p.second);
				AddTimingPoint(60000.0 / bpm, currTimingPoint->numerator, currTimingPoint->denominator, currTimingPoint->tickrateOffset);
			}
			else if (p.first == "tickrate_offset")
			{
				int8 value = atoi(*p.second);
				AddTimingPoint(currTimingPoint->beatDuration, currTimingPoint->numerator, currTimingPoint->denominator, value);
			}
			else if (p.first == "laserrange_l")
			{
				laserRanges[0] = 2.0f;
			}
			else if (p.first == "laserrange_r")
			{
				laserRanges[1] = 2.0f;
			}
			else if (p.first == "fx-l") // KSH 1.6
			{
				currentButtonEffectTypes[0] = ParseFXAndParameters(p.second, currentButtonEffectParams);
				splitupHoldNotes[0] = true;
			}
			else if (p.first == "fx-r") // KSH 1.6
			{
				currentButtonEffectTypes[1] = ParseFXAndParameters(p.second, currentButtonEffectParams + maxEffectParamsPerButtons);
				splitupHoldNotes[1] = true;
			}
			else if (p.first == "fx-l_param1")
			{
				currentButtonEffectParams[0] = atoi(*p.second);
				splitupHoldNotes[0] = true;
			}
			else if (p.first == "fx-r_param1")
			{
				currentButtonEffectParams[maxEffectParamsPerButtons] = atoi(*p.second);
				splitupHoldNotes[1] = true;
			}
			else if (p.first == "filtertype")
			{
				// Inser filter type change event
				EventObjectState* evt = new EventObjectState();
				evt->interTickIndex = tickSettingIndex;
				evt->time = mapTime;
				evt->key = EventKey::LaserEffectType;
				evt->data.effectVal = ParseFilterType(p.second);
				m_objectStates.emplace_back(std::unique_ptr<ObjectState>(*evt));
			}
			else if (p.first == "pfiltergain")
			{
				// Inser filter type change event
				float gain = (float)atol(*p.second) / 100.0f;
				EventObjectState* evt = new EventObjectState();
				evt->interTickIndex = tickSettingIndex;
				evt->time = mapTime;
				evt->key = EventKey::LaserEffectMix;
				evt->data.floatVal = gain;
				m_objectStates.emplace_back(std::unique_ptr<ObjectState>(*evt));
			}
			else if (p.first == "chokkakuvol")
			{
				float vol = (float)atol(*p.second) / 100.0f;
				EventObjectState* evt = new EventObjectState();
				evt->interTickIndex = tickSettingIndex;
				evt->time = mapTime;
				evt->key = EventKey::LaserEffectMix;
				evt->data.floatVal = vol;
				m_objectStates.emplace_back(std::unique_ptr<ObjectState>(*evt));
			}
			else if (p.first == "zoom_bottom")
			{
				const double value = atol(p.second.data()) / 100.0;
				m_effects.InsertGraphValue(EffectTimeline::GraphType::ZOOM_BOTTOM, mapTime, value);
			}
			else if (p.first == "zoom_top")
			{
				const double value = atol(p.second.data()) / 100.0;
				m_effects.InsertGraphValue(EffectTimeline::GraphType::ZOOM_TOP, mapTime, value);
			}
			else if (p.first == "zoom_side")
			{
				const double value = atol(p.second.data()) / 100.0;
				m_effects.InsertGraphValue(EffectTimeline::GraphType::SHIFT_X, mapTime, value);
			}
			/* OLD USC MANUAL ROLL, KEPT JUST IN CASE
			else if (p.first == "roll")
			{
				ZoomControlPoint* point = new ZoomControlPoint();
				point->time = mapTime;
				point->index = 3;
				point->zoom = (float)atol(*p.second) / 360.0f;
				m_zoomControlPoints.Add(point);
				CHECK_FIRST;
			}
			*/
			else if (p.first == "lane_toggle")
			{
				LaneHideTogglePoint point;
				point.time = mapTime;
				point.duration = atol(*p.second);
				m_laneTogglePoints.Add(std::move(point));
			}
			else if (p.first == "center_split")
			{
				const double value = atol(*p.second) / 100.0;
				m_centerSplit.Insert(mapTime, value);
			}
			else if (p.first == "tilt")
			{
				EventObjectState *evt = new EventObjectState();
				evt->time = mapTime;
				evt->interTickIndex = tickSettingIndex;
				evt->key = EventKey::TrackRollBehaviour;
				evt->data.rollVal = TrackRollBehaviour::Zero;

				String v = p.second;
				size_t f = v.find("keep_");
				if (f != -1)
				{
					evt->data.rollVal = TrackRollBehaviour::Keep;
					v = v.substr(f + 5);
				}

				if (v == "normal")
					evt->data.rollVal = evt->data.rollVal | TrackRollBehaviour::Normal;
				else if (v == "bigger")
					evt->data.rollVal = evt->data.rollVal | TrackRollBehaviour::Bigger;
				else if (v == "biggest")
					evt->data.rollVal = evt->data.rollVal | TrackRollBehaviour::Biggest;
				else if (v == "zero")
					evt->data.rollVal = evt->data.rollVal | TrackRollBehaviour::Zero;
				else
				{
					evt->data.rollVal = TrackRollBehaviour::Manual;

					const double rotation = atof(p.second.data()) * -(10.0 / 360.0);
					m_effects.InsertGraphValue(EffectTimeline::GraphType::ROTATION_Z, mapTime, rotation);

					isManualTilt = true;
					goto after_manual_check;
				}

				if (isManualTilt)
				{
					m_effects.GetGraph(EffectTimeline::GraphType::ROTATION_Z).Extend(mapTime);
				}

			after_manual_check:
				m_objectStates.emplace_back(std::unique_ptr<ObjectState>(*evt));
			}
			else if (p.first == "fx-r_se")
			{
				String filename, vol;
				int fxi = 1;
				useFxSample[fxi] = true;
				if (p.second.Split(";", &filename, &vol))
				{
					fxSampleVolume[fxi] = (float)atoi(*vol) / 100.0f;
				}
				else
				{
					filename = p.second;
				}

				auto it = std::find(m_samplePaths.begin(), m_samplePaths.end(), filename);
				if (it == m_samplePaths.end())
				{
					fxSampleIndex[fxi] = static_cast<uint8>(m_samplePaths.size());
					m_samplePaths.Add(filename);
				}
				else
				{
					fxSampleIndex[fxi] = static_cast<uint8>(std::distance(m_samplePaths.begin(), it));
				}
			}
			else if (p.first == "fx-l_se")
			{
				String filename, vol;
				int fxi = 0;
				useFxSample[fxi] = true;
				if (p.second.Split(";", &filename, &vol))
				{
					fxSampleVolume[fxi] = (float)atoi(*vol) / 100.0f;
				}
				else
				{
					filename = p.second;
				}

				auto it = std::find(m_samplePaths.begin(), m_samplePaths.end(), filename);
				if (it == m_samplePaths.end())
				{
					fxSampleIndex[fxi] = static_cast<uint8>(m_samplePaths.size());
					m_samplePaths.Add(filename);
				}
				else
				{
					fxSampleIndex[fxi] = std::distance(m_samplePaths.begin(), it);
				}
			}
			else if (p.first == "stop")
			{
				// Stops will be applied after the scroll speed graph is constructed.
				const MapTime stopDuration = Math::RoundToInt((atol(*p.second) / 192.0f) * (currTimingPoint->beatDuration) * 4);
				bool isOverlappingStop = false;

				if (!stops.empty() && mapTime < std::get<1>(*stops.rbegin()))
				{
					isOverlappingStop = true;
					std::get<2>(*stops.rbegin()) = true;
				}

				stops.Add(std::make_tuple(mapTime, mapTime + stopDuration, isOverlappingStop));
			}
			else if (p.first == "scroll_speed")
			{
				LineGraph& scrollSpeedGraph = m_effects.GetGraph(EffectTimeline::GraphType::SCROLL_SPEED);
				scrollSpeedGraph.Insert(mapTime, atol(p.second.data()) / 100.0);
			}
			else
			{
				Logf("[KSH]Unkown map parameter at %d:%d: %s", Logger::Severity::Warning, it.GetTime().block, it.GetTime().tick, p.first);
			}
			tickSettingIndex++;
		}

		// Set button states
		for (uint32 i = 0; i < 6; i++)
		{
			char c = i < 4 ? tick.buttons[i] : tick.fx[i - 4];
			TempButtonState *&state = buttonStates[i];
			HoldObjectState *lastHoldObject = nullptr;

			auto IsHoldState = [&]() {
				return state && state->numTicks > 0 && state->fineSnap;
			};
			auto CreateButton = [&]() {
				if (IsHoldState())
				{
					HoldObjectState *obj = lastHoldObject = new HoldObjectState();
					obj->time = TickToMapTime(state->startTick);
					obj->index = i;
					obj->duration = TickToMapTime(currentTick) - obj->time;
					obj->effectType = state->effectType;
					if (state->lastHoldObject)
						state->lastHoldObject->next = obj;
					obj->prev = state->lastHoldObject;
					memcpy(obj->effectParams, state->effectParams, sizeof(state->effectParams));
					m_objectStates.emplace_back(std::unique_ptr<ObjectState>(*obj));;
				}
				else
				{
					ButtonObjectState *obj = new ButtonObjectState();

					obj->time = TickToMapTime(state->startTick);
					obj->index = i;
					obj->hasSample = state->usingSample;
					obj->sampleIndex = state->sampleIndex;
					obj->sampleVolume = state->sampleVolume;
					m_objectStates.emplace_back(std::unique_ptr<ObjectState>(*obj));
				}

				// Reset
				delete state;
				state = nullptr;
			};

			// Split up multiple hold notes
			if (i > 3 && IsHoldState() && splitupHoldNotes[i - 4])
			{
				CreateButton();
			}

			if (c == '0')
			{
				// Terminate hold button
				if (state)
				{
					CreateButton();
				}

				if (i >= 4)
				{
					// Unset effect parameters
					currentButtonEffectParams[i - 4] = -1;
				}
			}
			else if (!state)
			{
				// Create new hold state
				state = new TempButtonState(currentTick);
				uint32 div = (uint32)block.ticks.size();

				if (lastHoldObject)
					state->lastHoldObject = lastHoldObject;

				if (i < 4)
				{
					// Normal '1' notes are always individual
					state->fineSnap = c != '1';
				}
				else
				{
					// FX object '2' is always individual
					state->fineSnap = c != '2';

					// Set effect
					if (c == 'B')
					{
						state->effectType = EffectType::Bitcrush;
						if (currentButtonEffectParams[i - 4] != -1)
							state->effectParams[0] = currentButtonEffectParams[i - 4];
						else
							state->effectParams[0] = 5;
					}
					else if (c >= 'G' && c <= 'L') // Gate 4/8/16/32/12/24
					{
						state->effectType = EffectType::Gate;
						int16 paramMap[] = {
							4, 8, 16, 32, 12, 24};
						state->effectParams[0] = paramMap[c - 'G'];
					}
					else if (c >= 'S' && c <= 'W') // Retrigger 8/16/32/12/24
					{
						state->effectType = EffectType::Retrigger;
						int16 paramMap[] = {
							8, 16, 32, 12, 24};
						state->effectParams[0] = paramMap[c - 'S'];
					}
					else if (c == 'Q')
					{
						state->effectType = EffectType::Phaser;
					}
					else if (c == 'F')
					{
						state->effectType = EffectType::Flanger;
						state->effectParams[0] = 5000;
					}
					else if (c == 'X')
					{
						state->effectType = EffectType::Wobble;
						state->effectParams[0] = 12;
					}
					else if (c == 'D')
					{
						state->effectType = EffectType::SideChain;
					}
					else if (c == 'A')
					{
						state->effectType = EffectType::TapeStop;
						if (currentButtonEffectParams[i - 4] != -1)
							memcpy(state->effectParams, currentButtonEffectParams + (i - 4) * maxEffectParamsPerButtons,
								   sizeof(state->effectParams));
						else
							state->effectParams[0] = 50;
					}
					else if (c == '2')
					{
						state->sampleIndex = fxSampleIndex[i - 4];
						state->usingSample = useFxSample[i - 4];
						state->sampleVolume = fxSampleVolume[i - 4];
					}
					else
					{
						// Use settings method of setting effects+params (1.60)
						state->effectType = currentButtonEffectTypes[i - 4];
						if (currentButtonEffectParams[(i - 4) * maxEffectParamsPerButtons] != -1)
							memcpy(state->effectParams, currentButtonEffectParams + (i - 4) * maxEffectParamsPerButtons,
								   sizeof(state->effectParams));
						else
						{
							state->effectParams[0] = defaultEffectParams[state->effectType];
							state->effectParams[1] = 0;
						}
					}
				}
			}
			else
			{
				// For buttons not using the 1/32 grid
				if (!state->fineSnap)
				{
					CreateButton();

					// Create new hold state
					state = new TempButtonState(currentTick);
					uint32 div = (uint32)block.ticks.size();

					if (i < 4)
					{
						// Normal '1' notes are always individual
						state->fineSnap = c != '1';
					}
					else
					{
						// Hold are always on a high enough snap to make suere they are seperate when needed
						if (c == '2')
						{
							state->fineSnap = false;
							state->sampleIndex = fxSampleIndex[i - 4];
							state->usingSample = useFxSample[i - 4];
							state->sampleVolume = fxSampleVolume[i - 4];
						}
						else
							state->fineSnap = true;
					}
				}
				else
				{
					// Update current hold state
					state->numTicks++;
				}
			}

			// Terminate last item
			if (lastTick && state)
				CreateButton();
		}

		// Set laser states
		for (uint32 i = 0; i < 2; i++)
		{
			TempLaserState *&state = laserStates[i];
			char c = tick.laser[i];

			// Function that creates a new segment out of the current state
			auto CreateLaserSegment = [&](float endPos) {
				// Process existing segment
				//assert(state->numTicks > 0);

				LaserObjectState *obj = new LaserObjectState();

				obj->time = TickToMapTime(state->startTick);
				obj->tick = state->startTick;
				obj->duration = TickToMapTime(currentTick) - obj->time;
				obj->index = i;
				obj->points[0] = state->startPosition;
				obj->points[1] = endPos;
				uint32 tickDuration = currentTick - state->absoluteStartTick;

				if (laserRanges[i] > 1.0f)
				{
					obj->flags |= LaserObjectState::flag_Extended;
				}
				uint32 laserSlamThreshold = tickResolution / 8;
				bool lastSlam = (state->last && (state->last->flags & LaserObjectState::flag_Instant) != 0); // Deal with super fast repeat slams

				if (tickDuration <= laserSlamThreshold && (obj->points[1] != obj->points[0]))
				{
					obj->flags |= LaserObjectState::flag_Instant;
					obj->time = TickToMapTime(state->absoluteStartTick);
					obj->tick = state->absoluteStartTick;
					if (state->spinType != 0)
					{
						obj->spin.duration = state->spinDuration;
						obj->spin.amplitude = state->spinBounceAmplitude;
						obj->spin.frequency = state->spinBounceFrequency;
						obj->spin.decay = state->spinBounceDecay;

						if (state->spinIsBounce)
							obj->spin.type = SpinStruct::SpinType::Bounce;
						else
						{
							switch (state->spinType)
							{
							case '(':
							case ')':
								obj->spin.type = SpinStruct::SpinType::Full;
								break;
							case '<':
							case '>':
								obj->spin.type = SpinStruct::SpinType::Quarter;
								break;
							default:
								break;
							}
						}

						switch (state->spinType)
						{
						case '<':
						case '(':
							obj->spin.direction = -1.0f;
							break;
						case ')':
						case '>':
							obj->spin.direction = 1.0f;
							break;
						default:
							break;
						}
					}
				}

				// Link segments together
				if (state->last)
				{
					// Always fixup duration so they are connected by duration as well
					obj->prev = state->last;
					MapTime actualPrevDuration = obj->time - obj->prev->time;
					if (obj->prev->duration != actualPrevDuration)
					{
						obj->prev->duration = actualPrevDuration;
					}
					obj->prev->next = obj;
				}

				if ((obj->flags & LaserObjectState::flag_Instant) != 0 && lastSlam) //add short straight segment between the slams
				{
					auto midobj = new LaserObjectState();
					midobj->flags = obj->prev->flags & ~LaserObjectState::flag_Instant;
					midobj->points[0] = obj->points[0];
					midobj->points[1] = obj->points[0];
					midobj->time = obj->prev->time;
					midobj->duration = lastLaserPointTime[i] - midobj->time;
					midobj->index = obj->index;

					obj->time = lastLaserPointTime[i];

					midobj->prev = obj->prev;
					obj->prev = midobj;
					midobj->next = obj;
					midobj->prev->next = midobj;

					m_objectStates.emplace_back(std::unique_ptr<ObjectState>(*midobj));
				}

				// Add to list of objects

				assert(obj->GetRoot() != nullptr);

				m_objectStates.emplace_back(std::unique_ptr<ObjectState>(*obj));

				return obj;
			};

			if (c == '-')
			{
				// Terminate laser
				if (state)
				{
					// Reset state
					delete state;
					state = nullptr;

					// Reset range extension
					laserRanges[i] = 1.0f;
				}
			}
			else if (c == ':')
			{
				// Update current laser state
				if (state)
				{
					state->numTicks++;
				}
			}
			else
			{
				float pos = kshootMap.TranslateLaserChar(c);
				LaserObjectState *last = nullptr;
				if (state)
				{
					last = CreateLaserSegment(pos);

					// Reset state
					delete state;
					state = nullptr;
				}

				uint32 startTick = currentTick;
				if (last && (last->flags & LaserObjectState::flag_Instant) != 0)
				{
					// Move offset to be the same as last segment, as in ksh maps there is a 1 tick delay after laser slams
					startTick = last->tick;
				}
				state = new TempLaserState(startTick, currentTick, 0);
				state->last = last; // Link together
				state->startPosition = pos;

				//@[Type][Speed] = spin
				//Types
				//) or ( = full spin
				//> or < = quarter spin
				//Speed is number of 192nd notes
				if (!tick.add.empty() && (tick.add[0] == '@' || tick.add[0] == 'S'))
				{
					state->spinIsBounce = tick.add[0] == 'S';
					state->spinType = tick.add[1];

					String add = tick.add.substr(2);
					if (state->spinIsBounce)
					{
						String duration, amplitude, frequency, decay;

						add.Split(";", &duration, &amplitude);
						amplitude.Split(";", &amplitude, &frequency);
						frequency.Split(";", &frequency, &decay);

						state->spinDuration = std::stoi(duration);
						state->spinBounceAmplitude = std::stoi(amplitude);
						state->spinBounceFrequency = std::stoi(frequency);
						state->spinBounceDecay = std::stoi(decay);
					}
					else
					{
						state->spinDuration = std::stoi(add);
						if (state->spinType == '(' || state->spinType == ')')
							state->spinDuration = state->spinDuration;
					}
				}

				lastLaserPointTime[i] = mapTime;
			}
		}

		lastMapTime = mapTime;
		currentTick += static_cast<uint32>((tickResolution * 4 * currTimingPoint->numerator / currTimingPoint->denominator) / block.ticks.size());
	}

	// Apply stops
	for (const auto& stop : stops)
	{
		const MapTime stopBegin = std::get<0>(stop);
		const MapTime stopEnd = std::get<1>(stop);
		const bool isOverlapping = std::get<2>(stop);

		LineGraph& scrollSpeedGraph = m_effects.GetGraph(EffectTimeline::GraphType::SCROLL_SPEED);

		// In older versions of USC there was a bug where overlapping stop regions made notes scrolling backwards.
		// In other words, stops weren't actually setting the scroll speed to 0, but instead decreased the speed by 1.
		// This bug was utilized as gimmicks for several charts, so for backwards compatibility this behavior is reimplemented when stops are overlapping.
		// For individual stops, scroll speed will actually set to 0 to make those behave nicely with manual scroll speed modifiers.

		if (isOverlapping)
		{
			scrollSpeedGraph.RangeAdd(stopBegin, stopEnd, -1.0);
		}
		else
		{
			scrollSpeedGraph.RangeSet(stopBegin, stopEnd, 0.0);
		}
	}

	// Add chart end event
	EventObjectState *evt = new EventObjectState();
	evt->time = lastMapTime + 2000;
	evt->key = EventKey::ChartEnd;
	m_objectStates.emplace_back(std::unique_ptr<ObjectState>(*evt));

	// Re-sort collection to fix some inconsistencies caused by corrections after laser slams
	ObjectState::SortArray(m_objectStates);

	return true;
}
