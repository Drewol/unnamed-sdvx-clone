#pragma once
#include <Beatmap/BeatmapPlayback.hpp>
#include "HitStat.hpp"
#include "Input.hpp"
#include "Game.hpp"

#define AUTOPLAY_BUTTON_HIT_DURATION (4 / 60.f)

enum class TickFlags : uint8
{
	None = 0,
	// Used for segment start/end parts
	Start = 0x1,
	End = 0x2,
	// Hold notes (BT or FX)
	Hold = 0x4,
	// Normal/Single hit buttons
	Button = 0x8,
	// For lasers only
	Laser = 0x10,
	Slam = 0x20,

	// Used to make hit effects appear correctly for holds
	Ignore = 0x40,

	Processed = 0x80
};
TickFlags operator|(const TickFlags& a, const TickFlags& b);
TickFlags operator&(const TickFlags& a, const TickFlags& b);

// Tick object to record hits
struct ScoreTick
{
public:
	ScoreTick() = default;
	ScoreTick(ObjectState* object) : object(object) {};

	// Hit rating when hitting object give a delta 
	ScoreHitRating GetHitRatingFromDelta(const HitWindow& hitWindow, MapTime delta) const;
	// Check a flag
	bool HasFlag(TickFlags flag) const;
	void SetFlag(TickFlags flag);

	TickFlags flags = TickFlags::None;
	MapTime time;
	ObjectState* object = nullptr;
};

struct AutoplayInfo
{
    // Autoplay mode
    bool autoplay = false;
    // Autoplay but for buttons
    bool autoplayButtons = false;
    float buttonAnimationTimer[6] = { 0 };

    bool IsAutoplayButtons() const { return autoplay || autoplayButtons; };
};

// Various information about all the objects in a map
struct MapTotals
{
	// Number of single notes
	uint32 numSingles;
	// Number of laser/hold ticks
	uint32 numTicks;
	// The maximum possible score a map can give
	// The score is calculated per 2 (2 = critical, 1 = near)
	// Hold buttons, lasers, etc. give 2 points per tick
	uint32 maxScore;
};

/*
	Calculates game score and checks which objects are hit
	also keeps track of laser positions
*/
class Scoring : public Unique
{
public:
	Scoring();
	~Scoring();

	static ClearMark CalculateBadge(const ScoreIndex& score);
	static ClearMark CalculateBestBadge(Vector<ScoreIndex*> scores);

	// Needs to be set to find out which objects are active/hittable
	void SetPlayback(BeatmapPlayback& playback);

	// Needs to be set to handle input
	void SetInput(Input* input);

	void SetOptions(PlaybackOptions opts);
	void SetEndTime(MapTime time);

	inline void SetHitWindow(const HitWindow& window) { hitWindow = window; }

	// Resets/Initializes the scoring system
	// Called after SetPlayback
	void Reset(const MapTimeRange& range = {});

	void FinishGame();

	// Updates the list of objects that are possible to hit
	void Tick(float deltaTime);

	float GetLaserPosition(uint32 index, float pos);
	float GetLaserRollOutput(uint32 index);
	// Check if any lasers are currently active
	bool GetLaserActive();
	bool GetFXActive();
	float GetLaserOutput();

	float GetMeanHitDelta(bool absolute = false);
	int16 GetMedianHitDelta(bool absolute = false);

	// Check if an object is currently held
	//	works only for lasers and hold buttons
	bool IsObjectHeld(ObjectState* object);
	// Check if an object is currently held, by object index
	//	Buttons[0,5], Lasers[6,7]
	bool IsObjectHeld(uint32 index) const;
	// Check if a laser is currently held
	bool IsLaserHeld(uint32 laserIndex, bool includeSlams = true) const;

	// Checks if a laser is currently not used or needed soon
	bool IsLaserIdle(uint32 index) const;

	bool IsFailOut() const;
	class Gauge* GetTopGauge() const;
	void SetAllGaugeValues(const Vector<float>, bool zeroRest=true);
	void GetAllGaugeValues(Vector<float>& out) const;

	// Calculates the maximum score of the current map
	MapTotals CalculateMapTotals() const;

	// Actual score, in the range 0-10,000,000
	uint32 CalculateCurrentScore() const;
	uint32 CalculateScore(uint32 hitScore) const;

	uint32 CalculateCurrentDisplayScore() const;
	uint32 CalculateCurrentDisplayScore(const ScoreReplay& replay) const;
	uint32 CalculateCurrentDisplayScore(uint32 currHit, uint32 currMaxHit) const;

	// The score if the rest would be played perfectly
	uint32 CalculateCurrentMaxPossibleScore() const;
	uint32 CalculateCurrentMaxPossibleScore(uint32 currHit, uint32 currMaxHit) const;

	// The score based on the current pace
	uint32 CalculateCurrentAverageScore(uint32 currHit, uint32 currMaxHit) const;

	inline uint32 GetMisses() const { return categorizedHits[0]; }
	inline uint32 GetGoods() const { return categorizedHits[1]; }
	inline uint32 GetPerfects() const { return categorizedHits[2]; }
	inline bool IsPerfect() const { return GetMisses() == 0 && GetGoods() == 0; }
	inline bool IsFullCombo() const { return GetMisses() == 0; }

	bool HoldObjectAvailable(uint32 index, bool checkIfPassedCritLine);

	// Called when a hit is recorded on a given button index (excluding hold notes)
	// (Hit Button, Score, Hit Object(optional))
	Delegate<Input::Button, ScoreHitRating, ObjectState*, MapTime> OnButtonHit;
	// Called when a miss is recorded on a given button index
	Delegate<Input::Button, bool, ObjectState*> OnButtonMiss;

	// Called when an object is picked up
	Delegate<Input::Button, ObjectState*> OnObjectHold;
	// Called when an object is let go of
	Delegate<Input::Button, ObjectState*> OnObjectReleased;

	Delegate<Input::Button> OnHoldEnter;
	Delegate<Input::Button> OnHoldLeave;

	// Called when a laser slam was hit
	// (Laser slam segment)
	Delegate<LaserObjectState*> OnLaserSlamHit;
	// Called during a laser slam
	Delegate<LaserObjectState*> OnLaserSlam;
	// Called when a laser has passed the crit line
	Delegate<LaserObjectState*> OnLaserExit;
	// Called when the combo counter changed
	// (New Combo)
	Delegate<uint32> OnComboChanged;

	// Called when score has changed
	//	(New Score)
	Delegate<> OnScoreChanged;

	Delegate<class Gauge*, class Gauge*> OnGaugeChanged;

	// Object timing window
	HitWindow hitWindow = HitWindow::NORMAL;

	// Map total infos
	MapTotals mapTotals;
	// Maximum accumulated score of object that have been hit or missed
	// used to calculate accuracy up to a give point
	uint32 currentMaxScore = 0;
	// The actual amount of gotten score
	uint32 currentHitScore = 0;

	// Hits per type in order:
	//	0 = Miss
	//	1 = Good
	//	2 = Perfect
	uint32 categorizedHits[3] = { 0 };

	// Early and Late count:
	// 0 = Early
	// 1 = Late
	uint32 timedHits[2] = { 0 };

	// Current combo
	uint32 currentComboCounter;

	// Combo state (0 = regular, 1 = full combo, 2 = perfect)
	uint8 comboState = 2;

	// Highest combo in current run
	uint32 maxComboCounter;

	// The timings of hit objects, sorted by time hit
	// these are used for debugging
	Vector<HitStat*> hitStats;

	struct AutoplayInfo autoplayInfo;

	// Actual positions of the laser
	float laserPositions[2];
	// Sampled target position of the lasers in the map
	float laserTargetPositions[2] = { 0 };
	// Current lasers are extended
	bool lasersAreExtend[2] = { false, false };
	// Time since laser has been used
	float timeSinceLaserUsed[2];
private:
	// Calculates the number of ticks for a given TP
	double m_CalculateTicks(const TimingPoint* tp) const;
	// Calculates the times at which a single hold object ticks
	void m_CalculateHoldTicks(HoldObjectState* hold, Vector<MapTime>& ticks) const;
	// Calculates the times at which a single laser chain object ticks
	//	use the root laser object
	void m_CalculateLaserTicks(LaserObjectState* laserRoot, Vector<ScoreTick>& ticks) const;
	void m_OnObjectEntered(ObjectState* obj);
	void m_OnObjectLeaved(ObjectState* obj);
	void m_OnFXBegin(HoldObjectState* obj);

	// Button event handlers
	void m_OnButtonPressed(Input::Button buttonCode);
	void m_OnButtonReleased(Input::Button buttonCode);
	void m_CleanupInput();

	// Updates all pending ticks
	void m_UpdateTicks();
	// Tries to trigger a hit event on an approaching tick
	ObjectState* m_ConsumeTick(uint32 buttonCode);
	// Called whenether missed or not
	void m_OnTickProcessed(ScoreTick* tick, uint32 index);
	void m_TickHit(ScoreTick* tick, uint32 index, MapTime delta = 0);
	void m_TickMiss(ScoreTick* tick, uint32 index, MapTime delta);
	void m_UpdateGauges(ScoreHitRating rating, TickFlags flags);
	void m_UpdateGaugeSamples();
	void m_CleanupTicks();
	void m_CleanupGauges();

	// Called when score is gained
	//	should only be called once for a single object since this also increments the combo counter
	void m_AddScore(uint32 score);
	// Reset combo counter
	void m_ResetCombo();

	// Sets a held object
	void m_SetHoldObject(ObjectState* obj, uint32 index);
	void m_ReleaseHoldObject(ObjectState* obj);
	void m_ReleaseHoldObject(uint32 index);
	bool m_IsBeingHeld(const ScoreTick* tick) const;

	// Check whether the laser segment is the beginning
	bool m_IsRoot(const LaserObjectState* laser) const;
	bool m_IsRoot(const HoldObjectState* hold) const;

	// Updates the target laser positions and currently tracked laser segments for those
	//  also updates laser input and returns lasers back to idle position when not used
	void m_UpdateLasers(float deltaTime);

	// Returns the raw laser output value, not interpolated
	float m_GetLaserOutputRaw();
	void m_UpdateLaserOutput(float deltaTime);

	// Creates or retrieves an existing hit stat and returns it
	HitStat* m_AddOrUpdateHitStat(ObjectState* object);
	void m_CleanupHitStats();

	LaserObjectState* m_GetLaserObjectWithinTwoBeats(uint8 index);

	// Updates laser output with or without interpolation
	bool m_interpolateLaserOutput = false;

	// Lerp for laser output
	float m_laserOutputSource = 0.0f;
	float m_laserOutputTarget = 0.0f;
	float m_timeSinceOutputSet = 0.0f;

	class Input* m_input = nullptr;
	class BeatmapPlayback* m_playback = nullptr;

	// Input values for laser [-1,1]
	float m_laserInput[2] = { 0.0f };
	// Decides if the coming tick should be auto completed
	float m_autoLaserTime[2] = { 0.0f };
	const double m_laserDistanceLeniency = 1 / 6.;
	const float m_autoLaserDuration = 4 / 60.f;
	const float m_autoLaserDurationAfterSlam = 8 / 60.f;
	
	// Saves the time when a button was hit, used to decide if a button was held before a hold object was active
	MapTime m_buttonHitTime[6] = { 0, 0, 0, 0, 0, 0 };
	MapTime m_buttonReleaseTime[6] = { 0, 0, 0, 0, 0, 0 };
	// Saves the time when a button was hit or released for bounce guarding
	MapTime m_buttonGuardTime[6] = { 0, 0, 0, 0, 0, 0 };

	// Offet to use for calculating judge (ms)
	int32 m_inputOffset = 0;
	int32 m_laserOffset = 0;
	int32 m_bounceGuard = 0;
	float m_drainMultiplier = 1.0f;
	MapTime m_endTime = 180000;

	// used the update the amount of hit ticks for hold/laser notes
	Map<ObjectState*, HitStat*> m_holdHitStats;

	// Laser objects currently in range
	//	used to sample target laser positions
	LaserObjectState* m_currentLaserSegments[2] = { nullptr };
	// Queue for the above list
	Vector<LaserObjectState*> m_laserSegmentQueue;

	// Ticks for each BT[4] / FX[2] / Laser[2]
	Vector<ScoreTick*> m_ticks[8];

	// Hold objects
	ObjectState* m_holdObjects[8];
	Set<ObjectState*> m_heldObjects;
	bool m_prevHoldHit[6];

	PlaybackOptions m_options;
	MapTimeRange m_range;

	// A stack of gauges which are all calculated at the same time.
	// The top gauge is what the user should see and if that one raches its fail state
	// then the next gauge is to be used. If the last gauge fails out then the player
	// shall be put on the score screen.
	Vector<class Gauge*> m_gaugeStack;
};
