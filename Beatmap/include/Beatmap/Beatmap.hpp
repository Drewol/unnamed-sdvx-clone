#pragma once
#include "BeatmapObjects.hpp"
#include "AudioEffects.hpp"
#include "EffectTimeline.hpp"

/* Global settings stored in a beatmap */
struct BeatmapSettings
{
	static bool StaticSerialize(BinaryStream& stream, BeatmapSettings*& settings);

	// Basic song meta data
	String title;
	String artist;
	String effector;
	String illustrator;
	String tags;
	// Reported BPM range by the map
	String bpm;
	// Offset in ms for the map to start
	MapTime offset;
	// Both audio tracks specified for the map / if any is set
	String audioNoFX;
	String audioFX;
	// Path to the jacket image
	String jacketPath;
	// Path to the background and foreground shader files
	String backgroundPath;
	String foregroundPath;

	// Level, as indicated by map creator
	uint8 level;

	// Difficulty, as indicated by map creator
	uint8 difficulty;

	// Total, total gauge gained when played perfectly
	uint16 total = 0;

	// Preview offset
	MapTime previewOffset;
	// Preview duration
	MapTime previewDuration;

	// Initial audio settings
	float slamVolume = 1.0f;
	float laserEffectMix = 1.0f;
	float musicVolume = 1.0f;

	// BPM Override for mmod calculation
	float speedBpm = -1.0f;

	EffectType laserEffectType = EffectType::PeakingFilter;
};

/*
	Generic beatmap format, Can either load it's own format or KShoot maps
*/
class Beatmap : public Unique
{
public:
	// Vector interacts badly with unique_ptr, so std::vector was used instead.
	using Objects = std::vector<std::unique_ptr<ObjectState>>;
	using ObjectsIterator = Objects::const_iterator;

	using TimingPoints = Vector<TimingPoint>;
	using TimingPointsIterator = TimingPoints::const_iterator;

	using LaneTogglePoints = Vector<LaneHideTogglePoint>;
	using LaneTogglePointsIterator = LaneTogglePoints::const_iterator;

public:
	bool Load(BinaryStream& input, bool metadataOnly = false);

	/// Returns the settings of the map, contains metadata + song/image paths.
	const BeatmapSettings& GetMapSettings() const;

	const Vector<LaneHideTogglePoint>& GetLaneTogglePoints() const { return m_laneTogglePoints; }

	const Vector<String>& GetSamplePaths() const { return m_samplePaths; }
	const Vector<String>& GetSwitchablePaths() const { return m_switchablePaths; }

	/// Retrieves audio effect settings for a given button id
	AudioEffect GetEffect(EffectType type) const;
	/// Retrieves audio effect settings for a given filter effect id
	AudioEffect GetFilter(EffectType type) const;

	/// Get the timing of the first (non-event) object
	MapTime GetFirstObjectTime(MapTime lowerBound) const;
	/// Get the timing of the last (non-event) object
	MapTime GetLastObjectTime() const;
	/// Get the timing of the last object, including the event objects
	MapTime GetLastObjectTimeIncludingEvents() const;

	/// Measure -> Time
	MapTime GetMapTimeFromMeasureInd(int measure) const;
	/// Time -> Measure
	int GetMeasureIndFromMapTime(MapTime time) const;

	/// Computes the most frequently occuring BPM (to be used for MMod)
	double GetModeBPM() const;
	void GetBPMInfo(double& startBPM, double& minBPM, double& maxBPM, double& modeBPM) const;

	void Shuffle(int seed, bool random, bool mirror);
	void ApplyShuffle(const std::array<int, 6>& swaps, bool flipLaser);

	/// # of (4th-note) beats between the start and the end
	float GetBeatCount(MapTime start, MapTime end, TimingPointsIterator hint) const;
	float GetBeatCountWithScrollSpeedApplied(MapTime start, MapTime end, TimingPointsIterator hint) const;

	inline float GetBeatCount(MapTime start, MapTime end) const
	{
		return GetBeatCount(start, end, GetTimingPoint(start));
	}

	inline float GetBeatCountWithScrollSpeedApplied(MapTime start, MapTime end) const
	{
		return GetBeatCountWithScrollSpeedApplied(start, end, GetTimingPoint(start));
	}

	const Objects& GetObjectStates() const { return m_objectStates; }

	ObjectsIterator GetFirstObjectState() const { return m_objectStates.begin(); }
	ObjectsIterator GetEndObjectState() const { return m_objectStates.end(); }

	bool HasObjectState() const { return !m_objectStates.empty(); }

	const TimingPoints& GetTimingPoints() const { return m_timingPoints; }

	TimingPointsIterator GetFirstTimingPoint() const { return m_timingPoints.begin(); }
	TimingPointsIterator GetEndTimingPoint() const { return m_timingPoints.end(); }

	/// Returns the latest timing point for given mapTime
	inline TimingPointsIterator GetTimingPoint(MapTime mapTime) const
	{
		return GetTimingPoint(mapTime, 0, m_timingPoints.size());
	}

	/// GetTimingPoint but a hint is given
	TimingPointsIterator GetTimingPoint(MapTime mapTime, TimingPointsIterator hint, bool forwardOnly = false) const;

	/// GetTimingPoint but begin and end ranges are specified
	inline TimingPointsIterator GetTimingPoint(MapTime mapTime, TimingPointsIterator beginIt, TimingPointsIterator endIt) const
	{
		return GetTimingPoint(mapTime, static_cast<size_t>(std::distance(m_timingPoints.begin(), beginIt)), static_cast<size_t>(std::distance(m_timingPoints.begin(), endIt)));
	}

	TimingPointsIterator GetTimingPoint(MapTime mapTime, size_t begin, size_t end) const;

	LaneTogglePointsIterator GetFirstLaneTogglePoint() const { return m_laneTogglePoints.begin(); }
	LaneTogglePointsIterator GetEndLaneTogglePoint() const { return m_laneTogglePoints.end(); }

	float GetGraphValueAt(EffectTimeline::GraphType type, MapTime mapTime) const;
	bool CheckIfManualTiltInstant(MapTime bound, MapTime mapTime) const;

	float GetCenterSplitValueAt(MapTime mapTime) const;
	float GetScrollSpeedAt(MapTime mapTime) const;

private:
	bool m_ProcessKShootMap(BinaryStream& input, bool metadataOnly);

	Map<EffectType, AudioEffect> m_customAudioEffects;
	Map<EffectType, AudioEffect> m_customAudioFilters;

	Objects m_objectStates;
	TimingPoints m_timingPoints;

	EffectTimeline m_effects;

	LineGraph m_centerSplit;
	Vector<LaneHideTogglePoint> m_laneTogglePoints;
	Map<String, Map<MapTime, String>> m_positionalOptions;

	Vector<String> m_samplePaths;
	Vector<String> m_switchablePaths;
	BeatmapSettings m_settings;
};