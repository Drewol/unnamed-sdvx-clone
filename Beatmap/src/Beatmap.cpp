#include "stdafx.h"
#include "Beatmap.hpp"
#include "Shared/Profiling.hpp"

static const uint32 c_mapVersion = 1;

bool Beatmap::Load(BinaryStream& input, bool metadataOnly)
{
	ProfilerScope $("Load Beatmap");

	if(!m_ProcessKShootMap(input, metadataOnly)) // Load KSH format first
	{
		// Load binary map format
		// input.Seek(0);
		// if(!m_Serialize(input, metadataOnly))
		return false;
	}

	return true;
}

bool Beatmap::Save(BinaryStream& output) const
{
	return false;
	
	// ProfilerScope $("Save Beatmap");
	// Const cast because serialize is universal for loading and saving
	// return const_cast<Beatmap*>(this)->m_Serialize(output, false);
}

const BeatmapSettings& Beatmap::GetMapSettings() const
{
	return m_settings;
}

AudioEffect Beatmap::GetEffect(EffectType type) const
{
	if(type >= EffectType::UserDefined0)
	{
		const AudioEffect* fx = m_customAudioEffects.Find(type);
		assert(fx);
		return *fx;
	}

	return AudioEffect::GetDefault(type);
}

AudioEffect Beatmap::GetFilter(EffectType type) const
{
	if(type >= EffectType::UserDefined0)
	{
		const AudioEffect* fx = m_customAudioFilters.Find(type);
		assert(fx);
		return *fx;
	}

	return AudioEffect::GetDefault(type);
}

MapTime Beatmap::GetFirstObjectTime(MapTime lowerBound) const
{
	if (m_objectStates.empty())
	{
		return lowerBound;
	}

	for (const auto& obj : m_objectStates)
	{
		if (obj->type == ObjectType::Event) continue;
		if (obj->time < lowerBound) continue;

		return obj->time;
	}

	return lowerBound;
}

MapTime Beatmap::GetLastObjectTime() const
{
	if (m_objectStates.empty())
		return 0;

	for (auto it = m_objectStates.rbegin(); it != m_objectStates.rend(); ++it)
	{
		const auto& obj = *it;

		switch (obj->type)
		{
		case ObjectType::Event:
			continue;
		case ObjectType::Hold:
			return obj->time + ((const HoldObjectState*) obj.get())->duration;
		case ObjectType::Laser:
			return obj->time + ((const LaserObjectState*) obj.get())->duration;
		default:
			return obj->time;
		}
	}

	return 0;
}

MapTime Beatmap::GetLastObjectTimeIncludingEvents() const
{
	return m_objectStates.empty() ? 0 : m_objectStates.back()->time;
}

constexpr static double MEASURE_EPSILON = 0.005;

inline static int GetBarCount(const TimingPoint& a, const TimingPoint& b)
{
	const MapTime measureDuration = b.time - a.time;
	const double barCount = measureDuration / a.GetBarDuration();
	int barCountInt = static_cast<int>(barCount + 0.5);

	if (std::abs(barCount - static_cast<double>(barCountInt)) >= MEASURE_EPSILON)
	{
		Logf("A timing point at %d contains non-integer # of bars: %g", Logger::Severity::Debug, a.time, barCount);
		if (barCount > barCountInt) ++barCountInt;
	}

	return barCountInt;
}

MapTime Beatmap::GetMapTimeFromMeasureInd(int measure) const
{
	if (measure < 0) return 0;

	int currMeasure = 0;
	for (int i = 0; i < m_timingPoints.size(); ++i)
	{
		bool isInCurrentTimingPoint = false;
		if (i == m_timingPoints.size() - 1 || measure <= currMeasure)
		{
			isInCurrentTimingPoint = true;
		}
		else
		{
			const int barCount = GetBarCount(m_timingPoints[i], m_timingPoints[i + 1]);

			if (measure < currMeasure + barCount)
				isInCurrentTimingPoint = true;
			else
				currMeasure += barCount;
		}
		if (isInCurrentTimingPoint)
		{
			measure -= currMeasure;
			return static_cast<MapTime>(m_timingPoints[i].time + m_timingPoints[i].GetBarDuration() * measure);
		}
	}

	assert(false);
	return 0;
}

int Beatmap::GetMeasureIndFromMapTime(MapTime time) const
{
	if (time <= 0) return 0;

	int currMeasureCount = 0;
	for (int i = 0; i < m_timingPoints.size(); ++i)
	{
		if (i < m_timingPoints.size() - 1 && m_timingPoints[i + 1].time <= time)
		{
			currMeasureCount += GetBarCount(m_timingPoints[i], m_timingPoints[i + 1]);
			continue;
		}

		return currMeasureCount + static_cast<int>(MEASURE_EPSILON + (time - m_timingPoints[i].time) / m_timingPoints[i].GetBarDuration());
	}

	assert(false);
	return 0;
}

double Beatmap::GetModeBPM() const
{
	Map<double, MapTime> bpmDurations;

	MapTime lastMT = m_settings.offset;
	double largestMT = -1;
	double useBPM = -1;
	double lastBPM = -1;

	for (const TimingPoint& tp : m_timingPoints)
	{
		const double thisBPM = tp.GetBPM();
		const MapTime timeSinceLastTP = tp.time - lastMT;

		const double duration = bpmDurations[lastBPM] += timeSinceLastTP;
		if (duration > largestMT)
		{
			useBPM = lastBPM;
			largestMT = duration;
		}
		lastMT = tp.time;
		lastBPM = thisBPM;
	}

	bpmDurations[lastBPM] += GetLastObjectTime() - lastMT;

	if (bpmDurations[lastBPM] > largestMT)
	{
		useBPM = lastBPM;
	}

	return useBPM;
}

void Beatmap::GetBPMInfo(double& startBPM, double& minBPM, double& maxBPM, double& modeBPM) const
{
	startBPM = -1;
	minBPM = -1;
	maxBPM = -1;
	modeBPM = -1;

	Map<double, MapTime> bpmDurations;

	MapTime lastMT = m_settings.offset;
	
	double largestMT = -1;
	double lastBPM = -1;

	for (const TimingPoint& tp : m_timingPoints)
	{
		const double thisBPM = tp.GetBPM();
		const MapTime timeSinceLastTP = tp.time - lastMT;

		if (startBPM == -1) startBPM = thisBPM;
		if (minBPM == -1 || minBPM > thisBPM) minBPM = thisBPM;
		if (maxBPM == -1 || maxBPM < thisBPM) maxBPM = thisBPM;

		const double duration = bpmDurations[lastBPM] += timeSinceLastTP;
		if (duration > largestMT)
		{
			modeBPM = lastBPM;
			largestMT = duration;
		}
		lastMT = tp.time;
		lastBPM = thisBPM;
	}

	bpmDurations[lastBPM] += GetLastObjectTime() - lastMT;

	if (bpmDurations[lastBPM] > largestMT)
	{
		modeBPM = lastBPM;
	}
}

void Beatmap::Shuffle(int seed, bool random, bool mirror)
{
	if (!random && !mirror) return;

	if (!random)
	{
		assert(mirror);
		ApplyShuffle({3, 2, 1, 0, 5, 4}, true);

		return;
	}

	std::default_random_engine engine(seed);
	std::array<int, 6> swaps = { 0, 1, 2, 3, 4, 5 };

	std::shuffle(swaps.begin(), swaps.begin() + 4, engine);
	std::shuffle(swaps.begin() + 4, swaps.end(), engine);

	bool unchanged = true;
	for (int i = 0; i < 4; ++i)
	{
		if (swaps[i] != (mirror ? 3 - i : i))
		{
			unchanged = true;
			break;
		}
	}

	if (unchanged)
	{
		if (mirror)
		{
			swaps[4] = 4;
			swaps[5] = 5;
		}
		else
		{
			swaps[4] = 5;
			swaps[5] = 4;
		}
	}

	ApplyShuffle(swaps, mirror);
}

void Beatmap::ApplyShuffle(const std::array<int, 6>& swaps, bool flipLaser)
{
	for (auto& object : m_objectStates)
	{
		if (object->type == ObjectType::Single || object->type == ObjectType::Hold)
		{
			ButtonObjectState* bos = (ButtonObjectState*) object.get();
			bos->index = swaps[bos->index];
		}
		else if (object->type == ObjectType::Laser)
		{
			LaserObjectState* los = (LaserObjectState*) object.get();

			if (flipLaser)
			{
				los->index = (los->index + 1) % 2;
				for (size_t i = 0; i < 2; i++)
				{
					los->points[i] = fabsf(los->points[i] - 1.0f);
				}
			}
		}
	}
}

float Beatmap::GetGraphValueAt(EffectTimeline::GraphType type, MapTime mapTime, int aux /*=-1*/) const
{
	// TODO: must tweak camera logic to properly support aux tilt without manual main tilt.

	float value = static_cast<float>(m_baseEffects.GetGraph(type).ValueAt(mapTime));

	if (aux >= 0 && aux < static_cast<int>(m_auxEffects.size()))
	{
		value += static_cast<float>(m_auxEffects[aux].GetGraph(type).ValueAt(mapTime));
	}

	return value;
}

bool Beatmap::CheckIfManualTiltInstant(MapTime lowerBound, MapTime mapTime, int aux /*=-1*/) const
{
	auto checkManualTiltInstant = [&](LineGraph::PointsIterator point, LineGraph::PointsIterator end) {
		if (point == end)
		{
			return false;
		}

		if (!point->second.IsSlam())
		{
			return false;
		}

		if (point->first > mapTime)
		{
			return false;
		}

		return true;
	};

	const LineGraph& baseGraph = m_baseEffects.GetGraph(EffectTimeline::GraphType::ROTATION_Z);
	LineGraph::PointsIterator basePoint = baseGraph.upper_bound(lowerBound);

	if (checkManualTiltInstant(basePoint, baseGraph.end()))
	{
		return true;
	}

	if (aux >= 0 && aux < static_cast<int>(m_auxEffects.size()))
	{
		const LineGraph& auxGraph = m_auxEffects[aux].GetGraph(EffectTimeline::GraphType::ROTATION_Z);
		LineGraph::PointsIterator auxPoint = auxGraph.upper_bound(lowerBound);

		if (checkManualTiltInstant(auxPoint, auxGraph.end()))
		{
			return true;
		}
	}

	return false;
}

float Beatmap::GetCenterSplitValueAt(MapTime mapTime) const
{
	return static_cast<float>(m_centerSplit.ValueAt(mapTime));
}

bool MultiObjectState::StaticSerialize(BinaryStream& stream, MultiObjectState*& obj)
{
	uint8 type = 0;
	if(stream.IsReading())
	{
		// Read type and create appropriate object
		stream << type;
		switch((ObjectType)type)
		{
		case ObjectType::Single:
			obj = (MultiObjectState*)new ButtonObjectState();
			break;
		case ObjectType::Hold:
			obj = (MultiObjectState*)new HoldObjectState();
			break;
		case ObjectType::Laser:
			obj = (MultiObjectState*)new LaserObjectState();
			break;
		case ObjectType::Event:
			obj = (MultiObjectState*)new EventObjectState();
			break;
		}
	}
	else
	{
		// Write type
		type = (uint8)obj->type;
		stream << type;
	}

	// Pointer is always initialized here, serialize data
	stream << obj->time; // Time always set
	switch(obj->type)
	{
	case ObjectType::Single:
		stream << obj->button.index;
		break;
	case ObjectType::Hold:
		stream << obj->hold.index;
		stream << obj->hold.duration;
		stream << (uint16&)obj->hold.effectType;
		stream << (int16&)obj->hold.effectParams[0];
		stream << (int16&)obj->hold.effectParams[1];
		break;
	case ObjectType::Laser:
		stream << obj->laser.index;
		stream << obj->laser.duration;
		stream << obj->laser.points[0];
		stream << obj->laser.points[1];
		stream << obj->laser.flags;
		break;
	case ObjectType::Event:
		stream << (uint8&)obj->event.key;
		stream << *&obj->event.data;
		break;
	}

	return true;
}

bool TimingPoint::StaticSerialize(BinaryStream& stream, TimingPoint*& out)
{
	if(stream.IsReading())
		out = new TimingPoint();
	stream << out->time;
	stream << out->beatDuration;
	stream << out->numerator;
	return true;
}

BinaryStream& operator<<(BinaryStream& stream, BeatmapSettings& settings)
{
	stream << settings.title;
	stream << settings.artist;
	stream << settings.effector;
	stream << settings.illustrator;
	stream << settings.tags;

	stream << settings.bpm;
	stream << settings.offset;

	stream << settings.audioNoFX;
	stream << settings.audioFX;

	stream << settings.jacketPath;

	stream << settings.level;
	stream << settings.difficulty;

	stream << settings.previewOffset;
	stream << settings.previewDuration;

	stream << settings.slamVolume;
	stream << settings.laserEffectMix;
	stream << (uint8&)settings.laserEffectType;
	return stream;
}

/*
bool Beatmap::m_Serialize(BinaryStream& stream, bool metadataOnly)
{
	static const uint32 c_magic = *(uint32*)"FXMM";
	uint32 magic = c_magic;
	uint32 version = c_mapVersion;
	stream << magic;
	stream << version;

	// Validate headers when reading
	if(stream.IsReading())
	{
		if(magic != c_magic)
		{
			Log("Invalid map format", Logger::Severity::Warning);
			return false;
		}
		if(version != c_mapVersion)
		{
			Logf("Incompatible map version [%d], loader is version %d", Logger::Severity::Warning, version, c_mapVersion);
			return false;
		}
	}

	stream << m_settings;
	stream << m_timingPoints;
	stream << reinterpret_cast<Vector<MultiObjectState*>&>(m_objectStates);

	// Manually fix up laser next-prev pointers
	LaserObjectState* prevLasers[2] = { 0 };
	if(stream.IsReading())
	{
		for(ObjectState* obj : m_objectStates)
		{
			if(obj->type == ObjectType::Laser)
			{
				LaserObjectState* laser = (LaserObjectState*)obj;
				LaserObjectState*& prev = prevLasers[laser->index];
				if(prev && (prev->time + prev->duration) == laser->time)
				{
					prev->next = laser;
					laser->prev = prev;
				}

				prev = laser;
			}
		}
	}

	return true;
}
*/

bool BeatmapSettings::StaticSerialize(BinaryStream& stream, BeatmapSettings*& settings)
{
	if(stream.IsReading())
		settings = new BeatmapSettings();
	stream << *settings;
	return true;
}