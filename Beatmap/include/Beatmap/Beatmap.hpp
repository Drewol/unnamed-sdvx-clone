#pragma once
#include "BeatmapObjects.hpp"
#include "AudioEffects.hpp"

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
	EffectType laserEffectType = EffectType::PeakingFilter;
};

/*
	Generic beatmap format, Can either load it's own format or KShoot maps
*/
class Beatmap : public Unique
{
public:
	virtual ~Beatmap();
	Beatmap() = default;
	Beatmap(Beatmap&& other);
	Beatmap& operator=(Beatmap&& other);

	bool Load(BinaryStream& input, bool metadataOnly = false);
	// Saves the map as it's own format
	bool Save(BinaryStream& output) const;

	// Returns the settings of the map, contains metadata + song/image paths.
	const BeatmapSettings& GetMapSettings() const;

	// Vector of timing points in the map, sorted by when they appear in the map
	// Must keep the beatmap class instance alive for these to stay valid
	// Can contain multiple objects at the same time
	const Vector<TimingPoint*>& GetLinearTimingPoints() const;
	// Vector of chart stops in the chart, sorted by when they appear in the map
	// Must keep the beatmap class instance alive for these to stay valid
	// Can contain multiple objects at the same time
	const Vector<ChartStop*>& GetLinearChartStops() const;
	// Vector of objects in the map, sorted by when they appear in the map
	// Must keep the beatmap class instance alive for these to stay valid
	// Can contain multiple objects at the same time
	const Vector<ObjectState*>& GetLinearObjects() const;
	// Vector of zoom control points in the map, sorted by when they appear in the map
	// Must keep the beatmap class instance alive for these to stay valid
	// Can contain multiple objects at the same time
	const Vector<ZoomControlPoint*>& GetZoomControlPoints() const;

	const Vector<LaneHideTogglePoint*>& GetLaneTogglePoints() const;

	const Vector<String>& GetSamplePaths() const;

	const Vector<String>& GetSwitchablePaths() const;

	// Retrieves audio effect settings for a given button id
	AudioEffect GetEffect(EffectType type) const;
	// Retrieves audio effect settings for a given filter effect id
	AudioEffect GetFilter(EffectType type) const;

	// Get the timing of the last (non-event) object
	MapTime GetLastObjectTime() const;

	// Measure -> Time
	MapTime GetMapTimeFromMeasureInd(int measure) const;
	// Time -> Measure
	int GetMeasureIndFromMapTime(MapTime time) const;

private:
	bool m_ProcessKShootMap(BinaryStream& input, bool metadataOnly);
	bool m_Serialize(BinaryStream& stream, bool metadataOnly);

	Map<EffectType, AudioEffect> m_customEffects;
	Map<EffectType, AudioEffect> m_customFilters;

	Vector<TimingPoint*> m_timingPoints;
	Vector<ChartStop*> m_chartStops;
	Vector<LaneHideTogglePoint*> m_laneTogglePoints;
	Vector<ObjectState*> m_objectStates;
	Vector<ZoomControlPoint*> m_zoomControlPoints;
	Vector<String> m_samplePaths;
	Vector<String> m_switchablePaths;
	BeatmapSettings m_settings;
};