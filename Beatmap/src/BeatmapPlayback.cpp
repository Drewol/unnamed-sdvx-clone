#include "stdafx.h"
#include "BeatmapPlayback.hpp"
#include "Shared/Profiling.hpp"

BeatmapPlayback::BeatmapPlayback(const Beatmap& beatmap) : m_beatmap(&beatmap)
{
}

bool BeatmapPlayback::Reset(MapTime initTime, MapTime start)
{
	m_effectObjects.clear();
	if (!m_beatmap || !m_beatmap->HasObjectState())
	{
		return false;
	}

	Logf("Resetting BeatmapPlayback, InitTime = %d, Start = %d", Logger::Severity::Debug, initTime, start);
	m_playbackTime = initTime;

	if (start <= 0) start = std::numeric_limits<decltype(start)>::min();
	m_playRange = { start, start };

	m_currObject = m_beatmap->GetFirstObjectState();
	m_currLaserObject = m_beatmap->GetFirstObjectState();
	m_currAlertObject = m_beatmap->GetFirstObjectState();

	m_currentTiming = m_beatmap->GetFirstTimingPoint();
	m_currentLaneTogglePoint = m_beatmap->GetFirstLaneTogglePoint();

	m_currentTrackRollBehaviour = TrackRollBehaviour::Normal;
	m_lastTrackRollBehaviourChange = 0;

	//hittableLaserEnter = (*m_currentTiming)->beatDuration * 4.0;
	//alertLaserThreshold = (*m_currentTiming)->beatDuration * 6.0;

	m_objectsByTime.clear();
	m_objectsByLeaveTime.clear();

	m_barTime = 0;
	m_beatTime = 0;
	m_initialEffectStateSent = false;

	return true;
}

void BeatmapPlayback::Update(MapTime newTime)
{
	MapTime delta = newTime - m_playbackTime;

	if (m_isCalibration) {
		// Count bars
		int32 beatID = 0;
		uint32 nBeats = CountBeats(m_playbackTime - delta, delta, beatID);
		const TimingPoint& tp = GetCurrentTimingPoint();
		double effectiveTime = ((double)newTime - tp.time); // Time with offset applied
		m_barTime = (float)fmod(effectiveTime / (tp.beatDuration * tp.numerator), 1.0);
		m_beatTime = (float)fmod(effectiveTime / tp.beatDuration, 1.0);

		// Set new time
		m_playbackTime = newTime;
		return;
	}

	if (newTime < m_playbackTime)
	{
		// Don't allow backtracking
		//Logf("New time was before last time %ull -> %ull", Logger::Warning, m_playbackTime, newTime);
		return;
	}

	// Fire initial effect changes (only once)
	if (!m_initialEffectStateSent)
	{
		const BeatmapSettings& settings = m_beatmap->GetMapSettings();
		OnEventChanged.Call(EventKey::LaserEffectMix, settings.laserEffectMix);
		OnEventChanged.Call(EventKey::LaserEffectType, settings.laserEffectType);
		OnEventChanged.Call(EventKey::SlamVolume, settings.slamVolume);
		m_initialEffectStateSent = true;
	}

	// Count bars
	int32 beatID = 0;
	uint32 nBeats = CountBeats(m_playbackTime - delta, delta, beatID);
	const TimingPoint& tp = GetCurrentTimingPoint();
	double effectiveTime = ((double)newTime - tp.time); // Time with offset applied
	m_barTime = (float)fmod(effectiveTime / (tp.beatDuration * tp.numerator), 1.0);
	m_beatTime = (float)fmod(effectiveTime / tp.beatDuration, 1.0);

	// Set new time
	m_playbackTime = newTime;

	// Advance timing
	Beatmap::TimingPointsIterator timingEnd = m_SelectTimingPoint(m_playbackTime);
	if (timingEnd != m_currentTiming)
	{
		m_currentTiming = timingEnd;
		/// TODO: Investigate why this causes score to be too high
		//hittableLaserEnter = (*m_currentTiming)->beatDuration * 4.0;
		//alertLaserThreshold = (*m_currentTiming)->beatDuration * 6.0;
		OnTimingPointChanged.Call(m_currentTiming);
	}

	// Advance lane toggle
	Beatmap::LaneTogglePointsIterator laneToggleEnd = m_SelectLaneTogglePoint(m_playbackTime);
	if (laneToggleEnd != m_currentLaneTogglePoint)
	{
		m_currentLaneTogglePoint = laneToggleEnd;
		OnLaneToggleChanged.Call(m_currentLaneTogglePoint);
	}

	// Advance objects
	Beatmap::ObjectsIterator objEnd = m_SelectHitObject(m_playbackTime + hittableObjectEnter);
	if (objEnd != m_currObject)
	{
		for (auto it = m_currObject; it < objEnd; it++)
		{
			MultiObjectState* obj = *(*it).get();
			if (obj->type == ObjectType::Laser) continue;

			if (!m_playRange.Includes(obj->time)) continue;

			if (obj->type == ObjectType::Hold && !m_playRange.Includes(obj->time + obj->hold.duration, true)) continue;

			MapTime duration = 0;
			if (obj->type == ObjectType::Hold)
			{
				duration = obj->hold.duration;
			}
			else if (obj->type == ObjectType::Event)
			{
				// Tiny offset to make sure events are triggered before they are needed
				duration = -2;
			}

			m_objectsByTime.Add(obj->time, (*it).get());
			m_objectsByLeaveTime.Add(obj->time + duration + hittableObjectLeave, (*it).get());

			OnObjectEntered.Call((*it).get());
		}
		m_currObject = objEnd;
	}

	// Advance lasers
	objEnd = m_SelectHitObject(m_playbackTime + hittableLaserEnter);
	if (objEnd != m_currLaserObject)
	{
		for (auto it = m_currLaserObject; it < objEnd; it++)
		{
			MultiObjectState* obj = *(*it).get();
			if (obj->type != ObjectType::Laser) continue;

			if (!m_playRange.Includes(obj->time)) continue;
			if (!m_playRange.Includes(obj->time + obj->laser.duration, true)) continue;

			m_objectsByTime.Add(obj->time, (*it).get());
			m_objectsByLeaveTime.Add(obj->time + obj->laser.duration + hittableObjectLeave, (*it).get());

			OnObjectEntered.Call((*it).get());
		}
		m_currLaserObject = objEnd;
	}

	// Check for lasers within the alert time
	objEnd = m_SelectHitObject(m_playbackTime + alertLaserThreshold);
	if (objEnd != m_currAlertObject)
	{
		for (auto it = m_currAlertObject; it < objEnd; it++)
		{
			MultiObjectState* obj = **it;
			if (!m_playRange.Includes(obj->time)) continue;

			if (obj->type == ObjectType::Laser)
			{
				LaserObjectState* laser = (LaserObjectState*)obj;
				if (!laser->prev)
					OnLaserAlertEntered.Call(laser);
			}
		}
		m_currAlertObject = objEnd;
	}

	// Check passed objects
	for (auto it = m_objectsByLeaveTime.begin(); it != m_objectsByLeaveTime.end() && it->first < m_playbackTime; it = m_objectsByLeaveTime.erase(it))
	{
		ObjectState* objState = it->second;
		MultiObjectState* obj = *(objState);

		// O(n^2) when there are n objects with same time,
		// but n is usually small so let's ignore that issue for now...
		{
			auto pair = m_objectsByTime.equal_range(obj->time);

			for (auto it2 = pair.first; it2 != pair.second; ++it2)
			{
				if (it2->second == objState)
				{
					m_objectsByTime.erase(it2);
					break;
				}
			}
		}

		switch (obj->type)
		{
		case ObjectType::Hold:
			OnObjectLeaved.Call(objState);

			if (m_effectObjects.Contains(objState))
			{
				OnFXEnd.Call((HoldObjectState*)objState);
				m_effectObjects.erase(objState);
			}
			break;
		case ObjectType::Laser:
		case ObjectType::Single:
			OnObjectLeaved.Call(objState);
			break;
		case ObjectType::Event:
		{
			EventObjectState* evt = (EventObjectState*) obj;

			if (evt->key == EventKey::TrackRollBehaviour)
			{
				if (m_currentTrackRollBehaviour != evt->data.rollVal)
				{
					m_currentTrackRollBehaviour = evt->data.rollVal;
					m_lastTrackRollBehaviourChange = obj->time;
				}
			}

			// Trigger event
			OnEventChanged.Call(evt->key, evt->data);
			m_eventMapping[evt->key] = evt->data;
		}
		default:
			break;
		}
	}

	const MapTime audioPlaybackTime = m_playbackTime + audioOffset;

	// Process FX effects
	for (auto& it : m_objectsByTime)
	{
		ObjectState* objState = it.second;
		MultiObjectState* obj = *(objState);

		if (obj->type != ObjectType::Hold || obj->hold.effectType == EffectType::None)
		{
			continue;
		}

		const MapTime endTime = obj->time + obj->hold.duration;

		// Send `OnFXBegin` a little bit earlier (the other side checks the exact timing again)
		if (obj->time - 100 <= audioPlaybackTime && audioPlaybackTime <= endTime - 100)
		{
			if (!m_effectObjects.Contains(objState))
			{
				OnFXBegin.Call((HoldObjectState*)objState);
				m_effectObjects.Add(objState);
			}
		}

		if (endTime < audioPlaybackTime)
		{
			if (m_effectObjects.Contains(objState))
			{
				OnFXEnd.Call((HoldObjectState*) objState);
				m_effectObjects.erase(objState);
			}
		}
	}
}

void BeatmapPlayback::MakeCalibrationPlayback()
{
	m_isCalibration = true;

	for (size_t i = 0; i < 50; i++)
	{
		ButtonObjectState* newObject = new ButtonObjectState();
		newObject->index = i % 4;
		newObject->time = static_cast<MapTime>(i * 500);

		m_calibrationObjects.Add(Ref<ObjectState>((ObjectState*)newObject));
	}

	m_calibrationTiming = {};
	m_calibrationTiming.beatDuration = 500;
	m_calibrationTiming.time = 0;
	m_calibrationTiming.denominator = 4;
	m_calibrationTiming.numerator = 4;
}

void BeatmapPlayback::GetObjectsInViewRange(float numBeats, Vector<ObjectState*>& objects)
{
	// TODO: properly implement backwards scroll speed support...
	numBeats *= 3;

	const static MapTime earlyVisibility = 200;

	if (m_isCalibration) {
		for (auto& o : m_calibrationObjects)
		{
			if (o->time < (MapTime)(m_playbackTime - earlyVisibility))
				continue;

			if (o->time > m_playbackTime + static_cast<MapTime>(numBeats * m_calibrationTiming.beatDuration))
				break;

			objects.Add(o.get());
		}

		return;
	}

	// Add objects
	for (auto& it : m_objectsByTime)
	{
		objects.Add(it.second);
	}

	Beatmap::TimingPointsIterator tp = m_SelectTimingPoint(m_playbackTime);
	Beatmap::TimingPointsIterator tp_next = std::next(tp);

	// # of beats from m_playbackTime to curr TP
	MapTime currRefTime = m_playbackTime;
	float currBeats = 0.0f;

	for (Beatmap::ObjectsIterator obj = m_currObject; !IsEndObject(obj) && m_playRange.Includes((*obj)->time); ++obj)
	{
		const MapTime objTime = (*obj)->time;

		if (!IsEndTiming(tp_next) && tp_next->time <= objTime)
		{
			currBeats += m_beatmap->GetBeatCountWithScrollSpeedApplied(currRefTime, tp_next->time, tp);

			tp = tp_next;
			tp_next = std::next(tp_next);
			currRefTime = tp->time;
		}

		const float objBeats = currBeats + m_beatmap->GetBeatCountWithScrollSpeedApplied(currRefTime, objTime, tp);
		if (objBeats >= numBeats)
		{
			break;
		}

		// Lasers might be already added before
		if ((*obj)->type == ObjectType::Laser && obj < m_currLaserObject)
		{
			continue;
		}

		objects.Add(obj->get());
	}
}

void BeatmapPlayback::GetBarPositionsInViewRange(float numBeats, Vector<float>& barPositions) const
{
	// TODO
	/*
	// Update ticks separating bars to draw
	double tickTime = (double)currentTime;
	MapTime rangeEnd = currentTime + playback.OLD_ViewDistanceToDuration(m_viewRange);
	const TimingPoint* tp = playback.GetTimingPointAt((MapTime)tickTime);
	double stepTime = tp->GetBarDuration(); // Every xth note based on signature

	// Overflow on first tick
	double firstOverflow = fmod((double)tickTime - tp->time, stepTime);
	if(fabs(firstOverflow) > 1)
		tickTime -= firstOverflow;

	m_barTicks.clear();

	// Add first tick
	m_barTicks.Add(playback.TimeToViewDistance((MapTime)tickTime));

	while(tickTime < rangeEnd)
	{
		double next = tickTime + stepTime;

		const TimingPoint* tpNext = playback.GetTimingPointAt((MapTime)tickTime);
		if(tpNext != tp)
		{
			tp = tpNext;
			tickTime = tp->time;
			stepTime = tp->GetBarDuration(); // Every xth note based on signature
		}
		else
		{
			tickTime = next;
		}

		// Add tick
		m_barTicks.Add(playback.TimeToViewDistance((MapTime)tickTime));
	}
	*/
}

const TimingPoint& BeatmapPlayback::GetCurrentTimingPoint() const
{
	if (m_isCalibration)
	{
		return m_calibrationTiming;
	}

	if (IsEndTiming(m_currentTiming))
	{
		return *(m_beatmap->GetFirstTimingPoint());
	}

	return *m_currentTiming;
}

const TimingPoint* BeatmapPlayback::GetTimingPointAt(MapTime time) const
{
	if (m_isCalibration)
	{
		return &m_calibrationTiming;
	}

	Beatmap::TimingPointsIterator it = const_cast<BeatmapPlayback*>(this)->m_SelectTimingPoint(time);
	if (IsEndTiming(it))
	{
		return nullptr;
	}
	else
	{
		return &(*it);
	}
}

uint32 BeatmapPlayback::CountBeats(MapTime start, MapTime range, int32& startIndex, uint32 multiplier /*= 1*/) const
{
	const TimingPoint& tp = GetCurrentTimingPoint();
	int64 delta = (int64)start - (int64)tp.time;
	double beatDuration = tp.GetWholeNoteLength() / tp.denominator;
	int64 beatStart = (int64)floor((double)delta / (beatDuration / multiplier));
	int64 beatEnd = (int64)floor((double)(delta + range) / (beatDuration / multiplier));
	startIndex = ((int32)beatStart + 1) % tp.numerator;
	return (uint32)Math::Max<int64>(beatEnd - beatStart, 0);
}

float BeatmapPlayback::GetViewDistance(MapTime startTime, MapTime endTime) const
{
	if (startTime == endTime)
	{
		return 0.0f;
	}

	if (cMod || m_isCalibration)
	{
		return GetViewDistanceIgnoringScrollSpeed(startTime, endTime);
	}

	return m_beatmap->GetBeatCountWithScrollSpeedApplied(startTime, endTime, m_currentTiming);
}

float BeatmapPlayback::GetViewDistanceIgnoringScrollSpeed(MapTime startTime, MapTime endTime) const
{
	if (startTime == endTime)
	{
		return 0.0f;
	}

	if (cMod)
	{
		return static_cast<float>(endTime - startTime) / 480000.0f;
	}

	if (m_isCalibration)
	{
		return static_cast<float>((endTime - startTime) / m_calibrationTiming.beatDuration);
	}

	return m_beatmap->GetBeatCount(startTime, endTime, m_currentTiming);
}

float BeatmapPlayback::GetZoom(uint8 index) const
{
	EffectTimeline::GraphType graphType;

	switch (index)
	{
	case 0:
		graphType = EffectTimeline::GraphType::ZOOM_BOTTOM;
		break;
	case 1:
		graphType = EffectTimeline::GraphType::ZOOM_TOP;
		break;
	case 2:
		graphType = EffectTimeline::GraphType::SHIFT_X;
		break;
	case 3:
		graphType = EffectTimeline::GraphType::ROTATION_Z;
		break;
	case 4:
		return m_beatmap->GetCenterSplitValueAt(m_playbackTime);
		break;
	default:
		assert(false);
		break;
	}

	// TODO: pass aux value
	return m_beatmap->GetGraphValueAt(graphType, m_playbackTime);
}

float BeatmapPlayback::GetScrollSpeed() const
{
	// TODO: pass aux value
	return m_beatmap->GetScrollSpeedAt(m_playbackTime);
}

bool BeatmapPlayback::CheckIfManualTiltInstant()
{
	if (m_currentTrackRollBehaviour != TrackRollBehaviour::Manual)
	{
		return false;
	}

	// TODO: pass aux value
	return m_beatmap->CheckIfManualTiltInstant(m_lastTrackRollBehaviourChange, m_playbackTime);
}

Beatmap::TimingPointsIterator BeatmapPlayback::m_SelectTimingPoint(MapTime time, bool allowReset)
{
	return m_beatmap->GetTimingPoint(time, m_currentTiming, !allowReset);
}

Beatmap::LaneTogglePointsIterator BeatmapPlayback::m_SelectLaneTogglePoint(MapTime time, bool allowReset)
{
	Beatmap::LaneTogglePointsIterator objStart = m_currentLaneTogglePoint;

	if (IsEndLaneToggle(objStart))
		return objStart;

	// Start at front of array if current object lies ahead of given input time
	if (objStart->time > time && allowReset)
		objStart = m_beatmap->GetFirstLaneTogglePoint();

	// Keep advancing the start pointer while the next object's starting time lies before the input time
	while (true)
	{
		if (!IsEndLaneToggle(objStart + 1) && (objStart + 1)->time <= time)
		{
			objStart = objStart + 1;
		}
		else
			break;
	}

	return objStart;
}

Beatmap::ObjectsIterator BeatmapPlayback::m_SelectHitObject(MapTime time, bool allowReset)
{
	Beatmap::ObjectsIterator objStart = m_currObject;
	if (IsEndObject(objStart))
		return objStart;

	// Start at front of array if current object lies ahead of given input time
	if (objStart[0]->time > time && allowReset)
		objStart = m_beatmap->GetFirstObjectState();

	// Keep advancing the start pointer while the next object's starting time lies before the input time
	while (true)
	{
		if (!IsEndObject(objStart) && objStart[0]->time < time)
		{
			objStart = std::next(objStart);
		}
		else
			break;
	}

	return objStart;
}

bool BeatmapPlayback::IsEndObject(const Beatmap::ObjectsIterator& obj) const
{
	return obj == m_beatmap->GetEndObjectState();
}

bool BeatmapPlayback::IsEndTiming(const Beatmap::TimingPointsIterator& obj) const
{
	return obj == m_beatmap->GetEndTimingPoint();
}

bool BeatmapPlayback::IsEndLaneToggle(const Beatmap::LaneTogglePointsIterator& obj) const
{
	return obj == m_beatmap->GetEndLaneTogglePoint();
}