#pragma once
#include "Beatmap.hpp"

/*
	Manages the iteration over beatmaps
*/
class BeatmapPlayback
{
public:
	BeatmapPlayback() = default;
	BeatmapPlayback(const Beatmap& beatmap);

	// Resets the playback of the map
	// Must be called before any other function is called on this object
	// returns false if the map contains no objects or timing or otherwise invalid
	bool Reset(MapTime initTime = 0, MapTime start = 0);

	// Updates the time of the playback
	// checks all items that have been triggered between last time and this time
	// if it is a new timing point, this is used for the new BPM
	void Update(MapTime newTime);

	MapTime hittableObjectEnter = 500;
	MapTime hittableLaserEnter = 1000;
	MapTime hittableObjectLeave = 500;
	MapTime alertLaserThreshold = 1500;
	MapTime audioOffset = 0;
	bool cMod = false;
	float cModSpeed = 400;

	// Removes any existing data and sets a special behaviour for calibration mode
	void MakeCalibrationPlayback();

	/// Get all objects that fall within the given visible range,
	/// `numBeats` is the # of 4th notes
	void GetObjectsInViewRange(float numBeats, Vector<ObjectState*>& objects);
	void GetBarPositionsInViewRange(float numBeats, Vector<float>& barPositions) const;

	[[nodiscard]]
	const ObjectState* GetFirstButtonOrHoldAfterTime(MapTime t, int lane) const;

	// Duration for objects to keep being returned by GetObjectsInRange after they have passed the current time
	MapTime keepObjectDuration = 1000;

	// Get the timing point at the current time
	[[nodiscard]]
	const TimingPoint& GetCurrentTimingPoint() const;
	// Get the timing point at a given time
	[[nodiscard]]
	const TimingPoint* GetTimingPointAt(MapTime time) const;
	
	// The beatmap this player is using
	[[nodiscard]]
	const Beatmap& GetBeatmap() noexcept { return *m_beatmap; }

	// Counts the total amount of beats that have passed within <start, start+range>
	// Returns the number of passed beats
	// Returns the starting index of the passed beats in 'startIndex'
	// Additionally the time signature is multiplied by multiplier
	//	with a multiplier of 2 a 4/4 signature would tick twice as fast
	[[nodiscard]]
	uint32 CountBeats(MapTime start, MapTime range, int32& startIndex, uint32 multiplier = 1) const;

	// View coordinate conversions
	[[nodiscard]]
	inline float TimeToViewDistance(MapTime mapTime) const
	{
		return GetViewDistance(m_playbackTime, mapTime);
	}

	[[nodiscard]]
	inline float TimeToViewDistanceIgnoringScrollSpeed(MapTime mapTime) const
	{
		return GetViewDistanceIgnoringScrollSpeed(m_playbackTime, mapTime);
	}

	[[nodiscard]]
	inline float ToViewDistance(MapTime startTime, MapTime duration) const
	{
		return GetViewDistance(startTime, startTime + duration);
	}

	[[nodiscard]]
	inline float ToViewDistanceIgnoringScrollSpeed(MapTime startTime, MapTime duration) const
	{
		return GetViewDistanceIgnoringScrollSpeed(startTime, startTime + duration);
	}

	/// Get # of (4th) beats between `startTime` and `endTime`, taking scroll speed changes into account (if CMOD is not used).
	[[nodiscard]]
	float GetViewDistance(MapTime startTime, MapTime endTime) const;

	/// Get # of (4th) beats between `startTime` and `endTime`, ignoring any scroll speed changes.
	[[nodiscard]]
	float GetViewDistanceIgnoringScrollSpeed(MapTime startTime, MapTime endTime) const;

	// Current map time in ms as last passed to Update
	[[nodiscard]]
	inline MapTime GetLastTime() const { return m_playbackTime; }

	// Value from 0 to 1 that indicates how far in a single bar the playback is
	[[nodiscard]]
	inline float GetBarTime() const noexcept { return m_barTime; }
	[[nodiscard]]
	inline float GetBeatTime() const noexcept { return m_beatTime; }

	// Gets the currently set value of a value set by events in the beatmap
	[[nodiscard]]
	const EventData& GetEventData(EventKey key);

	// Retrieve event data as any 32-bit type
	template<typename T>
	[[nodiscard]]
	const typename std::enable_if<std::is_integral<T>::value && sizeof(T) <= 4, T>::type& GetEventData(EventKey key)
	{
		return *(T*)&GetEventData(key);
	}

	// Get interpolated top or bottom zoom as set by the map
	[[nodiscard]]
	float GetZoom(uint8 index) const;
	[[nodiscard]]
	float GetScrollSpeed() const;

	// Checks if current manual tilt value is instant
	[[nodiscard]]
	bool CheckIfManualTiltInstant();

	/* Playback events */
	// Called when an object became within the 'hittableObjectTreshold'
	Delegate<ObjectState*> OnObjectEntered;
	// Called when a laser became within the 'alertLaserThreshold'
	Delegate<LaserObjectState*> OnLaserAlertEntered;
	// Called after an object has passed the duration it can be hit in
	Delegate<ObjectState*> OnObjectLeaved;
	// Called when an FX button with effect enters
	Delegate<HoldObjectState*> OnFXBegin;
	// Called when an FX button with effect leaves
	Delegate<HoldObjectState*> OnFXEnd;

	// Called when a new timing point becomes active
	Delegate<Beatmap::TimingPointsIterator> OnTimingPointChanged;
	Delegate<Beatmap::LaneTogglePointsIterator> OnLaneToggleChanged;
	Delegate<Beatmap::ReverseColorTogglePointsIterator> OnReverseColorToggleChanged;

	Delegate<EventKey, EventData> OnEventChanged;

	/// Used for debugging
	[[nodiscard]]
	Vector<String> GetStateString() const;

private:
	// Selects an object or timing point based on a given input state
	// if allowReset is true the search starts from the start of the object list if current point lies beyond given input time
	[[nodiscard]]
	Beatmap::ObjectsIterator m_SelectHitObject(MapTime time, bool allowReset = false) const;
	[[nodiscard]]
	Beatmap::TimingPointsIterator m_SelectTimingPoint(MapTime time, bool allowReset = false) const;
	[[nodiscard]]
	Beatmap::LaneTogglePointsIterator m_SelectLaneTogglePoint(MapTime time, bool allowReset = false) const;
	[[nodiscard]]
	Beatmap::ReverseColorTogglePointsIterator m_SelectReverseColorTogglePoint(MapTime time, bool allowReset = false) const;

	// End object iterator, this is not a valid iterator, but points to the element after the last element
	[[nodiscard]]
	bool IsEndObject(const Beatmap::ObjectsIterator& obj) const;
	[[nodiscard]]
	bool IsEndTiming(const Beatmap::TimingPointsIterator& obj) const;
	[[nodiscard]]
	bool IsEndLaneToggle(const Beatmap::LaneTogglePointsIterator& obj) const;
	[[nodiscard]]
	bool IsEndReverseColorToggle(const Beatmap::ReverseColorTogglePointsIterator& obj) const;

	// Current map position of this playback object
	MapTime m_playbackTime;

	// Disregard objects outside of this range
	MapTimeRange m_playRange;
	bool m_initialEffectStateSent = false;

	Beatmap::ObjectsIterator m_currObject;
	Beatmap::ObjectsIterator m_currLaserObject;
	Beatmap::ObjectsIterator m_currAlertObject;

	Beatmap::TimingPointsIterator m_currentTiming;
	Beatmap::LaneTogglePointsIterator m_currentLaneTogglePoint;
	Beatmap::ReverseColorTogglePointsIterator m_currentReverseColorTogglePoint;

	TrackRollBehaviour m_currentTrackRollBehaviour = TrackRollBehaviour::Normal;
	MapTime m_lastTrackRollBehaviourChange = 0;

	// Contains all the objects that are in the current valid timing area
	Multimap<MapTime, ObjectState*> m_objectsByTime;

	// Ordered by leaving time
	Multimap<MapTime, ObjectState*> m_objectsByLeaveTime;
	
	// Hold buttons with effects that are active
	Set<ObjectState*> m_effectObjects;

	// Current state of events
	Map<EventKey, EventData> m_eventMapping;

	float m_barTime;
	float m_beatTime;

	const Beatmap* m_beatmap = nullptr;

	// For the calibration mode
	bool m_isCalibration = false;
	Vector<Ref<ObjectState>> m_calibrationObjects;
	TimingPoint m_calibrationTiming;
};
