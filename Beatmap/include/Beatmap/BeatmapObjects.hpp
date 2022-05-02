/*
	This file contains all the data types that are used for objects inside maps
	The basic object class is ObjectState
		this class contains 'type' member which indicates to which type it is castable

	No vtable for smaller memory footprint
*/
#pragma once
// For effect type enum
#include "AudioEffects.hpp"

// Time unit used for all objects
// this is the offset from the audio file beginning in ms for timing points
// this is the offset from the map's global offset in ms for object states
// this type is also used for negative time delta values
// the maximum map length that can be represented by map time is
//	2147483648	ms
//	~2147483	sec
//	~35791		min
using MapTime = int32;

struct MapTimeRange
{
	constexpr MapTimeRange() noexcept : begin(0), end(0) {}
	explicit constexpr MapTimeRange(MapTime begin) noexcept : begin(begin), end(0) {}
	constexpr MapTimeRange(MapTime begin, MapTime end) noexcept : begin(begin), end(end) {}

	constexpr MapTimeRange(const MapTimeRange&) noexcept = default;
	constexpr MapTimeRange(MapTimeRange&&) noexcept = default;

	constexpr MapTimeRange& operator= (const MapTimeRange&) noexcept = default;
	constexpr MapTimeRange& operator= (MapTimeRange&&) noexcept = default;

	constexpr bool HasEnd() const noexcept { return begin < end; }
	constexpr MapTime Length() const noexcept { return HasEnd() ? end - begin : 0; }
	constexpr MapTime Length(const MapTime defaultEnd) const noexcept { return HasEnd() ? end - begin : defaultEnd - begin; }
	constexpr bool Includes(const MapTime t, const bool includingEnd = false) const noexcept { return begin <= t && (!HasEnd() || (includingEnd ? t <= end : t < end)); }
	constexpr bool Includes(const MapTimeRange& other) const noexcept { return Includes(other.begin) && (other.HasEnd() ? Includes(other.end, true) : !HasEnd());}
	constexpr bool Overlaps(const MapTimeRange& other) const noexcept { if (begin <= other.begin) return Includes(other.begin); else return other.Includes(begin); }

	MapTime begin, end;
};

// Type enum for map object
enum class ObjectType : uint8
{
	Invalid = 0,
	Single, // Either normal or FX button
	Hold,   // Either normal or FX button but with a duration
	Laser,  // A laser segment
	Event   // Event object
};

// The key parameter for event objects
enum class EventKey : uint8
{
	SlamVolume,			// Float
	LaserEffectType,	// Effect
	LaserEffectMix,		// Float
	TrackRollBehaviour, // (uint8,Float)
	ChartEnd,
};

enum class TrackRollBehaviour : uint8
{
	// Either one of the following four
	Zero = 0,
	Normal = 0x1,
	Bigger = 0x2,
	Biggest = 0x3,
	Manual = 0x4,
	// Flag for keep
	Keep = 0x8,
};
TrackRollBehaviour operator|(const TrackRollBehaviour &l, const TrackRollBehaviour &r);
TrackRollBehaviour operator&(const TrackRollBehaviour &l, const TrackRollBehaviour &r);

// Sensitive data layout since union structure is used to access buttons/holds/... object states
#pragma pack(push, 1)

// Common data for all object types
struct ObjectTypeData_Base
{
	ObjectTypeData_Base(ObjectType type) : type(type){};

	// Position in ms when this object appears
	MapTime time;
	// Type of this object, determines the size of this struct and which type its data is
	ObjectType type;
};

struct MultiObjectState;

// Object state type with contains specific data of type T
template <typename T>
struct TObjectState : public ObjectTypeData_Base, T
{
	TObjectState() : ObjectTypeData_Base(T::staticType){};

	// Implicit down-cast
	operator TObjectState<void> *() { return (TObjectState<void> *)this; }
	operator const TObjectState<void> *() const { return (TObjectState<void> *)this; }
	// Implicit down-cast
	operator MultiObjectState *() { return (MultiObjectState *)this; }
	operator const MultiObjectState *() const { return (MultiObjectState *)this; }
};

// Generic object, does not have an object member
template <>
struct TObjectState<void> : public ObjectTypeData_Base
{
	TObjectState() : ObjectTypeData_Base(ObjectType::Invalid){};

	// Sort object states by their time and other properties
	static void SortArray(std::vector<std::unique_ptr<TObjectState<void>>>& arr);

	// Always allow casting from typeless object to Union State object
	operator MultiObjectState *() { return (MultiObjectState *)this; }
	operator const MultiObjectState *() const { return (MultiObjectState *)this; }
};

// A Single button
struct ObjectTypeData_Button
{
	// The index of the button
	// 0-3 Normal buttons
	// 4-5 FX buttons
	uint8 index = 0xFF;

	// Does this button have a sound sample attached
	bool hasSample = false;

	// Index of the sound sample
	uint8 sampleIndex = 0xFF;

	// Playback volume of the sample
	float sampleVolume = 1.0;

	static const ObjectType staticType = ObjectType::Single;
};
// A Hold button, extends a normal button with duration and effect type
struct ObjectTypeData_Hold : public ObjectTypeData_Button
{
	TObjectState<ObjectTypeData_Hold> *GetRoot();
	// Used for hold notes, 0 is a normal note
	MapTime duration = 0;
	// The sound effect on the note
	EffectType effectType = EffectType::None;
	// The parameter for effects that have it
	// the maximum number of parameters is 2 (only echo uses this)
	int16 effectParams[2] = {0};

	// Set for hold notes that are a continuation of the previous one, but with a different effect
	TObjectState<ObjectTypeData_Hold> *next = nullptr;
	TObjectState<ObjectTypeData_Hold> *prev = nullptr;

	static const ObjectType staticType = ObjectType::Hold;
};

struct SpinStruct
{
	enum SpinType
	{
		None = 0x0,
		Full = 0x1,
		Quarter = 0x2,
		// the side bounce thing
		Bounce = 0x3,
	};
	SpinType type = SpinType::None;
	float direction = 0;
	uint32 duration = 0;
	// for the side bounce thing
	uint32 amplitude = 0;
	uint32 frequency = 0;
	uint32 decay = 0;
};

// A laser segment
struct ObjectTypeData_Laser
{
	// Retrieves the starting laser point
	TObjectState<ObjectTypeData_Laser>* GetRoot();
	inline const TObjectState<ObjectTypeData_Laser>* GetRoot() const
	{
		return const_cast<ObjectTypeData_Laser*>(this)->GetRoot();
	}

	// Ending point of laser
	TObjectState<ObjectTypeData_Laser>* GetTail();
	float GetDirection() const;
	float SamplePosition(MapTime time) const;
	// Convert extended range to normal range
	static float ConvertToNormalRange(float inputRange);
	// Convert normal range to extended range
	static float ConvertToExtendedRange(float inputRange);

	// Time to a direction change, returns -1 if there are no direction changes in this section
	MapTime GetTimeToDirectionChange(MapTime currentTime, MapTime maxDelta);
	// Duration of laser segment
	MapTime duration = 0;
	// 0 or 1 for left and right respectively
	uint8 index = 0;
	// Special options
	uint8 flags = 0;
	// Position of the laser on the track
	float points[2];
	// Set the to the object state that connects to this laser, if any, otherwise null
	TObjectState<ObjectTypeData_Laser> *next = nullptr;
	TObjectState<ObjectTypeData_Laser> *prev = nullptr;

	SpinStruct spin;

	static const ObjectType staticType = ObjectType::Laser;

	// Indicates that this segment is instant and should generate a laser slam segment
	static const uint8 flag_Instant = 0x1;
	// Indicates that the range of this laser is extended from -0.5 to 1.5
	static const uint8 flag_Extended = 0x2;

	// Tick used for more accuracy in some calculations
	uint32 tick;

	// Indicates that the slam has been processed (ensures slam rolls are applied once)
	static const uint8 flag_slamProcessed = 0x4;
};

struct EventData
{
	EventData() = default;
	template <typename T>
	EventData(const T &obj)
	{
		static_assert(sizeof(T) <= 4, "Invalid object size");
		memset(this, 0, 4);
		memcpy(this, &obj, sizeof(T));
	}
	union {
		float floatVal;
		uint32 uintVal;
		int32 intVal;
		uint8 byteVal;
		EffectType effectVal;
		TrackRollBehaviour rollVal;
	};

	// Address of operator that returns a pointer to 32-bit data
	uint32 *operator&()
	{
		return (uint32 *)this;
	}
};

// An event segment, these set various settings at a given point, such as an effect volume, the roll behaviour of the track, etc.
struct ObjectTypeData_Event
{
	// The key value for what value this event is setting
	EventKey key;

	// Always 32 bits of data, but the one used depends on the key
	EventData data;

	// For sorting events that happen on the same tick
	uint32 interTickIndex;

	static const ObjectType staticType = ObjectType::Event;
};

// Object state with union data member
struct MultiObjectState
{
	static bool StaticSerialize(BinaryStream &stream, MultiObjectState *&out);

	// Position in ms when this object appears
	MapTime time;
	// Type of this object, determines the size of this struct and which type its data is
	ObjectType type;
	union {
		ObjectTypeData_Button button;
		ObjectTypeData_Hold hold;
		ObjectTypeData_Laser laser;
		ObjectTypeData_Event event;
	};

	// Implicit down-cast
	operator TObjectState<void> *() { return (TObjectState<void> *)this; }
	operator const TObjectState<void> *() const { return (TObjectState<void> *)this; }
};

// Restore packing
#pragma pack(pop)

typedef TObjectState<void> ObjectState;
typedef TObjectState<ObjectTypeData_Button> ButtonObjectState;
typedef TObjectState<ObjectTypeData_Hold> HoldObjectState;
typedef TObjectState<ObjectTypeData_Laser> LaserObjectState;
typedef TObjectState<ObjectTypeData_Event> EventObjectState;

// Map timing point
struct TimingPoint
{
	static bool StaticSerialize(BinaryStream &stream, TimingPoint *&out);

	double GetWholeNoteLength() const { return beatDuration * 4; }
	double GetBarDuration() const { return GetWholeNoteLength() * ((double)numerator / (double)denominator); }
	double GetBPM() const { return 60000.0 / beatDuration; }

	/// Position in ms when this timing point appears
	MapTime time = 0;
	/// Beat duration of a 4th note in milliseconds (equals 60000.0 / BPM)
	double beatDuration;
	/// Upper part of the time signature (how many beats per bar)
	uint8 numerator = 4;
	/// Lower part of the time signature (the note value (4th, 3th, 8th notes, etc.) for a beat)
	uint8 denominator = 4;
	/// Multiplier for tickrates (x 2^tickrateOffset)
	int8 tickrateOffset = 0;
};

struct LaneHideTogglePoint {
	/// Position in ms when to hide or show the lane
	MapTime time;

	/// How long the transition to/from hidden should take in 1/192nd notes
	uint32 duration = 192;
};