#include "stdafx.h"
#include "Beatmap.hpp"
#include "Shared/Profiling.hpp"

static const uint32 c_mapVersion = 1;

Beatmap::~Beatmap()
{
	// Perform cleanup
	for(auto tp : m_timingPoints)
		delete tp;
	for(auto obj : m_objectStates)
		delete obj;
	for (auto z : m_zoomControlPoints)
		delete z;
	for (auto z : m_laneTogglePoints)
		delete z;
	for (auto cs : m_chartStops)
		delete cs;
}
Beatmap::Beatmap(Beatmap&& other)
{
	m_timingPoints = std::move(other.m_timingPoints);
	m_objectStates = std::move(other.m_objectStates);
	m_zoomControlPoints = std::move(other.m_zoomControlPoints);
	m_laneTogglePoints = std::move(other.m_laneTogglePoints);
	m_settings = std::move(other.m_settings);
}
Beatmap& Beatmap::operator=(Beatmap&& other)
{
	// Perform cleanup
	for(auto tp : m_timingPoints)
		delete tp;
	for(auto obj : m_objectStates)
		delete obj;
	for(auto z : m_zoomControlPoints)
		delete z;
	m_timingPoints = std::move(other.m_timingPoints);
	m_objectStates = std::move(other.m_objectStates);
	m_zoomControlPoints = std::move(other.m_zoomControlPoints);
	m_laneTogglePoints = std::move(other.m_laneTogglePoints);
	m_settings = std::move(other.m_settings);
	return *this;
}
bool Beatmap::Load(BinaryStream& input, bool metadataOnly)
{
	ProfilerScope $("Load Beatmap");

	if(!m_ProcessKShootMap(input, metadataOnly)) // Load KSH format first
	{
		// Load binary map format
		input.Seek(0);
		if(!m_Serialize(input, metadataOnly))
			return false;
	}

	return true;
}
bool Beatmap::Save(BinaryStream& output) const
{
	ProfilerScope $("Save Beatmap");
	// Const cast because serialize is universal for loading and saving
	return const_cast<Beatmap*>(this)->m_Serialize(output, false);
}

const BeatmapSettings& Beatmap::GetMapSettings() const
{
	return m_settings;
}

const Vector<TimingPoint*>& Beatmap::GetLinearTimingPoints() const
{
	return m_timingPoints;
}
const Vector<ChartStop*>& Beatmap::GetLinearChartStops() const
{
	return m_chartStops;
}
const Vector<LaneHideTogglePoint*>& Beatmap::GetLaneTogglePoints() const
{
	return m_laneTogglePoints;
}
const Vector<ObjectState*>& Beatmap::GetLinearObjects() const
{
	return reinterpret_cast<const Vector<ObjectState*>&>(m_objectStates);
}
const Vector<ZoomControlPoint*>& Beatmap::GetZoomControlPoints() const
{
	return m_zoomControlPoints;
}

const Vector<String>& Beatmap::GetSamplePaths() const
{
	return m_samplePaths;
}

const Vector<String>& Beatmap::GetSwitchablePaths() const
{
	return m_switchablePaths;
}

AudioEffect Beatmap::GetEffect(EffectType type) const
{
	if(type >= EffectType::UserDefined0)
	{
		const AudioEffect* fx = m_customEffects.Find(type);
		assert(fx);
		return *fx;
	}
	return AudioEffect::GetDefault(type);
}
AudioEffect Beatmap::GetFilter(EffectType type) const
{
	if(type >= EffectType::UserDefined0)
	{
		const AudioEffect* fx = m_customFilters.Find(type);
		assert(fx);
		return *fx;
	}
	return AudioEffect::GetDefault(type);
}

MapTime Beatmap::GetLastObjectTime() const
{
	if (m_objectStates.size() == 0)
		return 0;

	for (auto it = m_objectStates.rbegin(); it != m_objectStates.rend(); ++it)
	{
		const ObjectState* obj = *it;
		switch (obj->type)
		{
		case ObjectType::Event:
			continue;
		case ObjectType::Hold:
			return obj->time + ((const HoldObjectState*)obj)->duration;
		case ObjectType::Laser:
			return obj->time + ((const LaserObjectState*)obj)->duration;
		default:
			return obj->time;
		}
	}

	return 0;
}

constexpr static double MEASURE_EPSILON = 0.005;

inline static int GetBarCount(const TimingPoint* a, const TimingPoint* b)
{
	const MapTime measureDuration = b->time - a->time;
	const double barCount = measureDuration / a->GetBarDuration();
	int barCountInt = static_cast<int>(barCount + 0.5);

	if (std::abs(barCount - static_cast<double>(barCountInt)) >= MEASURE_EPSILON)
	{
		Logf("A timing point at %d contains non-integer # of bars: %g", Logger::Severity::Info, a->time, barCount);
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
			return static_cast<MapTime>(m_timingPoints[i]->time + m_timingPoints[i]->GetBarDuration() * measure);
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
		if (i < m_timingPoints.size() - 1 && m_timingPoints[i + 1]->time <= time)
		{
			currMeasureCount += GetBarCount(m_timingPoints[i], m_timingPoints[i + 1]);
			continue;
		}

		return currMeasureCount + static_cast<int>(MEASURE_EPSILON + (time - m_timingPoints[i]->time) / m_timingPoints[i]->GetBarDuration());
	}

	assert(false);
	return 0;
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

bool BeatmapSettings::StaticSerialize(BinaryStream& stream, BeatmapSettings*& settings)
{
	if(stream.IsReading())
		settings = new BeatmapSettings();
	stream << *settings;
	return true;
}
