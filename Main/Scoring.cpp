#include "stdafx.h"
#include "Scoring.hpp"
#include <Beatmap/BeatmapPlayback.hpp>
#include <math.h>
#include "GameConfig.hpp"

const MapTime Scoring::missHitTime = 150;
const MapTime Scoring::goodHitTime = 100;
const MapTime Scoring::perfectHitTime = 35;
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

void Scoring::SetPlayback(BeatmapPlayback& playback)
{
	if(m_playback)
	{
		m_playback->OnObjectEntered.RemoveAll(this);
		m_playback->OnObjectLeaved.RemoveAll(this);
	}
	m_playback = &playback;
	m_playback->OnObjectEntered.Add(this, &Scoring::m_OnObjectEntered);
	m_playback->OnObjectLeaved.Add(this, &Scoring::m_OnObjectLeaved);
}

void Scoring::SetInput(Input* input)
{
	m_CleanupInput();
	if(input)
	{
		m_input = input;
		m_input->OnButtonPressed.Add(this, &Scoring::m_OnButtonPressed);
		m_input->OnButtonReleased.Add(this, &Scoring::m_OnButtonReleased);
	}
}
void Scoring::m_CleanupInput()
{
	if(m_input)
	{
		m_input->OnButtonPressed.RemoveAll(this);
		m_input->OnButtonReleased.RemoveAll(this);
		m_input = nullptr;
	}
}

void Scoring::Reset()
{
	// Reset score/combo counters
	currentMaxScore = 0;
	currentHitScore = 0;
	currentComboCounter = 0;
	maxComboCounter = 0;

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
	// Get laser assist level
	m_assistLevel = g_gameConfig.GetInt(GameConfigKeys::LaserAssistLevel);
	// Recalculate maximum score
	mapTotals = CalculateMapTotals();

	// Recalculate gauge gain
	// TODO: change variables to depend on the "total" variable in the chart.
	if (mapTotals.numTicks == 0 && mapTotals.numSingles != 0)
	{
		shortGaugeGain = 2.1f / (float)mapTotals.numSingles;
	}
	else if (mapTotals.numSingles == 0 && mapTotals.numTicks != 0)
	{
		tickGaugeGain = 2.1f / (float)mapTotals.numTicks;
	}
	else
	{
		shortGaugeGain = 42.0f / (5.0f * ((float)mapTotals.numTicks + (4.0f *(float)mapTotals.numSingles)));
		tickGaugeGain = shortGaugeGain / 4.0f;
	}
	currentGauge = 0.0f;

	m_heldObjects.clear();
	memset(m_holdObjects, 0, sizeof(m_holdObjects));
	memset(m_currentLaserSegments, 0, sizeof(m_currentLaserSegments));
	m_CleanupHitStats();
	m_CleanupTicks();

	OnScoreChanged.Call(0);
}

void Scoring::Tick(float deltaTime)
{
	m_UpdateLasers(deltaTime);
	m_UpdateTicks();
}

float Scoring::GetLaserRollOutput(uint32 index)
{
	assert(index >= 0 && index <= 1);
	if(m_currentLaserSegments[index])
	{
		if(index == 0)
			return -laserTargetPositions[index];
		if(index == 1)
			return (1.0f - laserTargetPositions[index]);
	}
	return 0.0f;
}

static const float laserOutputInterpolationDuration = 0.1f;
float Scoring::GetLaserOutput()
{
	float f = Math::Min(1.0f, m_timeSinceOutputSet / laserOutputInterpolationDuration);
	return m_laserOutputSource + (m_laserOutputTarget - m_laserOutputSource) * f;
}
float Scoring::m_GetLaserOutputRaw()
{
	float val = 0.0f;
	for(int32 i = 0; i < 2; i++)
	{
		if(IsLaserHeld(i) && m_currentLaserSegments[i])
		{
			// Skip single or end slams
			if(!m_currentLaserSegments[i]->next && (m_currentLaserSegments[i]->flags & LaserObjectState::flag_Instant) != 0)
				continue;

			float actual = laserTargetPositions[i];
			// Undo laser extension
			if((m_currentLaserSegments[i]->flags & LaserObjectState::flag_Extended) != 0)
			{
				actual += 0.5f;
				actual *= 0.5f;
				assert(actual >= 0.0f && actual <= 1.0f);
			}
			if(i == 1) // Second laser goes the other way
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
	if(v != m_laserOutputTarget)
	{
		m_laserOutputTarget = v;
		m_laserOutputSource = GetLaserOutput();
		m_timeSinceOutputSet = m_interpolateLaserOutput ? 0.0f : laserOutputInterpolationDuration;
	}
}

HitStat* Scoring::m_AddOrUpdateHitStat(ObjectState* object)
{
	if(object->type == ObjectType::Single)
	{
		HitStat* stat = new HitStat(object);
		hitStats.Add(stat);
		return stat;
	}
	else if(object->type == ObjectType::Hold)
	{
		HoldObjectState* hold = (HoldObjectState*)object;
		HitStat** foundStat = m_holdHitStats.Find(object);
		if(foundStat)
			return *foundStat;
		HitStat* stat = new HitStat(object);
		hitStats.Add(stat);
		m_holdHitStats.Add(object, stat);

		// Get tick count
		Vector<MapTime> ticks;
		m_CalculateHoldTicks(hold, ticks);
		stat->holdMax = (uint32)ticks.size();

		return stat;
	}
	else if(object->type == ObjectType::Laser)
	{
		LaserObjectState* rootLaser = ((LaserObjectState*)object)->GetRoot();
		HitStat** foundStat = m_holdHitStats.Find(*rootLaser);
		if(foundStat)
			return *foundStat;
		HitStat* stat = new HitStat(*rootLaser);
		hitStats.Add(stat);
		m_holdHitStats.Add(object, stat);

		// Get tick count
		Vector<ScoreTick> ticks;
		m_CalculateLaserTicks(rootLaser, ticks);
		stat->holdMax = (uint32)ticks.size();

		return stat;
	}

	// Shouldn't get here
	assert(false);
	return nullptr;
}

void Scoring::m_CleanupHitStats()
{
	for(HitStat* hit : hitStats)
		delete hit;
	hitStats.clear();
	m_holdHitStats.clear();
}

bool Scoring::IsObjectHeld(ObjectState* object)
{
	if(object->type == ObjectType::Laser)
	{
		// Select root node of laser
		object = *((LaserObjectState*)object)->GetRoot();
	}
	else if(object->type == ObjectType::Hold)
	{
		// Check all hold notes in a hold sequence to see if it is held
		bool held = false;
		HoldObjectState* root = ((HoldObjectState*)object)->GetRoot();
		while(root != nullptr)
		{
			if(m_heldObjects.Contains(*root))
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
	if(includeSlams)
		return IsObjectHeld(laserIndex + 6);

	if(m_holdObjects[laserIndex+6])
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

void Scoring::m_CalculateHoldTicks(HoldObjectState* hold, Vector<MapTime>& ticks) const
{
	const TimingPoint* tp = m_playback->GetTimingPointAt(hold->time);

	// Tick rate based on BPM
	const double tickNoteValue = 16 / (pow(2, Math::Max((int)(log2(tp->GetBPM())) - 7, 0)));
	const double tickInterval = tp->GetWholeNoteLength() / tickNoteValue;

	uint32 numTicks = (uint32)Math::Floor((double)hold->duration / tickInterval);
	if(numTicks < 1)
		numTicks = 1; // At least 1 tick at the start

	for(uint32 i = 0; i < numTicks; i++)
	{
		ticks.Add((MapTime)((double)hold->time + tickInterval * (double)i));
	}
}
void Scoring::m_CalculateLaserTicks(LaserObjectState* laserRoot, Vector<ScoreTick>& ticks) const
{
	assert(laserRoot->prev == nullptr);
	const TimingPoint* tp = m_playback->GetTimingPointAt(laserRoot->time);

	// Tick rate based on BPM
	const double tickNoteValue = 16 / (pow(2, Math::Max((int)(log2(tp->GetBPM())) - 7,0)));
	const double tickInterval = tp->GetWholeNoteLength() / tickNoteValue;

	LaserObjectState* sectionStart = laserRoot;
	MapTime sectionStartTime = laserRoot->time;
	MapTime combinedDuration = 0;
	LaserObjectState* lastSlam = nullptr;
	auto AddTicks = [&]()
	{
		uint32 numTicks = (uint32)Math::Floor((double)combinedDuration / tickInterval);
		for(uint32 i = 0; i < numTicks; i++)
		{
			if(lastSlam && i == 0) // No first tick if connected to slam
				continue;

			ScoreTick& t = ticks.Add(ScoreTick(*sectionStart));
			t.time = sectionStartTime + (MapTime)(tickInterval*(double)i);
			t.flags = TickFlags::Laser;
			
			// Link this tick to the correct segment
			if(sectionStart->next && (sectionStart->time + sectionStart->duration) <= t.time)
			{
				assert((sectionStart->next->flags & LaserObjectState::flag_Instant) == 0);
				t.object = *(sectionStart = sectionStart->next);
			}


			if(!lastSlam && i == 0)
				t.SetFlag(TickFlags::Start);
		}
		combinedDuration = 0;
	};

	for(auto it = laserRoot; it; it = it->next)
	{
		if((it->flags & LaserObjectState::flag_Instant) != 0)
		{
			AddTicks();
			ScoreTick& t = ticks.Add(ScoreTick(*it));
			t.time = it->time;
			t.flags = TickFlags::Laser | TickFlags::Slam;
			lastSlam = it;
			if(it->next)
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
	if(ticks.size() > 0)
		ticks.back().SetFlag(TickFlags::End);
}

void Scoring::m_OnObjectEntered(ObjectState* obj)
{
	// The following code registers which ticks exist depending on the object type / duration
	if(obj->type == ObjectType::Single)
	{
		ButtonObjectState* bt = (ButtonObjectState*)obj;
		ScoreTick* t = m_ticks[bt->index].Add(new ScoreTick(obj));
		t->time = bt->time;
		t->SetFlag(TickFlags::Button);

	}
	else if(obj->type == ObjectType::Hold)
	{
		const TimingPoint* tp = m_playback->GetTimingPointAt(obj->time);
		HoldObjectState* hold = (HoldObjectState*)obj;
		
		// Add all hold ticks
		Vector<MapTime> holdTicks;
		m_CalculateHoldTicks(hold, holdTicks);
		for(size_t i = 0; i < holdTicks.size(); i++)
		{
			ScoreTick* t = m_ticks[hold->index].Add(new ScoreTick(obj));
			t->SetFlag(TickFlags::Hold);
			if(i == 0 && !hold->prev)
				t->SetFlag(TickFlags::Start);
			if(i == holdTicks.size() - 1 && !hold->next)
				t->SetFlag(TickFlags::End);
			t->time = holdTicks[i];
		}
	}
	else if(obj->type == ObjectType::Laser)
	{
		LaserObjectState* laser = (LaserObjectState*)obj;
		if(!laser->prev) // Only register root laser objects
		{
			// All laser ticks, including slam segments
			Vector<ScoreTick> laserTicks;
			m_CalculateLaserTicks(laser, laserTicks);
			for(size_t i = 0; i < laserTicks.size(); i++)
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
	if(obj->type == ObjectType::Laser)
	{
		LaserObjectState* laser = (LaserObjectState*)obj;
		if(laser->next != nullptr)
			return; // Only terminate holds on last of laser section
		obj = *laser->GetRoot();
	}
	m_ReleaseHoldObject(obj);
}

void Scoring::m_UpdateTicks()
{
	MapTime currentTime = m_playback->GetLastTime();

	// This loop checks for ticks that are missed
	for(uint32 buttonCode = 0; buttonCode < 8; buttonCode++)
	{
		Input::Button button = (Input::Button)buttonCode;

		// List of ticks for the current button code
		auto& ticks = m_ticks[buttonCode];
		for(uint32 i = 0; i < ticks.size(); i++)
		{
			ScoreTick* tick = ticks[i];
			MapTime delta = currentTime - ticks[i]->time;
			bool shouldMiss = delta > tick->GetHitWindow();
			bool processed = false;
			if(delta >= 0)
			{
				if(tick->HasFlag(TickFlags::Button) && (autoplay || autoplayButtons))
				{
					m_TickHit(tick, buttonCode, 0);
					processed = true;
				}

				if(tick->HasFlag(TickFlags::Hold))
				{
					// Ignore the first hold note ticks
					//	except for autoplay, which just hits it.
					if(!tick->HasFlag(TickFlags::Start) || (autoplay || autoplayButtons))
					{
						// Check buttons here for holds
						if((m_input && m_input->GetButton(button)) || autoplay || autoplayButtons)
						{							
							// Make sure the button wasnt being held long before the hold became active.
							if (tick->object->time - goodHitTime < m_buttonHitTime[buttonCode] || autoplay || autoplayButtons)
							{
								m_TickHit(tick, buttonCode);
								processed = true;
							}
						}
					}
				}
				else if(tick->HasFlag(TickFlags::Laser))
				{
					LaserObjectState* laserObject = (LaserObjectState*)tick->object;
					if(tick->HasFlag(TickFlags::Slam))
					{
						// Check if slam hit
						float dirSign = Math::Sign(laserObject->GetDirection());
						float inputSign = Math::Sign(m_input->GetInputLaserDir(buttonCode - 6));
						if(autoplay)
							inputSign = dirSign;
						if(dirSign == inputSign && delta > 0)
						{
							m_TickHit(tick, buttonCode);
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
							m_autoLaserTick[laserObject->index] = m_assistLevel;
						}

						// Check laser input
						float laserDelta = fabs(laserPositions[laserObject->index] - laserTargetPositions[laserObject->index]);\

						if(laserDelta < laserDistanceLeniency)
						{
							m_TickHit(tick, buttonCode);
							if(m_autoLaserTick[laserObject->index] > 0)
								m_autoLaserTick[laserObject->index] -= 1;
							processed = true;
						}
					}
				}

				if(shouldMiss && !processed)
				{
					m_TickMiss(tick, buttonCode, delta);
					processed = true;
				}

				if(processed)
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
}
ObjectState* Scoring::m_ConsumeTick(uint32 buttonCode)
{
	MapTime currentTime = m_playback->GetLastTime();

	assert(buttonCode < 8);
	auto& ticks = m_ticks[buttonCode];
	for(uint32 i = 0; i < ticks.size(); i++)
	{
		ScoreTick* tick = ticks[i];
		MapTime delta = currentTime - ticks[i]->time + m_inputOffset;
		if(abs(delta) < tick->GetHitWindow())
		{
			ObjectState* hitObject = tick->object;

			if(tick->HasFlag(TickFlags::Laser))
			{
				// Ignore laser ticks
				continue;
			}
			if(delta < Scoring::goodHitTime)
				m_TickHit(tick, buttonCode, delta);
			else
				m_TickMiss(tick, buttonCode, delta);
			delete tick;
			ticks.Remove(tick, false);

			return hitObject;
		}
	}
	return nullptr;
}

void Scoring::m_OnTickProcessed(ScoreTick* tick, uint32 index)
{
	if(OnScoreChanged.IsHandled())
	{
		OnScoreChanged.Call(CalculateCurrentScore());
	}
}
void Scoring::m_TickHit(ScoreTick* tick, uint32 index, MapTime delta /*= 0*/)
{
	HitStat* stat = m_AddOrUpdateHitStat(tick->object);
	if(tick->HasFlag(TickFlags::Button))
	{
		stat->delta = delta;
		stat->rating = tick->GetHitRatingFromDelta(delta);
		OnButtonHit.Call((Input::Button)index, stat->rating, tick->object);


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
	else if(tick->HasFlag(TickFlags::Hold))
	{
		HoldObjectState* hold = (HoldObjectState*)tick->object;
		if(hold->time + hold->duration > m_playback->GetLastTime()) // Only set active hold object if object hasn't passed yet
		m_SetHoldObject(tick->object, index);

		stat->rating = ScoreHitRating::Perfect;
		stat->hold++;
		currentGauge += tickGaugeGain;
		m_AddScore(2);
	}
	else if(tick->HasFlag(TickFlags::Laser))
	{
		LaserObjectState* object = (LaserObjectState*)tick->object;
		LaserObjectState* rootObject = ((LaserObjectState*)tick->object)->GetRoot();
		if(tick->HasFlag(TickFlags::Slam))
		{
			OnLaserSlamHit.Call((LaserObjectState*)tick->object);
			// Set laser pointer position after hitting slam
			laserTargetPositions[object->index] = object->points[1];
			laserPositions[object->index] = object->points[1];

			

		}
		if(m_holdObjects[object->index + 6] != *rootObject)
		{
			// Only set active hold object if object hasn't passed yet
			LaserObjectState* endObject = ((LaserObjectState*)tick->object)->GetTail();
			if(endObject->time + endObject->duration > m_playback->GetLastTime())
				m_SetHoldObject(*rootObject, index);
		}
		m_SetHoldObject(*rootObject, index);
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
	if(tick->HasFlag(TickFlags::Button))
	{
		OnButtonMiss.Call((Input::Button)index); 
		stat->rating = ScoreHitRating::Miss;
		stat->delta = delta;
		currentGauge -= 0.02f;
	}
	else if(tick->HasFlag(TickFlags::Hold))
	{
		m_ReleaseHoldObject(index);
		currentGauge -= 0.005f;
		stat->rating = ScoreHitRating::Miss;
	}
	else if(tick->HasFlag(TickFlags::Laser))
	{
		m_ReleaseHoldObject(index);
		currentGauge -= 0.005f;
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
	for(uint32 i = 0; i < 8; i++)
	{
		for(ScoreTick* tick : m_ticks[i])
			delete tick;
		m_ticks[i].clear();
	}
}

void Scoring::m_AddScore(uint32 score)
{
	assert(score > 0 && score <= 2);
	currentHitScore += score;
	currentGauge = std::min(1.0f, currentGauge);
	currentComboCounter += 1;
	maxComboCounter = Math::Max(maxComboCounter, currentComboCounter);
	OnComboChanged.Call(currentComboCounter);
}
void Scoring::m_ResetCombo()
{
	currentComboCounter = 0;
	OnComboChanged.Call(currentComboCounter);
}

void Scoring::m_SetHoldObject(ObjectState* obj, uint32 index)
{
	if(m_holdObjects[index] != obj)
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
	if(it != m_heldObjects.end())
	{
		m_heldObjects.erase(it);

		// Unset hold objects
		for(uint32 i = 0; i < 8; i++)
		{
			if(m_holdObjects[i] == obj)
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

void Scoring::m_UpdateLasers(float deltaTime)
{
	MapTime mapTime = m_playback->GetLastTime();
	for(uint32 i = 0; i < 2; i++)
	{
		// Check for new laser segments in laser queue
		for(auto it = m_laserSegmentQueue.begin(); it != m_laserSegmentQueue.end();)
		{
			// Reset laser usage timer
			timeSinceLaserUsed[(*it)->index] = 0.0f;

			if((*it)->time <= mapTime)
			{
				// Replace the currently active segment
				m_currentLaserSegments[(*it)->index] = *it;
				it = m_laserSegmentQueue.erase(it);
				continue;
			}
			it++;
		}
		
		LaserObjectState* currentSegment = m_currentLaserSegments[i];
		if(currentSegment)
		{
			lasersAreExtend[i] = (currentSegment->flags & LaserObjectState::flag_Extended) != 0;
			if((currentSegment->time + currentSegment->duration) < mapTime)
			{
				currentSegment = nullptr;
				m_currentLaserSegments[i] = nullptr;
			}
			else
			{
				// Update target position
				laserTargetPositions[i] = currentSegment->SamplePosition(mapTime);
			}
		}

		m_laserInput[i] = autoplay ? 0.0f : m_input->GetInputLaserDir(i);

		bool notAffectingGameplay = true;
		if(currentSegment)
		{
			// Update laser gameplay
			float positionDelta = laserTargetPositions[i] - laserPositions[i];
			float moveDir = Math::Sign(positionDelta);
			float laserDir = currentSegment->GetDirection();
			float input = m_laserInput[i];
			float inputDir = Math::Sign(input);

			// Always snap laser to start sections if they are completely vertical
			// Check Yggdrasil_ch.ksh for a part that starts of with vertical lasers and then curve towards the other side (46500 ms in)
			if (laserDir == 0.0f && currentSegment->prev == nullptr)
				laserPositions[i] = laserTargetPositions[i];
			// Lock lasers on straight parts
			else if (laserDir == 0.0f && fabs(positionDelta) < laserDistanceLeniency)
			{
				laserPositions[i] = laserTargetPositions[i];
				if (m_autoLaserTick[i] < m_assistLevel)
					m_autoLaserTick[i] += 1;
			}
			else if(inputDir != 0.0f)
			{
				if(laserDir < 0 && positionDelta < 0)
				{
					laserPositions[i] = Math::Max(laserPositions[i] + input, laserTargetPositions[i]);
				}
				else if (laserDir > 0 && positionDelta > 0)
				{
					laserPositions[i] = Math::Min(laserPositions[i] + input, laserTargetPositions[i]);
				}
				else if (laserDir < 0 && positionDelta > 0 || laserDir > 0 && positionDelta < 0)
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
				notAffectingGameplay = false;
				if (inputDir == moveDir && fabs(positionDelta) < laserDistanceLeniency && m_autoLaserTick[i] < m_assistLevel)
				{
					m_autoLaserTick[i] += 1;
				}
			}
			timeSinceLaserUsed[i] = 0.0f;
		}
		else
		{
			timeSinceLaserUsed[i] += deltaTime;
		}
		if (autoplay || m_autoLaserTick[i] > 0)
		{
			laserPositions[i] = laserTargetPositions[i];
		}
		// Clamp cursor between 0 and 1
		laserPositions[i] = Math::Clamp(laserPositions[i], 0.0f, 1.0f);
	}

	// Interpolate laser output
	m_UpdateLaserOutput(deltaTime);
}

void Scoring::m_OnButtonPressed(Input::Button buttonCode)
{
	// Ignore buttons on autoplay
	if(autoplay)
		return;

	if(buttonCode < Input::Button::BT_S)
	{
		m_buttonHitTime[(uint32)buttonCode] = m_playback->GetLastTime();
		ObjectState* obj = m_ConsumeTick((uint32)buttonCode);
		if(!obj)
		{
			// Fire event for idle hits
			OnButtonHit.Call(buttonCode, ScoreHitRating::Idle, nullptr);
		}
	}
	else if (buttonCode > Input::Button::BT_S)
	{
		ObjectState* obj = nullptr;
		if(buttonCode < Input::Button::LS_1Neg)
			obj = m_ConsumeTick(6); // Laser L
		else
			obj = m_ConsumeTick(7); // Laser R
	}
}
void Scoring::m_OnButtonReleased(Input::Button buttonCode)
{
}

MapTotals Scoring::CalculateMapTotals() const
{
	MapTotals ret = { 0 };
	const Beatmap& map = m_playback->GetBeatmap();

	Set<LaserObjectState*> processedLasers;

	assert(m_playback);
	auto& objects = map.GetLinearObjects();
	for(auto& _obj : objects)
	{
		MultiObjectState* obj = *_obj;
		const TimingPoint* tp = m_playback->GetTimingPointAt(obj->time);
		if(obj->type == ObjectType::Single)
		{
			ret.maxScore += (uint32)ScoreHitRating::Perfect;
			ret.numSingles += 1;
		}
		else if(obj->type == ObjectType::Hold)
		{
			Vector<MapTime> holdTicks;
			m_CalculateHoldTicks((HoldObjectState*)obj, holdTicks);
			ret.maxScore += (uint32)ScoreHitRating::Perfect * (uint32)holdTicks.size();
			ret.numTicks += (uint32)holdTicks.size();
		}
		else if(obj->type == ObjectType::Laser)
		{
			LaserObjectState* laserRoot = obj->laser.GetRoot();

			// Don't evaluate ticks for every segment, only for entire chains of segments
			if(!processedLasers.Contains(laserRoot))
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

uint32 Scoring::CalculateCurrentScore() const
{
	return (uint32)(((double)currentHitScore / (double)mapTotals.maxScore) * 10000000.0);
}

uint32 Scoring::CalculateCurrentGrade() const
{
	uint32 value = (uint32)((double)CalculateCurrentScore() * (double)0.9 + currentGauge * 1000000.0);
	if(value > 9800000) // AAA
		return 0;
	if(value > 9400000) // AA
		return 1;
	if(value > 8900000) // A
		return 2;
	if(value > 8000000) // B
		return 3;
	if(value > 7000000) // C
		return 4;
	return 5; // D
}

MapTime ScoreTick::GetHitWindow() const
{
	// Hold ticks don't have a hit window, but the first ones do
	if(HasFlag(TickFlags::Hold) && !HasFlag(TickFlags::Start))
		return 0;
	// Laser ticks also don't have a hit window except for the first ticks and slam segments
	if(HasFlag(TickFlags::Laser))
	{
		if(!HasFlag(TickFlags::Start) && !HasFlag(TickFlags::Slam))
			return 0;
	}
	return Scoring::missHitTime;
}
ScoreHitRating ScoreTick::GetHitRating(MapTime currentTime) const
{
	MapTime delta = abs(time - currentTime);
	return GetHitRatingFromDelta(delta);
}
ScoreHitRating ScoreTick::GetHitRatingFromDelta(MapTime delta) const
{
	delta = abs(delta);
	if(HasFlag(TickFlags::Button))
	{
		// Button hit judgeing
		if(delta > Scoring::goodHitTime)
			return ScoreHitRating::Miss;
		if(delta < Scoring::perfectHitTime)
			return ScoreHitRating::Perfect;
		return ScoreHitRating::Good;
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
