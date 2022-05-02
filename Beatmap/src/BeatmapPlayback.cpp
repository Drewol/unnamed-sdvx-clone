#include "stdafx.h"
#include "BeatmapPlayback.hpp"

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

	// Ensure that nothing could go wrong when the start is 0
	if (start <= 0) start = std::numeric_limits<decltype(start)>::min();
	m_playRange = { start, start };

	m_currObject = m_beatmap->GetFirstObjectState();

	m_currLaserObject = m_currObject;
	m_currAlertObject = m_currObject;

	m_currentTiming = m_beatmap->GetFirstTimingPoint();
	m_currentLaneTogglePoint = m_beatmap->GetFirstLaneTogglePoint();

	m_currentTrackRollBehaviour = TrackRollBehaviour::Normal;
	m_lastTrackRollBehaviourChange = 0;

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
			EventObjectState* evt = (EventObjectState*)obj;

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
				OnFXEnd.Call((HoldObjectState*)objState);
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

const ObjectState* BeatmapPlayback::GetFirstButtonOrHoldAfterTime(MapTime time, int lane) const
{
	for (const auto& obj : m_beatmap->GetObjectStates())
	{
		if (obj->time < time)
			continue;

		if (obj->type != ObjectType::Hold && obj->type != ObjectType::Single)
			continue;

		const MultiObjectState* mobj = *(obj.get());

		if (mobj->button.index != lane)
			continue;

		return obj.get();
	}

	return nullptr;
}

void BeatmapPlayback::GetObjectsInViewRange(float numBeats, Vector<ObjectState*>& objects)
{
	// TODO: properly implement backwards scroll speed support...
	// numBeats *= 3;

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

	for (Beatmap::ObjectsIterator obj = m_currObject; !IsEndObject(obj); ++obj)
	{
		const MapTime objTime = (*obj)->time;

		if (!m_playRange.Includes(objTime))
		{
			if (m_playRange.HasEnd() && objTime >= m_playRange.end)
			{
				break;
			}

			continue;
		}

		if (!IsEndTiming(tp_next) && tp_next->time <= objTime)
		{
			currBeats += GetViewDistance(currRefTime, tp_next->time);

			tp = tp_next;
			tp_next = std::next(tp_next);
			currRefTime = tp->time;
		}

		const float objBeats = currBeats + GetViewDistance(currRefTime, objTime);
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

inline MapTime GetLastBarPosition(const TimingPoint& tp, MapTime currTime, /* out*/ uint64& measureNo)
{
	MapTime offset = currTime - tp.time;
	if (offset < 0) offset = 0;

	measureNo = static_cast<uint64>(static_cast<double>(offset) / tp.GetBarDuration());

	return tp.time + static_cast<MapTime>(measureNo * tp.GetBarDuration());
}

// Arbitrary cutoff
constexpr uint64 MAX_DISP_BAR_COUNT = 1000;

void BeatmapPlayback::GetBarPositionsInViewRange(float numBeats, Vector<float>& barPositions) const
{
	uint64 measureNo = 0;
	MapTime currTime = 0;

	if (m_isCalibration)
	{
		const TimingPoint& ctp = m_calibrationTiming;
		currTime = GetLastBarPosition(ctp, m_playbackTime, measureNo);

		while (barPositions.size() < MAX_DISP_BAR_COUNT)
		{
			barPositions.Add(TimeToViewDistance(currTime));
			currTime = ctp.time + static_cast<MapTime>(++measureNo * ctp.GetBarDuration());

			if (currTime - m_playbackTime >= ctp.beatDuration * numBeats)
			{
				return;
			}
		}

		return;
	}

	Beatmap::TimingPointsIterator tp = m_SelectTimingPoint(m_playbackTime);
	assert(!IsEndTiming(tp));

	currTime = GetLastBarPosition(*tp, m_playbackTime, measureNo);

	while (barPositions.size() < MAX_DISP_BAR_COUNT)
	{
		barPositions.Add(TimeToViewDistance(currTime));
		
		Beatmap::TimingPointsIterator ntp = next(tp);
		currTime = tp->time + static_cast<MapTime>(++measureNo * tp->GetBarDuration());

		if (!IsEndTiming(ntp) && currTime >= ntp->time)
		{
			tp = ntp;
			currTime = ntp->time;
			measureNo = 0;
		}

		if (GetViewDistance(m_playbackTime, currTime) >= numBeats)
		{
			return;
		}
	}
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

	return m_beatmap->GetGraphValueAt(graphType, m_playbackTime);
}

float BeatmapPlayback::GetScrollSpeed() const
{
	return m_beatmap->GetScrollSpeedAt(m_playbackTime);
}

bool BeatmapPlayback::CheckIfManualTiltInstant()
{
	if (m_currentTrackRollBehaviour != TrackRollBehaviour::Manual)
	{
		return false;
	}

	return m_beatmap->CheckIfManualTiltInstant(m_lastTrackRollBehaviourChange, m_playbackTime);
}

Beatmap::TimingPointsIterator BeatmapPlayback::m_SelectTimingPoint(MapTime time, bool allowReset) const
{
	assert(!m_isCalibration);

	return m_beatmap->GetTimingPoint(time, m_currentTiming, !allowReset);
}

Beatmap::LaneTogglePointsIterator BeatmapPlayback::m_SelectLaneTogglePoint(MapTime time, bool allowReset) const
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

Beatmap::ObjectsIterator BeatmapPlayback::m_SelectHitObject(MapTime time, bool allowReset) const
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

Vector<String> BeatmapPlayback::GetStateString() const
{
	auto ObjectStateToStr = [](const ObjectState* state)
	{
		if (state == nullptr)
		{
			return String{"null"};
		}

		const auto* obj = (const MultiObjectState*) state;

		switch (obj->type)
		{
		case ObjectType::Single:
		{
			const auto* state = (const ButtonObjectState*)obj;
			return Utility::Sprintf("%d(type=single index=%u)", obj->time, static_cast<unsigned int>(state->index));
		}
		break;
		case ObjectType::Hold:
		{
			const auto* state = (const HoldObjectState*)obj;
			return Utility::Sprintf("%d(type=hold index=%u len=%d)", obj->time, static_cast<unsigned int>(state->index), state->duration);
		}
		break;
		case ObjectType::Laser:
		{
			const auto* state = (const LaserObjectState*)obj;
			return Utility::Sprintf("%d(type=laser %s len=%d start=%.3f end=%.3f flags=%x)",
				obj->time, state->index == 0 ? "left" : state->index == 1 ? "right" : "invalid", state->duration, state->points[0], state->points[1], static_cast<unsigned int>(state->flags));
		}
		break;
		case ObjectType::Event:
		{
			const auto* state = (const EventObjectState*)obj;
			return Utility::Sprintf("%d(type=event key=%d)",
				obj->time, static_cast<int>(state->key));
		}
		break;
		default:
			return Utility::Sprintf("%d(type=%u)", obj->time, static_cast<unsigned int>(obj->type));
		}
	};

	auto BeatmapObjectToStr = [&](const Beatmap::ObjectsIterator& it)
	{
		if (IsEndObject(it))
		{
			return String{"end"};
		}

		return ObjectStateToStr(it->get());
	};

	Vector<String> lines;

	const auto& tp = GetCurrentTimingPoint();
	const float currentBPM = (float)(60000.0 / tp.beatDuration);
	lines.Add(Utility::Sprintf("TimingPoint: time=%d bpm=%.3f sig=%d/%d", tp.time, currentBPM, tp.numerator, tp.denominator));

	lines.Add(Utility::Sprintf(
		"PlayRange: %d to %d | Playback: time=%d",
		m_playRange.begin, m_playRange.end, m_playbackTime
	));

	lines.Add(Utility::Sprintf("CurrObject: %s", BeatmapObjectToStr(m_currObject)));
	lines.Add(Utility::Sprintf("CurrLaser: %s", BeatmapObjectToStr(m_currLaserObject)));
	lines.Add(Utility::Sprintf("CurrAlert: %s", BeatmapObjectToStr(m_currAlertObject)));

	if (m_objectsByTime.empty())
	{
		lines.Add("Objects: none");
	}
	else
	{
		const auto* firstObj = m_objectsByTime.begin()->second;
		const auto* lastObj = m_objectsByTime.rbegin()->second;

		lines.Add(Utility::Sprintf("Objects: %u objects, %s to %s", m_objectsByTime.size(), ObjectStateToStr(firstObj), ObjectStateToStr(lastObj)));
	}

	return lines;
}
