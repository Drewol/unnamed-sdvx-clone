#include "stdafx.h"
#include "Scoring.hpp"
#include <Beatmap/BeatmapPlayback.hpp>
#include <math.h>
#include "GameConfig.hpp"

const float Scoring::idleLaserSpeed = 1.0f;

Scoring::Scoring()
{
}
Scoring::~Scoring()
{
	m_CleanupInput();
	m_CleanupHitStats();
	m_CleanupTicks();
}

ClearMark Scoring::CalculateBadge(const ScoreIndex& score)
{
	if (score.score >= static_cast<int32>(MAX_SCORE)) //Perfect
		return ClearMark::Perfect;
	if (score.miss == 0) //Full Combo
		return ClearMark::FullCombo;
	if (((GameFlags)score.gameflags & GameFlags::Hard) != GameFlags::None && score.gauge > 0) //Hard Clear
		return ClearMark::HardClear;
	if (((GameFlags)score.gameflags & GameFlags::Hard) == GameFlags::None && score.gauge >= 0.70) //Normal Clear
		return ClearMark::NormalClear;

	return ClearMark::Played; //Failed
}

ClearMark Scoring::CalculateBestBadge(Vector<ScoreIndex*> scores)
{
	if (scores.size() < 1)
		return ClearMark::NotPlayed;

	ClearMark top = ClearMark::Played;

	for (ScoreIndex* score : scores)
	{
		ClearMark temp = CalculateBadge(*score);
		if (temp > top) top = temp;
	}

	return top;
}

void Scoring::SetPlayback(BeatmapPlayback& playback)
{
	if (m_playback)
	{
		m_playback->OnObjectEntered.RemoveAll(this);
		m_playback->OnObjectLeaved.RemoveAll(this);
	}
	m_playback = &playback;
	//m_playback->OnFXBegin.Add(this, &Scoring::m_OnFXBegin);
	m_playback->OnObjectEntered.Add(this, &Scoring::m_OnObjectEntered);
	m_playback->OnObjectLeaved.Add(this, &Scoring::m_OnObjectLeaved);
}

void Scoring::SetInput(Input* input)
{
	m_CleanupInput();
	if (input)
	{
		m_input = input;
		m_input->OnButtonPressed.Add(this, &Scoring::m_OnButtonPressed);
		m_input->OnButtonReleased.Add(this, &Scoring::m_OnButtonReleased);
	}
}
void Scoring::SetFlags(GameFlags flags)
{
	m_flags = flags;
}
void Scoring::SetEndTime(MapTime time)
{
	m_endTime = time;
}
void Scoring::m_CleanupInput()
{
	if (m_input)
	{
		m_input->OnButtonPressed.RemoveAll(this);
		m_input->OnButtonReleased.RemoveAll(this);
		m_input = nullptr;
	}
}

void Scoring::Reset(const MapTimeRange& range)
{
	{
		MapTime begin = range.begin;
		MapTime end = range.end;

		// Ensure that nothing could go wrong when the start is 0
		if (begin <= 0)
		{
			begin = std::numeric_limits<decltype(begin)>::min();
			if (!range.HasEnd()) end = begin;
		}

		m_range = { begin, end };
	}

	// Reset score/combo counters
	currentMaxScore = 0;
	currentHitScore = 0;
	currentComboCounter = 0;
	maxComboCounter = 0;
	comboState = 2;
	m_assistTime = m_assistLevel * 0.1f;

	// Reset laser positions
	laserTargetPositions[0] = 0.0f;
	laserTargetPositions[1] = 0.0f;
	laserPositions[0] = 0.0f;
	laserPositions[1] = 1.0f;
	timeSinceLaserUsed[0] = 1000.0f;
	timeSinceLaserUsed[1] = 1000.0f;

	memset(categorizedHits, 0, sizeof(categorizedHits));
	memset(timedHits, 0, sizeof(timedHits));
	// Clear hit statistics
	hitStats.clear();

	// Get input offset
	m_inputOffset = g_gameConfig.GetInt(GameConfigKeys::InputOffset);
	// Get bounce guard duration
	m_bounceGuard = g_gameConfig.GetInt(GameConfigKeys::InputBounceGuard);
	// Get laser assist level
	m_assistLevel = g_gameConfig.GetFloat(GameConfigKeys::LaserAssistLevel);
	m_assistPunish = g_gameConfig.GetFloat(GameConfigKeys::LaserPunish);
	m_assistChangeExponent = g_gameConfig.GetFloat(GameConfigKeys::LaserChangeExponent);
	m_assistChangePeriod = g_gameConfig.GetFloat(GameConfigKeys::LaserChangeTime);

	// Recalculate maximum score
	mapTotals = CalculateMapTotals();

	// Recalculate gauge gain
	currentGauge = 0.0f;
	float total = 2.10f + 0.001f; //Add a little in case floats go under
	bool manualTotal = m_playback->GetBeatmap().GetMapSettings().total > 99;
	if (manualTotal)
	{
		total = (float)m_playback->GetBeatmap().GetMapSettings().total / 100.0f + 0.001f;
	}
	if ((m_flags & GameFlags::Hard) != GameFlags::None)
	{
		total *= 12.f / 21.f;
		currentGauge = 1.0f;
	}

	if (mapTotals.numTicks == 0 && mapTotals.numSingles != 0)
	{
		shortGaugeGain = total / (float)mapTotals.numSingles;
	}
	else if (mapTotals.numSingles == 0 && mapTotals.numTicks != 0)
	{
		tickGaugeGain = total / (float)mapTotals.numTicks;
	}
	else
	{
		shortGaugeGain = (total * 20) / (5.0f * ((float)mapTotals.numTicks + (4.0f * (float)mapTotals.numSingles)));
		tickGaugeGain = shortGaugeGain / 4.0f;
	}

	if (manualTotal)
	{
		m_drainMultiplier = 1.0;
	}
	else
	{
		MapTime drainNormal, drainHalf;
		drainNormal = g_gameConfig.GetInt(GameConfigKeys::GaugeDrainNormal);
		drainHalf = g_gameConfig.GetInt(GameConfigKeys::GaugeDrainHalf);

		double secondsOver = ((double)m_endTime / 1000.0) - (double)drainNormal;
		secondsOver = Math::Max(0.0, secondsOver);
		m_drainMultiplier = 1.0 / (1.0 + (secondsOver / (double)(drainHalf - drainNormal)));
	}

	m_heldObjects.clear();
	
	memset(m_holdObjects, 0, sizeof(m_holdObjects));
	memset(m_prevHoldHit, 0, sizeof(m_prevHoldHit));
	memset(m_currentLaserSegments, 0, sizeof(m_currentLaserSegments));

	memset(m_buttonHitTime, 0, sizeof(m_buttonHitTime));
	memset(m_buttonReleaseTime, 0, sizeof(m_buttonReleaseTime));
	memset(m_buttonGuardTime, 0, sizeof(m_buttonGuardTime));

	m_CleanupHitStats();
	m_CleanupTicks();

	OnScoreChanged.Call();
	OnComboChanged.Call(0);
}

void Scoring::FinishGame()
{
	m_CleanupInput();
	m_CleanupTicks();
	for (size_t i = 0; i < 8; i++)
	{
		m_ReleaseHoldObject(i);
	}
}

void Scoring::Tick(float deltaTime)
{
	m_UpdateLasers(deltaTime);
	m_UpdateTicks();
	if (autoplay || autoplayButtons)
	{
		for (size_t i = 0; i < 6; i++)
		{
			if (m_ticks[i].size() > 0)
			{
				auto tick = m_ticks[i].front();
				if (tick->HasFlag(TickFlags::Hold))
				{
					if (tick->object->time <= m_playback->GetLastTime())
						m_SetHoldObject(tick->object, i);
				}
			}
		}
	}
}

float Scoring::GetLaserPosition(uint32 index, float pos)
{
	assert(index == 0 || index == 1);
	return index == 0 ? -pos : 1.f - pos;
}

float Scoring::GetLaserRollOutput(uint32 index)
{
	assert(index <= 1);
	// Ignore slams that are the last segment since Camera handles slam behaviour
	if (m_currentLaserSegments[index] && !(m_currentLaserSegments[index]->flags & LaserObjectState::flag_Instant &&
			!m_currentLaserSegments[index]->next))
	{
		return GetLaserPosition(index, laserTargetPositions[index]);
	}
	else // Check if any upcoming lasers are within 2 beats
	{
		for (auto l : m_laserSegmentQueue)
		{
			if (l->index == index && m_IsRoot(l))
			{
				if (l->time - m_playback->GetLastTime() <= m_playback->GetCurrentTimingPoint().beatDuration * 2)
				{
					return GetLaserPosition(index, l->points[0]);
				}
			}
		}
			
	}
	return 0.0f;
}

bool Scoring::GetLaserActive()
{
	return IsObjectHeld(6) || IsObjectHeld(7);
}

bool Scoring::GetFXActive()
{
	return IsObjectHeld(4) || IsObjectHeld(5);
}

static const float laserOutputInterpolationDuration = 0.1f;
float Scoring::GetLaserOutput()
{
	float f = Math::Min(1.0f, m_timeSinceOutputSet / laserOutputInterpolationDuration);
	return m_laserOutputSource + (m_laserOutputTarget - m_laserOutputSource) * f;
}
float Scoring::GetMeanHitDelta(bool absolute)
{
	float sum = 0;
	uint32 count = 0;
	for (auto hit : hitStats)
	{
		if (hit->object->type != ObjectType::Single || hit->rating == ScoreHitRating::Miss)
			continue;
		sum += absolute ? abs(hit->delta) : hit->delta;
		count++;
	}
	if (count == 0)
		return 0.0f;
	return sum / count;
}
int16 Scoring::GetMedianHitDelta(bool absolute)
{
	Vector<MapTime> deltas;
	for (auto hit : hitStats)
	{
		if (hit->object->type != ObjectType::Single || hit->rating == ScoreHitRating::Miss)
			continue;
		deltas.Add(absolute ? abs(hit->delta) : hit->delta);
	}
	if (deltas.size() == 0)
		return 0;
	std::sort(deltas.begin(), deltas.end());
	return deltas[deltas.size() / 2];
}
float Scoring::m_GetLaserOutputRaw()
{
	float val = 0.0f;
	for (int32 i = 0; i < 2; i++)
	{
		if (IsLaserHeld(i) && m_currentLaserSegments[i])
		{
			// Skip single or end slams
			if (!m_currentLaserSegments[i]->next && (m_currentLaserSegments[i]->flags & LaserObjectState::flag_Instant) != 0)
				continue;

			float actual = laserTargetPositions[i];
			// Undo laser extension
			if ((m_currentLaserSegments[i]->flags & LaserObjectState::flag_Extended) != 0)
			{
				actual += 0.5f;
				actual *= 0.5f;
				assert(actual >= 0.0f && actual <= 1.0f);
			}
			if (i == 1) // Second laser goes the other way
				actual = 1.0f - actual;
			val = Math::Max(actual, val);
		}
	}
	return val;
}
void Scoring::m_UpdateLaserOutput(float deltaTime)
{
	m_timeSinceOutputSet += deltaTime;
	float v = m_GetLaserOutputRaw();
	if (v != m_laserOutputTarget)
	{
		m_laserOutputTarget = v;
		m_laserOutputSource = GetLaserOutput();
		m_timeSinceOutputSet = m_interpolateLaserOutput ? 0.0f : laserOutputInterpolationDuration;
	}
}

HitStat* Scoring::m_AddOrUpdateHitStat(ObjectState* object)
{
	if (object->type == ObjectType::Single)
	{
		HitStat* stat = new HitStat(object);
		hitStats.Add(stat);
		return stat;
	}
	else if (object->type == ObjectType::Hold)
	{
		HoldObjectState* hold = (HoldObjectState*)object;
		HitStat** foundStat = m_holdHitStats.Find(object);
		if (foundStat)
			return *foundStat;
		HitStat* stat = new HitStat(object);
		hitStats.Add(stat);
		m_holdHitStats.Add(object, stat);

		// Get tick count
		Vector<MapTime> ticks;
		m_CalculateHoldTicks(hold, ticks);
		stat->holdMax = (uint32)ticks.size();
		stat->forReplay = false;

		return stat;
	}
	else if (object->type == ObjectType::Laser)
	{
		LaserObjectState* rootLaser = ((LaserObjectState*)object)->GetRoot();
		HitStat** foundStat = m_holdHitStats.Find(*rootLaser);
		if (foundStat)
			return *foundStat;
		HitStat* stat = new HitStat(*rootLaser);
		hitStats.Add(stat);
		m_holdHitStats.Add(object, stat);

		// Get tick count
		Vector<ScoreTick> ticks;
		m_CalculateLaserTicks(rootLaser, ticks);
		stat->holdMax = (uint32)ticks.size();
		stat->forReplay = false;

		return stat;
	}

	// Shouldn't get here
	assert(false);
	return nullptr;
}

void Scoring::m_CleanupHitStats()
{
	for (HitStat* hit : hitStats)
		delete hit;
	hitStats.clear();
	m_holdHitStats.clear();
}

bool Scoring::IsObjectHeld(ObjectState* object)
{
	if (object->type == ObjectType::Laser)
	{
		// Select root node of laser
		object = *((LaserObjectState*)object)->GetRoot();
	}
	else if (object->type == ObjectType::Hold)
	{
		// Check all hold notes in a hold sequence to see if it is held
		bool held = false;
		HoldObjectState* root = ((HoldObjectState*)object)->GetRoot();
		while (root != nullptr)
		{
			if (m_heldObjects.Contains(*root))
			{
				held = true;
				break;
			}
			root = root->next;
		}
		return held;
	}

	return m_heldObjects.Contains(object);
}
bool Scoring::IsObjectHeld(uint32 index) const
{
	assert(index < 8);
	return m_holdObjects[index] != nullptr;
}
bool Scoring::IsLaserHeld(uint32 laserIndex, bool includeSlams) const
{
	if (includeSlams)
		return IsObjectHeld(laserIndex + 6);

	if (m_holdObjects[laserIndex + 6])
	{
		// Check for slams
		return (((LaserObjectState*)m_holdObjects[laserIndex + 6])->flags & LaserObjectState::flag_Instant) == 0;
	}
	return false;
}

bool Scoring::IsLaserIdle(uint32 index) const
{
	return m_laserSegmentQueue.empty() && m_currentLaserSegments[0] == nullptr && m_currentLaserSegments[1] == nullptr;
}

double Scoring::m_CalculateTicks(const TimingPoint* tp) const
{
	// Tick rate based on BPM
	float offset = powf(2.0, tp->tickrateOffset);
	const double tickNoteValue = (16 / (pow(2, Math::Max((int)(log2(tp->GetBPM())) - 7, 0)))) * offset;
	return tp->GetWholeNoteLength() / tickNoteValue;
}

void Scoring::m_CalculateHoldTicks(HoldObjectState* hold, Vector<MapTime>& ticks) const
{
	const TimingPoint* tp = m_playback->GetTimingPointAt(hold->time);

	// Tick rate based on BPM
	double tickInterval = m_CalculateTicks(tp);

	double tickpos = hold->time;
	if (m_IsRoot(hold)) // no tick at the very start of a hold
	{
		tickpos += tickInterval;
	}
	while (tickpos < hold->time + hold->duration - tickInterval)
	{
		ticks.Add((MapTime)tickpos);
		tickpos += tickInterval;
	}
	if (ticks.size() == 0)
	{
		ticks.Add(hold->time + (hold->duration / 2));
	}
}
void Scoring::m_CalculateLaserTicks(LaserObjectState* laserRoot, Vector<ScoreTick>& ticks) const
{
	assert(m_IsRoot(laserRoot));
	const TimingPoint* tp = m_playback->GetTimingPointAt(laserRoot->time);

	// Tick rate based on BPM
	double tickInterval = m_CalculateTicks(tp);

	LaserObjectState* sectionStart = laserRoot;
	MapTime sectionStartTime = laserRoot->time;
	MapTime combinedDuration = 0;
	LaserObjectState* lastSlam = nullptr;
	auto AddTicks = [&]()
	{
		uint32 numTicks = (uint32)Math::Floor((double)combinedDuration / tickInterval);
		for (uint32 i = 0; i < numTicks; i++)
		{
			if (lastSlam && i == 0) // No first tick if connected to slam
				continue;

			ScoreTick& t = ticks.Add(ScoreTick(*sectionStart));
			t.time = sectionStartTime + (MapTime)(tickInterval * (double)i);
			t.flags = TickFlags::Laser;

			// Link this tick to the correct segment
			if (sectionStart->next && (sectionStart->time + sectionStart->duration) <= t.time)
			{
				assert((sectionStart->next->flags & LaserObjectState::flag_Instant) == 0);
				t.object = *(sectionStart = sectionStart->next);
			}


			if (!lastSlam && i == 0)
				t.SetFlag(TickFlags::Start);
		}
		combinedDuration = 0;
	};

	for (auto it = laserRoot; it; it = it->next)
	{
		if ((it->flags & LaserObjectState::flag_Instant) != 0)
		{
			AddTicks();
			ScoreTick& t = ticks.Add(ScoreTick(*it));
			t.time = it->time;
			t.flags = TickFlags::Laser | TickFlags::Slam;
			if (m_IsRoot(it))
				t.SetFlag(TickFlags::Start);
			lastSlam = it;
			if (it->next)
			{
				sectionStart = it->next;
				sectionStartTime = it->next->time;
			}
			else
			{
				sectionStart = nullptr;
				sectionStartTime = it->time;
			}
		}
		else
		{
			combinedDuration += it->duration;
		}
	}
	AddTicks();
	if (ticks.size() > 0)
		ticks.back().SetFlag(TickFlags::End);
}
void Scoring::m_OnFXBegin(HoldObjectState* obj)
{
	if (autoplay || autoplayButtons)
		m_SetHoldObject((ObjectState*)obj, obj->index);
}

void Scoring::m_OnObjectEntered(ObjectState* obj)
{
	// The following code registers which ticks exist depending on the object type / duration
	if (obj->type == ObjectType::Single)
	{
		ButtonObjectState* bt = (ButtonObjectState*)obj;
		ScoreTick* t = m_ticks[bt->index].Add(new ScoreTick(obj));
		t->time = bt->time;
		t->SetFlag(TickFlags::Button);

	}
	else if (obj->type == ObjectType::Hold)
	{
		const TimingPoint* tp = m_playback->GetTimingPointAt(obj->time);
		HoldObjectState* hold = (HoldObjectState*)obj;

		// Add all hold ticks
		Vector<MapTime> holdTicks;
		m_CalculateHoldTicks(hold, holdTicks);
		for (size_t i = 0; i < holdTicks.size(); i++)
		{
			ScoreTick* t = m_ticks[hold->index].Add(new ScoreTick(obj));
			t->SetFlag(TickFlags::Hold);
			if (i == 0 && m_IsRoot(hold))
				t->SetFlag(TickFlags::Start);
			if (i == holdTicks.size() - 1 && !hold->next)
				t->SetFlag(TickFlags::End);
			t->time = holdTicks[i];
		}
	}
	else if (obj->type == ObjectType::Laser)
	{
		LaserObjectState* laser = (LaserObjectState*)obj;
		if (m_IsRoot(laser)) // Only register root laser objects
		{
			// Can cause problems if the previous laser segment hasnt ended yet for whatever reason
			if (!m_currentLaserSegments[laser->index])
			{
				bool anyInQueue = false;
				for (auto l : m_laserSegmentQueue)
				{
					if (l->index == laser->index)
					{
						anyInQueue = true;
						break;
					}
				}
				if (!anyInQueue)
				{
					timeSinceLaserUsed[laser->index] = 0;
					laserPositions[laser->index] = laser->points[0];
					laserTargetPositions[laser->index] = laser->points[0];
					lasersAreExtend[laser->index] = laser->flags & LaserObjectState::flag_Extended;
				}
			}
			// All laser ticks, including slam segments
			Vector<ScoreTick> laserTicks;
			m_CalculateLaserTicks(laser, laserTicks);
			for (size_t i = 0; i < laserTicks.size(); i++)
			{
				// Add copy
				m_ticks[laser->index + 6].Add(new ScoreTick(laserTicks[i]));
			}
		}

		// Add to laser segment queue
		m_laserSegmentQueue.Add(laser);
	}
}
void Scoring::m_OnObjectLeaved(ObjectState* obj)
{
	if (obj->type == ObjectType::Laser)
	{
		LaserObjectState* laser = (LaserObjectState*)obj;
		if (laser->next != nullptr)
			return; // Only terminate holds on last of laser section
		obj = *laser->GetRoot();
	}
	m_ReleaseHoldObject(obj);
}

void Scoring::m_UpdateTicks()
{
	const MapTime currentTime = m_playback->GetLastTime();

	// This loop checks for ticks that are missed
	for (uint32 buttonCode = 0; buttonCode < 8; buttonCode++)
	{
		Input::Button button = (Input::Button) buttonCode;

		// List of ticks for the current button code
		auto& ticks = m_ticks[buttonCode];
		for (uint32 i = 0; i < ticks.size(); i++)
		{
			ScoreTick* tick = ticks[i];
			MapTime delta = currentTime - ticks[i]->time + m_inputOffset;
			bool shouldMiss = abs(delta) > tick->GetHitWindow(hitWindow);
			bool processed = false;
			if (delta >= 0)
			{
				if (tick->HasFlag(TickFlags::Button) && (autoplay || autoplayButtons))
				{
					m_TickHit(tick, buttonCode, 0);
					processed = true;
				}

				if (tick->HasFlag(TickFlags::Hold))
				{
					assert(buttonCode < 6);
					if (m_IsBeingHold(tick) || autoplay || autoplayButtons)
					{
						m_TickHit(tick, buttonCode);
						HitStat* stat = new HitStat(tick->object);
						stat->time = currentTime;
						stat->rating = ScoreHitRating::Perfect;
						hitStats.Add(stat);

						m_prevHoldHit[buttonCode] = true;
					}
					else
					{
						m_TickMiss(tick, buttonCode, 0);

						m_prevHoldHit[buttonCode] = false;
					}
					processed = true;
				}
				else if (tick->HasFlag(TickFlags::Laser))
				{
					LaserObjectState* laserObject = (LaserObjectState*)tick->object;
					if (tick->HasFlag(TickFlags::Slam))
					{
						// Check if slam hit
						float dirSign = Math::Sign(laserObject->GetDirection());
						float inputSign = Math::Sign(m_input->GetInputLaserDir(buttonCode - 6));
						if (autoplay)
						{
							inputSign = dirSign;
						}
						if (dirSign == inputSign && delta > -10)
						{
							m_TickHit(tick, buttonCode);
							HitStat* stat = new HitStat(tick->object);
							stat->time = currentTime;
							stat->rating = ScoreHitRating::Perfect;
							hitStats.Add(stat);
							processed = true;
						}
					}
					else
					{
						// Snap to first laser tick
						/// TODO: Find better solution
						if (tick->HasFlag(TickFlags::Start))
						{
							laserPositions[laserObject->index] = laserTargetPositions[laserObject->index];
							m_autoLaserTime[laserObject->index] = m_assistTime;
						}

						// Check laser input
						float laserDelta = fabs(laserPositions[laserObject->index] - laserTargetPositions[laserObject->index]); \

						if (laserDelta < laserDistanceLeniency)
						{
							m_TickHit(tick, buttonCode);
							HitStat* stat = new HitStat(tick->object);
							stat->time = currentTime;
							stat->rating = ScoreHitRating::Perfect;
							hitStats.Add(stat);
							processed = true;
						}
					}
				}
			}
			else if (tick->HasFlag(TickFlags::Slam) && !shouldMiss)
			{
				LaserObjectState* laserObject = (LaserObjectState*)tick->object;
				// Check if slam hit
				float dirSign = Math::Sign(laserObject->GetDirection());
				float inputSign = Math::Sign(m_input->GetInputLaserDir(buttonCode - 6));
				if (dirSign == inputSign)
				{
					m_TickHit(tick, buttonCode);
					HitStat* stat = new HitStat(tick->object);
					stat->time = currentTime;
					stat->rating = ScoreHitRating::Perfect;
					hitStats.Add(stat);
					processed = true;
				}
			}

			if (delta > hitWindow.good && !processed)
			{
				m_TickMiss(tick, buttonCode, delta);
				processed = true;
			}

			if (processed)
			{
				delete tick;
				ticks.Remove(tick, false);
				i--;
			}
			else
			{
				// No further ticks to process
				break;
			}
		}
	}
}
ObjectState* Scoring::m_ConsumeTick(uint32 buttonCode)
{
	const MapTime currentTime = m_playback->GetLastTime() + m_inputOffset;
	assert(buttonCode < 8);

	if (m_ticks[buttonCode].size() > 0)
	{
		ScoreTick* tick = m_ticks[buttonCode].front();

		const MapTime delta = currentTime - tick->time;
		ObjectState* hitObject = tick->object;
		if (tick->HasFlag(TickFlags::Laser))
		{
			// Ignore laser and hold ticks
			return nullptr;
		}
		else if (tick->HasFlag(TickFlags::Hold))
		{
			HoldObjectState* hos = (HoldObjectState*)hitObject;
			hos = hos->GetRoot();
			if (hos->time - hitWindow.hold <= currentTime)
				m_SetHoldObject(hitObject, buttonCode);
			return nullptr;
		}
		if (abs(delta) <= hitWindow.good)
			m_TickHit(tick, buttonCode, delta);
		else
			m_TickMiss(tick, buttonCode, delta);
		delete tick;
		m_ticks[buttonCode].Remove(tick, false);

		return hitObject;
	}
	return nullptr;
}

void Scoring::m_OnTickProcessed(ScoreTick* tick, uint32 index)
{
	currentMaxScore += (uint32)ScoreHitRating::Perfect;

	if (OnScoreChanged.IsHandled())
	{
		OnScoreChanged.Call();
	}
}
void Scoring::m_TickHit(ScoreTick* tick, uint32 index, MapTime delta /*= 0*/)
{
	HitStat* stat = m_AddOrUpdateHitStat(tick->object);
	if (tick->HasFlag(TickFlags::Button))
	{
		stat->delta = delta;
		stat->rating = tick->GetHitRatingFromDelta(hitWindow, delta);
		OnButtonHit.Call((Input::Button)index, stat->rating, tick->object, delta);

		if (stat->rating == ScoreHitRating::Perfect)
		{
			currentGauge += shortGaugeGain;
		}
		else
		{
			if (Math::Sign(delta) < 0)
				timedHits[0]++;
			else
				timedHits[1]++;

			currentGauge += shortGaugeGain / 3.0f;
		}
		m_AddScore((uint32)stat->rating);
	}
	else if (tick->HasFlag(TickFlags::Hold))
	{
		HoldObjectState* hold = (HoldObjectState*)tick->object;
		if (hold->time + hold->duration > m_playback->GetLastTime()) // Only set active hold object if object hasn't passed yet
			m_SetHoldObject(tick->object, index);

		stat->rating = ScoreHitRating::Perfect;
		stat->hold++;
		currentGauge += tickGaugeGain;
		m_AddScore(2);
	}
	else if (tick->HasFlag(TickFlags::Laser))
	{
		LaserObjectState* object = (LaserObjectState*)tick->object;
		LaserObjectState* rootObject = ((LaserObjectState*)tick->object)->GetRoot();
		if (tick->HasFlag(TickFlags::Slam))
		{
			OnLaserSlamHit.Call((LaserObjectState*)tick->object);
			// Set laser pointer position after hitting slam
			laserTargetPositions[object->index] = object->points[1];
			laserPositions[object->index] = object->points[1];
			m_autoLaserTime[object->index] = m_assistTime;
		}

		currentGauge += tickGaugeGain;
		m_AddScore(2);

		stat->rating = ScoreHitRating::Perfect;
		stat->hold++;
	}

	m_OnTickProcessed(tick, index);

	// Count hits per category (miss,perfect,etc.)
	categorizedHits[(uint32)stat->rating]++;
}
void Scoring::m_TickMiss(ScoreTick* tick, uint32 index, MapTime delta)
{
	HitStat* stat = m_AddOrUpdateHitStat(tick->object);
	stat->hasMissed = true;
	float shortMissDrain = 0.02f * m_drainMultiplier;
	if ((m_flags & GameFlags::Hard) != GameFlags::None)
	{
		// Thanks to Hibiki_ext in the discord for help with this
		float drainMultiplier = Math::Clamp(1.0f - ((0.3f - currentGauge) * 2.f), 0.5f, 1.0f);
		shortMissDrain = 0.09f * drainMultiplier * m_drainMultiplier;
	}
	if (tick->HasFlag(TickFlags::Button))
	{
		OnButtonMiss.Call((Input::Button)index, delta < 0 && abs(delta) > hitWindow.good, tick->object);
		stat->rating = ScoreHitRating::Miss;
		stat->delta = delta;
		currentGauge -= shortMissDrain;
	}
	else if (tick->HasFlag(TickFlags::Hold))
	{
		m_ReleaseHoldObject(index);
		currentGauge -= shortMissDrain / 4.f;
		stat->rating = ScoreHitRating::Miss;
	}
	else if (tick->HasFlag(TickFlags::Laser))
	{
		LaserObjectState* obj = (LaserObjectState*)tick->object;

		if (tick->HasFlag(TickFlags::Slam))
		{
			currentGauge -= shortMissDrain;
			m_autoLaserTime[obj->index] = -1;
		}
		else
			currentGauge -= shortMissDrain / 4.f;
		m_autoLaserTime[obj->index] = -1.f;
		stat->rating = ScoreHitRating::Miss;
	}

	// All misses reset combo
	currentGauge = std::max(0.0f, currentGauge);
	m_ResetCombo();
	m_OnTickProcessed(tick, index);

	// All ticks count towards the 'miss' counter
	categorizedHits[0]++;
}

void Scoring::m_CleanupTicks()
{
	for (uint32 i = 0; i < 8; i++)
	{
		for (ScoreTick* tick : m_ticks[i])
			delete tick;
		m_ticks[i].clear();
	}
}

void Scoring::m_AddScore(uint32 score)
{
	assert(score > 0 && score <= 2);
	if (score == 1 && comboState == 2)
		comboState = 1;
	currentHitScore += score;
	currentGauge = std::min(1.0f, currentGauge);
	currentComboCounter += 1;
	maxComboCounter = Math::Max(maxComboCounter, currentComboCounter);
	OnComboChanged.Call(currentComboCounter);
}
void Scoring::m_ResetCombo()
{
	comboState = 0;
	currentComboCounter = 0;
	OnComboChanged.Call(currentComboCounter);
}

void Scoring::m_SetHoldObject(ObjectState* obj, uint32 index)
{
	if (m_holdObjects[index] != obj)
	{
		assert(!m_heldObjects.Contains(obj));
		m_heldObjects.Add(obj);
		m_holdObjects[index] = obj;
		OnObjectHold.Call((Input::Button)index, obj);
	}
}
void Scoring::m_ReleaseHoldObject(ObjectState* obj)
{
	auto it = m_heldObjects.find(obj);
	if (it != m_heldObjects.end())
	{
		m_heldObjects.erase(it);

		// Unset hold objects
		for (uint32 i = 0; i < 8; i++)
		{
			if (m_holdObjects[i] == obj)
			{
				m_holdObjects[i] = nullptr;
				OnObjectReleased.Call((Input::Button)i, obj);
				return;
			}
		}
	}
}

void Scoring::m_ReleaseHoldObject(uint32 index)
{
	m_ReleaseHoldObject(m_holdObjects[index]);
}

bool Scoring::m_IsBeingHold(const ScoreTick* tick) const
{
	// NOTE: all these are just heuristics. If there's a better heuristic, change this.
	// See issue #355 for more detail.

	const HoldObjectState* obj = (HoldObjectState*) tick->object;
	const uint32 index = obj->index;
	assert(index < 6);
	
	// Button needs to be hold at this moment.
	// (Unless `tick` is the end of a long note; see below)
	if (m_input && m_input->GetButton((Input::Button) index))
	{
		// The object currently being held must be the given hold object.
		const ObjectState* heldObject = m_holdObjects[index];
		if (!heldObject || heldObject->type != ObjectType::Hold) return false;

		while (obj)
		{
			if (obj == (const HoldObjectState*)heldObject) return true;
			obj = obj->prev;
		}

		return false;
	}

	// If this is the end of an hold but the button is not being held,
	// it will still be counted as a hit hit as long as following two conditions are met.

	// a) The previous tick for this hold object was hit.
	if (tick->HasFlag(TickFlags::Start) || !tick->HasFlag(TickFlags::End)) return false;
	if (!m_prevHoldHit[index]) return false;

	// b) The last button release happened inside the 'near window' for the end of this hold object.
	if (obj->time + obj->duration - m_buttonReleaseTime[index] - static_cast<MapTime>(m_inputOffset) > hitWindow.hold) return false;

	return true;
}

bool Scoring::m_IsRoot(const LaserObjectState* laser) const
{
	if (!laser->prev) return true;
	return !m_range.Includes(laser->prev->time);
}

bool Scoring::m_IsRoot(const HoldObjectState* hold) const
{
	if (!hold->prev) return true;
	return !m_range.Includes(hold->prev->time);
}

void Scoring::m_UpdateLasers(float deltaTime)
{
	/// TODO: Change to only re-calculate on bpm change
	m_assistTime = m_assistLevel * 0.1f;

	MapTime mapTime = m_playback->GetLastTime();
	for (uint32 i = 0; i < 2; i++)
	{
		// Check for new laser segments in laser queue
		for (auto it = m_laserSegmentQueue.begin(); it != m_laserSegmentQueue.end();)
		{
			// Reset laser usage timer
			timeSinceLaserUsed[(*it)->index] = 0.0f;

			if ((*it)->time <= mapTime)
			{
				auto current = m_currentLaserSegments[(*it)->index];
				auto& currentTicks = m_ticks[6 + (*it)->index];
				if (!currentTicks.empty() && current != nullptr)
				{
					auto tick = currentTicks.front();
					if ((current->flags & LaserObjectState::flag_Instant) != 0)
					{
						if ((LaserObjectState*)tick->object == current) {
							// Don't continue to next segment before the slam has been decided as hit or not
							it++;
							continue;
						}
					}
				}
				// Replace the currently active segment
				m_currentLaserSegments[(*it)->index] = *it;
				if (m_currentLaserSegments[(*it)->index]->prev && m_currentLaserSegments[(*it)->index]->GetDirection() != m_currentLaserSegments[(*it)->index]->prev->GetDirection())
				{
					//Direction change
					//m_autoLaserTime[(*it)->index] = -1;
				}

				it = m_laserSegmentQueue.erase(it);
				continue;
			}
			it++;
		}

		LaserObjectState* currentSegment = m_currentLaserSegments[i];
		if (currentSegment)
		{
			lasersAreExtend[i] = (currentSegment->flags & LaserObjectState::flag_Extended) != 0;
			MapTime duration = currentSegment->duration;

			if ((currentSegment->time + currentSegment->duration) < mapTime)
			{
				auto currentTicks = m_ticks[6 + i];
				if ((currentSegment->flags & LaserObjectState::flag_Instant) == 0 
					|| currentTicks.empty() 
					|| (LaserObjectState*)currentTicks.front()->object != currentSegment) // Don't null slam that hasn't been judged yet
				{
					// Apply laser roll ignore when the laser has scrolled past
					if (!(currentSegment->flags & LaserObjectState::flag_Instant) && !currentSegment->next)
						OnLaserExit.Call(currentSegment);
					currentSegment = nullptr;
					m_currentLaserSegments[i] = nullptr;
					for (auto o : m_laserSegmentQueue)
					{
						if (o->index == i)
						{
							laserTargetPositions[i] = o->points[0];
							lasersAreExtend[i] = o->flags & LaserObjectState::flag_Extended;
							break;
						}
					}
				}
			}
			else
			{
				// Don't sample slams
				if (!(currentSegment->flags & LaserObjectState::flag_Instant))
				{
					// Update target position
					laserTargetPositions[i] = currentSegment->SamplePosition(mapTime);
				}
				// Apply slam roll instead
				else if (!(currentSegment->flags & LaserObjectState::flag_slamProcessed) && !currentSegment->next)
				{
					OnLaserSlam.Call(currentSegment);
				}
			}
		}

		m_laserInput[i] = autoplay ? 0.0f : m_input->GetInputLaserDir(i);

		if (currentSegment)
		{
			// Update laser gameplay
			float positionDelta = laserTargetPositions[i] - laserPositions[i];
			float moveDir = Math::Sign(positionDelta);
			float laserDir = currentSegment->GetDirection();
			float input = m_laserInput[i];
			float inputDir = Math::Sign(input);

			// Always snap laser to start sections if they are completely vertical
			if (laserDir == 0.0f && m_IsRoot(currentSegment))
			{
				laserPositions[i] = laserTargetPositions[i];
				m_autoLaserTime[i] = m_assistTime;
			}
			// Lock lasers on straight parts
			else if (laserDir == 0.0f && fabs(positionDelta) < laserDistanceLeniency)
			{
				laserPositions[i] = laserTargetPositions[i];
				m_autoLaserTime[i] = m_assistTime;
			}
			else if (inputDir != 0.0f)
			{
				if (laserDir < 0 && positionDelta < 0)
				{
					laserPositions[i] = Math::Max(laserPositions[i] + input, laserTargetPositions[i]);
				}
				else if (laserDir > 0 && positionDelta > 0)
				{
					laserPositions[i] = Math::Min(laserPositions[i] + input, laserTargetPositions[i]);
				}
				else if ((laserDir < 0 && positionDelta > 0) || (laserDir > 0 && positionDelta < 0))
				{
					laserPositions[i] = laserPositions[i] + input;
				}
				else if (laserDir == 0.0f)
				{
					if (positionDelta > 0)
						laserPositions[i] = Math::Min(laserPositions[i] + input, laserTargetPositions[i]);
					if (positionDelta < 0)
						laserPositions[i] = Math::Max(laserPositions[i] + input, laserTargetPositions[i]);
				}



				float punishMult = 1.0f;
				//if next segment is the opposite direction then allow for some extra wrong turning
				MapTime dirChangeTime = currentSegment->GetTimeToDirectionChange(mapTime, m_assistChangePeriod);
				if (dirChangeTime > -1)
				{
					punishMult = Math::Clamp((float)dirChangeTime / m_assistChangePeriod, 0.0f, 1.0f);
					punishMult = powf(punishMult, m_assistChangeExponent);
				}

				if (inputDir == moveDir && fabs(positionDelta) < laserDistanceLeniency)
				{
					m_autoLaserTime[i] = m_assistTime;
				}
				if (inputDir != 0 && inputDir != laserDir)
				{
					m_autoLaserTime[i] -= deltaTime * m_assistPunish * punishMult;
					//m_autoLaserTime[i] = Math::Min(m_autoLaserTime[i], m_assistTime * 0.2f);
				}
			}
			timeSinceLaserUsed[i] = 0.0f;
		}
		else
		{
			timeSinceLaserUsed[i] += deltaTime;
			//laserPositions[i] = laserTargetPositions[i];
		}
		if (autoplay || m_autoLaserTime[i] >= 0)
		{
			laserPositions[i] = laserTargetPositions[i];
		}
		// Clamp cursor between 0 and 1
		laserPositions[i] = Math::Clamp(laserPositions[i], 0.0f, 1.0f);
		m_autoLaserTime[i] -= deltaTime;
		if (fabsf(laserPositions[i] - laserTargetPositions[i]) < laserDistanceLeniency && currentSegment)
		{
			m_SetHoldObject(*currentSegment->GetRoot(), 6 + i);
		}
		else
		{
			m_ReleaseHoldObject(6 + i);
		}
	}

	// Interpolate laser output
	m_UpdateLaserOutput(deltaTime);
}

void Scoring::m_OnButtonPressed(Input::Button buttonCode)
{
	// Ignore buttons on autoplay
	if (autoplay)
		return;

	if (buttonCode < Input::Button::BT_S)
	{
		int32 guardDelta = m_playback->GetLastTime() - m_buttonGuardTime[(uint32)buttonCode];
		if (guardDelta < m_bounceGuard && guardDelta >= 0 && m_playback->GetLastTime() > 0.0)
		{
			//Logf("Button %d press bounce guard hit at %dms", Logger::Severity::Info, buttonCode, m_playback->GetLastTime());
			return;
		}

		//Logf("Button %d pressed at %dms", Logger::Severity::Info, buttonCode, m_playback->GetLastTime());
		m_buttonHitTime[(uint32)buttonCode] = m_playback->GetLastTime();
		m_buttonGuardTime[(uint32)buttonCode] = m_playback->GetLastTime();
		ObjectState* obj = m_ConsumeTick((uint32)buttonCode);
		if (!obj)
		{
			// Fire event for idle hits
			OnButtonHit.Call(buttonCode, ScoreHitRating::Idle, nullptr, 0);
		}
	}
	else if (buttonCode > Input::Button::BT_S)
	{
		if (buttonCode < Input::Button::LS_1Neg)
			m_ConsumeTick(6); // Laser L
		else
			m_ConsumeTick(7); // Laser R
	}
}
void Scoring::m_OnButtonReleased(Input::Button buttonCode)
{
	if (buttonCode < Input::Button::BT_S)
	{
		int32 guardDelta = m_playback->GetLastTime() - m_buttonGuardTime[(uint32)buttonCode];
		if (guardDelta < m_bounceGuard && guardDelta >= 0)
		{
			//Logf("Button %d release bounce guard hit at %dms", Logger::Severity::Info, buttonCode, m_playback->GetLastTime());
			return;
		}
		m_buttonReleaseTime[(uint32)buttonCode] = m_playback->GetLastTime();
		m_buttonGuardTime[(uint32)buttonCode] = m_playback->GetLastTime();
	}

	//Logf("Button %d released at %dms", Logger::Severity::Info, buttonCode, m_playback->GetLastTime());
	m_ReleaseHoldObject((uint32)buttonCode);
}

MapTotals Scoring::CalculateMapTotals() const
{
	MapTotals ret = { 0, 0, 0 };
	const Beatmap& map = m_playback->GetBeatmap();

	Set<LaserObjectState*> processedLasers;

	assert(m_playback);
	auto& objects = map.GetLinearObjects();
	for (auto& _obj : objects)
	{
		MultiObjectState* obj = *_obj;
		const TimingPoint* tp = m_playback->GetTimingPointAt(obj->time);
		if (obj->type == ObjectType::Single)
		{
			ret.maxScore += (uint32)ScoreHitRating::Perfect;
			ret.numSingles += 1;
		}
		else if (obj->type == ObjectType::Hold)
		{
			Vector<MapTime> holdTicks;
			m_CalculateHoldTicks((HoldObjectState*)obj, holdTicks);
			ret.maxScore += (uint32)ScoreHitRating::Perfect * (uint32)holdTicks.size();
			ret.numTicks += (uint32)holdTicks.size();
		}
		else if (obj->type == ObjectType::Laser)
		{
			LaserObjectState* laserRoot = obj->laser.GetRoot();

			// Don't evaluate ticks for every segment, only for entire chains of segments
			if (!processedLasers.Contains(laserRoot))
			{
				Vector<ScoreTick> laserTicks;
				m_CalculateLaserTicks((LaserObjectState*)obj, laserTicks);
				ret.maxScore += (uint32)ScoreHitRating::Perfect * (uint32)laserTicks.size();
				ret.numTicks += (uint32)laserTicks.size();
				processedLasers.Add(laserRoot);
			}
		}
	}

	return ret;
}

constexpr static uint32 CalculateScore(const uint32 currHitScore, const uint32 maxHitScore, const uint32 maxScore)
{
	if (maxHitScore == 0) return 0;
	else return (uint32) (((double) currHitScore / (double) maxHitScore) * (double) maxScore);
}

uint32 Scoring::CalculateCurrentScore() const
{
	return CalculateScore(currentHitScore);
}

uint32 Scoring::CalculateScore(uint32 hitScore) const
{
	return ::CalculateScore(hitScore, mapTotals.maxScore, MAX_SCORE);
}

uint32 Scoring::CalculateCurrentDisplayScore() const
{
	return CalculateCurrentDisplayScore(currentHitScore, currentMaxScore);
}

uint32 Scoring::CalculateCurrentDisplayScore(const ScoreReplay& replay) const
{
	return CalculateCurrentDisplayScore(replay.currentScore, replay.currentMaxScore);
}

uint32 Scoring::CalculateCurrentDisplayScore(uint32 currHit, uint32 currMaxHit) const
{
	switch (g_gameConfig.GetEnum<Enum_ScoreDisplayModes>(GameConfigKeys::ScoreDisplayMode))
	{
	case ScoreDisplayModes::Average:
		return CalculateCurrentAverageScore(currHit, currMaxHit);
		break;
	case ScoreDisplayModes::Subtractive:
		return CalculateCurrentMaxPossibleScore(currHit, currMaxHit);
		break;
	case ScoreDisplayModes::Additive:
	default:
		return CalculateScore(currHit);
	}
}

uint32 Scoring::CalculateCurrentMaxPossibleScore() const
{
	return CalculateCurrentMaxPossibleScore(currentHitScore, currentMaxScore);
}

uint32 Scoring::CalculateCurrentMaxPossibleScore(uint32 currHit, uint32 currMaxHit) const
{
	return CalculateScore(mapTotals.maxScore - (currMaxHit - currHit));
}

uint32 Scoring::CalculateCurrentAverageScore(uint32 currHit, uint32 currMaxHit) const
{
	return ::CalculateScore(currHit, currMaxHit, MAX_SCORE);
}

MapTime ScoreTick::GetHitWindow(const HitWindow& hitWindow) const
{
	// Hold ticks don't have a hit window, but the first ones do
	if (HasFlag(TickFlags::Hold) && !HasFlag(TickFlags::Start))
		return 0;
	// Laser ticks also don't have a hit window except for the first ticks and slam segments
	if (HasFlag(TickFlags::Laser))
	{
		if (!HasFlag(TickFlags::Start) && !HasFlag(TickFlags::Slam))
			return 0;
		return hitWindow.perfect;
	}
	return hitWindow.miss;
}
ScoreHitRating ScoreTick::GetHitRating(const HitWindow& hitWindow, MapTime currentTime) const
{
	const MapTime delta = abs(time - currentTime);
	return GetHitRatingFromDelta(hitWindow, delta);
}
ScoreHitRating ScoreTick::GetHitRatingFromDelta(const HitWindow& hitWindow, MapTime delta) const
{
	delta = abs(delta);
	if (HasFlag(TickFlags::Button))
	{
		// Button hit judgeing
		if (delta <= hitWindow.perfect)
			return ScoreHitRating::Perfect;
		if (delta <= hitWindow.good)
			return ScoreHitRating::Good;
		return ScoreHitRating::Miss;
	}
	return ScoreHitRating::Perfect;
}

bool ScoreTick::HasFlag(TickFlags flag) const
{
	return (flags & flag) != TickFlags::None;
}
void ScoreTick::SetFlag(TickFlags flag)
{
	flags = flags | flag;
}
TickFlags operator|(const TickFlags& a, const TickFlags& b)
{
	return (TickFlags)((uint8)a | (uint8)b);
}
TickFlags operator&(const TickFlags& a, const TickFlags& b)
{
	return (TickFlags)((uint8)a & (uint8)b);
}
