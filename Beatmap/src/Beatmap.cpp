#include "stdafx.h"
#include "Beatmap.hpp"
#include "Shared/Profiling.hpp"

static const uint32 c_mapVersion = 1;

bool Beatmap::Load(BinaryStream& input, bool metadataOnly)
{
	ProfilerScope $("Load Beatmap");

	// Load KSH format first
	if(!m_ProcessKShootMap(input, metadataOnly))
	{
		// Note: support for binary file format is dropped.
		input.Seek(0);
		if (!m_Serialize(input, metadataOnly))
		{
			return false;
		}
	}

	return true;
}

bool Beatmap::Save(BinaryStream& output) const
{
	// Note: support for binary file format is dropped.
	return false;
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

Beatmap::TimingPointsIterator Beatmap::GetTimingPoint(MapTime mapTime, TimingPointsIterator hint, bool forwardOnly) const
{
	if (m_timingPoints.empty())
	{
		return m_timingPoints.end();
	}

	// Check for common cases
	if (hint != m_timingPoints.end())
	{
		if (hint->time <= mapTime)
		{
			if (std::next(hint) == m_timingPoints.end())
			{
				return hint;
			}
			if (mapTime < std::next(hint)->time)
			{
				return hint;
			}
			else
			{
				++hint;
			}
		}
		else if (forwardOnly)
		{
			return hint;
		}
		else if(hint == m_timingPoints.begin())
		{
			return hint;
		}
		else if(std::prev(hint)->time <= mapTime)
		{
			return std::prev(hint);
		}
		else
		{
			--hint;
		}
	}

	size_t hintInd = static_cast<size_t>(std::distance(m_timingPoints.begin(), hint));

	if (hint == m_timingPoints.end() || mapTime < hint->time)
	{
		// mapTime before hint
		size_t diff = 1;
		size_t prevDiff = 0;
		while (diff <= hintInd)
		{
			if (m_timingPoints[hintInd - diff].time <= mapTime)
			{
				break;
			}

			prevDiff = diff;
			diff *= 2;
		}

		if (diff > hintInd)
		{
			diff = hintInd;
		}
		return GetTimingPoint(mapTime, hintInd - diff, hintInd - prevDiff);
	}
	else if (hint->time == mapTime)
	{
		return hint;
	}
	else
	{
		// mapTime after hint
		size_t diff = 1;
		size_t prevDiff = 0;
		while (hintInd + diff < m_timingPoints.size())
		{
			if (mapTime <= m_timingPoints[hintInd + diff].time)
			{
				break;
			}

			prevDiff = diff;
			diff *= 2;
		}

		if (hintInd + diff >= m_timingPoints.size())
		{
			diff = m_timingPoints.size() - 1 - hintInd;
		}

		return GetTimingPoint(mapTime, hintInd + prevDiff, hintInd + diff + 1);
	}
}

Beatmap::TimingPointsIterator Beatmap::GetTimingPoint(MapTime mapTime, size_t begin, size_t end) const
{
	if (end <= begin)
	{
		return m_timingPoints.begin();
	}

	if (mapTime < m_timingPoints[begin].time)
	{
		return m_timingPoints.begin() + begin;
	}

	if (end < m_timingPoints.size() && mapTime >= m_timingPoints[end].time)
	{
		return m_timingPoints.begin() + end;
	}

	while (begin + 2 < end)
	{
		const size_t mid = (begin + end) / 2;
		if (m_timingPoints[mid].time < mapTime)
		{
			begin = mid;
		}
		else if (m_timingPoints[mid].time > mapTime)
		{
			end = mid;
		}
		else
		{
			return m_timingPoints.begin() + mid;
		}
	}

	if (begin + 1 < end && m_timingPoints[begin + 1].time <= mapTime)
	{
		return m_timingPoints.begin() + (begin + 1);
	}
	else
	{
		return m_timingPoints.begin() + begin;
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

bool Beatmap::CheckIfManualTiltInstant(MapTime bound, MapTime mapTime, int aux /*=-1*/) const
{
	auto checkManualTiltInstant = [&](const EffectTimeline& timeline) {
		const LineGraph& graph = timeline.GetGraph(EffectTimeline::GraphType::ROTATION_Z);
		if (graph.empty()) return false;

		const LineGraph::PointsIterator point = graph.upper_bound(bound);
		if (point == graph.end())
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

	if (checkManualTiltInstant(m_baseEffects))
	{
		return true;
	}

	if (aux >= 0 && aux < static_cast<int>(m_auxEffects.size()))
	{
		if (checkManualTiltInstant(m_auxEffects[aux]))
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

bool Beatmap::m_Serialize(BinaryStream& stream, bool metadataOnly)
{
	return false;
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

bool BeatmapSettings::StaticSerialize(BinaryStream& stream, BeatmapSettings*& settings)
{
	if (stream.IsReading())
		settings = new BeatmapSettings();
	stream << *settings;
	return true;
}