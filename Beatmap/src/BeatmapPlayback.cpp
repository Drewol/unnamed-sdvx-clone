#include "stdafx.h"
#include "BeatmapPlayback.hpp"

BeatmapPlayback::BeatmapPlayback(Beatmap& beatmap) : m_beatmap(&beatmap)
{
}
BeatmapPlayback::~BeatmapPlayback()
{
	if (m_isCalibration) {
		for (auto* o : m_calibrationObjects) {
			delete o;
		}
		m_calibrationObjects.clear();
		delete m_timingPoints.at(0);
		m_timingPoints.clear();
	}
}
bool BeatmapPlayback::Reset(MapTime initTime, MapTime start)
{
	m_effectObjects.clear();
	m_timingPoints = m_beatmap->GetLinearTimingPoints();
	m_chartStops = m_beatmap->GetLinearChartStops();
	m_objects = m_beatmap->GetLinearObjects();
	m_zoomPoints = m_beatmap->GetZoomControlPoints();
	m_laneTogglePoints = m_beatmap->GetLaneTogglePoints();

	if (m_objects.size() == 0)
		return false;
	if (m_timingPoints.size() == 0)
		return false;

	Logf("Resetting BeatmapPlayback, InitTime = %d, Start = %d", Logger::Severity::Info, initTime, start);
	m_playbackTime = initTime;

	// Ensure that nothing could go wrong when the start is 0
	if (start <= 0) start = std::numeric_limits<decltype(start)>::min();
	m_viewRange = { start, start };

	m_currentObj = &m_objects.front();
	m_currentAlertObj = &m_objects.front();
	m_currentLaserObj = &m_objects.front();
	m_currentTiming = &m_timingPoints.front();
	m_currentZoomPoint = m_zoomPoints.empty() ? nullptr : &m_zoomPoints.front();
	for (ZoomControlPoint* z : m_zoomPoints)
	{
		if (z->time != INT32_MIN) //Not a starting point.
			break;

		m_zoomStartPoints[z->index] = z;
	}
	m_currentLaneTogglePoint = m_laneTogglePoints.empty() ? nullptr : &m_laneTogglePoints.front();

	//hittableLaserEnter = (*m_currentTiming)->beatDuration * 4.0;
	//alertLaserThreshold = (*m_currentTiming)->beatDuration * 6.0;
	m_hittableObjects.clear();
	m_holdObjects.clear();

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
	TimingPoint** timingEnd = m_SelectTimingPoint(m_playbackTime);
	if (timingEnd != nullptr && timingEnd != m_currentTiming)
	{
		m_currentTiming = timingEnd;
		/// TODO: Investigate why this causes score to be too high
		//hittableLaserEnter = (*m_currentTiming)->beatDuration * 4.0;
		//alertLaserThreshold = (*m_currentTiming)->beatDuration * 6.0;
		OnTimingPointChanged.Call(*m_currentTiming);
	}

	// Advance lane toggle
	LaneHideTogglePoint** laneToggleEnd = m_SelectLaneTogglePoint(m_playbackTime);
	if (laneToggleEnd != nullptr && laneToggleEnd != m_currentLaneTogglePoint)
	{
		m_currentLaneTogglePoint = laneToggleEnd;
		OnLaneToggleChanged.Call(*m_currentLaneTogglePoint);
	}

	// Advance objects
	ObjectState** objEnd = m_SelectHitObject(m_playbackTime + hittableObjectEnter);
	if (objEnd != nullptr && objEnd != m_currentObj)
	{
		for (auto it = m_currentObj; it < objEnd; it++)
		{
			MultiObjectState* obj = **it;
			if (obj->type == ObjectType::Laser) continue;

			if (!m_viewRange.Includes(obj->time)) continue;
			if (obj->type == ObjectType::Hold && !m_viewRange.Includes(obj->time + obj->hold.duration, true)) continue;

			if (obj->type == ObjectType::Hold || obj->type == ObjectType::Single)
			{
				m_holdObjects.Add(*obj);
			}
			m_hittableObjects.AddUnique(*it);
			OnObjectEntered.Call(*it);
		}
		m_currentObj = objEnd;
	}


	// Advance lasers
	objEnd = m_SelectHitObject(m_playbackTime + hittableLaserEnter);
	if (objEnd != nullptr && objEnd != m_currentLaserObj)
	{
		for (auto it = m_currentLaserObj; it < objEnd; it++)
		{
			MultiObjectState* obj = **it;
			if (obj->type != ObjectType::Laser) continue;

			if (!m_viewRange.Includes(obj->time)) continue;
			if (!m_viewRange.Includes(obj->time + obj->laser.duration, true)) continue;

			m_holdObjects.Add(*obj);
			m_hittableObjects.AddUnique(*it);
			OnObjectEntered.Call(*it);
		}
		m_currentLaserObj = objEnd;
	}


	// Check for lasers within the alert time
	objEnd = m_SelectHitObject(m_playbackTime + alertLaserThreshold);
	if (objEnd != nullptr && objEnd != m_currentAlertObj)
	{
		for (auto it = m_currentAlertObj; it < objEnd; it++)
		{
			MultiObjectState* obj = **it;
			if (!m_viewRange.Includes(obj->time)) continue;

			if (obj->type == ObjectType::Laser)
			{
				LaserObjectState* laser = (LaserObjectState*)obj;
				if (!laser->prev)
					OnLaserAlertEntered.Call(laser);
			}
		}
		m_currentAlertObj = objEnd;
	}

	// Advance zoom points
	if (m_currentZoomPoint)
	{
		ZoomControlPoint** objEnd = m_SelectZoomObject(m_playbackTime);
		for (auto it = m_currentZoomPoint; it < objEnd; it++)
		{
			// Set this point as new start point
			uint32 index = (*it)->index;
			m_zoomStartPoints[index] = *it;

			// Set next point
			m_zoomEndPoints[index] = nullptr;
			ZoomControlPoint** ptr = it + 1;
			while (!IsEndZoomPoint(ptr))
			{
				if ((*ptr)->index == index)
				{
					m_zoomEndPoints[index] = *ptr;
					break;
				}
				ptr++;
			}
		}
		m_currentZoomPoint = objEnd;
	}

	// Check passed hittable objects
	MapTime objectPassTime = m_playbackTime - hittableObjectLeave;
	for (auto it = m_hittableObjects.begin(); it != m_hittableObjects.end();)
	{
		MultiObjectState* obj = **it;
		if (obj->type == ObjectType::Hold)
		{
			MapTime endTime = obj->hold.duration + obj->time;
			if (endTime < objectPassTime)
			{
				OnObjectLeaved.Call(*it);
				it = m_hittableObjects.erase(it);
				continue;
			}
			if (obj->hold.effectType != EffectType::None && // Hold button with effect
				obj->time - 100 <= m_playbackTime + audioOffset && endTime - 100 > m_playbackTime + audioOffset) // Hold button in active range
			{
				if (!m_effectObjects.Contains(*obj))
				{
					OnFXBegin.Call((HoldObjectState*)*it);
					m_effectObjects.Add(*obj);
				}
			}
		}
		else if (obj->type == ObjectType::Laser)
		{
			if ((obj->laser.duration + obj->time) < objectPassTime)
			{
				OnObjectLeaved.Call(*it);
				it = m_hittableObjects.erase(it);
				continue;
			}
		}
		else if (obj->type == ObjectType::Single)
		{
			if (obj->time < objectPassTime)
			{
				OnObjectLeaved.Call(*it);
				it = m_hittableObjects.erase(it);
				continue;
			}
		}
		else if (obj->type == ObjectType::Event)
		{
			EventObjectState* evt = (EventObjectState*)obj;
			if (obj->time < (m_playbackTime + 2)) // Tiny offset to make sure events are triggered before they are needed
			{
				// Trigger event
				OnEventChanged.Call(evt->key, evt->data);
				m_eventMapping[evt->key] = evt->data;
				it = m_hittableObjects.erase(it);
				continue;
			}
		}
		it++;
	}

	// Remove passed hold objects
	for (auto it = m_holdObjects.begin(); it != m_holdObjects.end();)
	{
		MultiObjectState* obj = **it;
		if (obj->type == ObjectType::Hold)
		{
			MapTime endTime = obj->hold.duration + obj->time;
			if (endTime < objectPassTime)
			{
				it = m_holdObjects.erase(it);
				continue;
			}
			if (endTime < m_playbackTime)
			{
				if (m_effectObjects.Contains(*it))
				{
					OnFXEnd.Call((HoldObjectState*)*it);
					m_effectObjects.erase(*it);
				}
			}
		}
		else if (obj->type == ObjectType::Laser)
		{
			if ((obj->laser.duration + obj->time) < objectPassTime)
			{
				it = m_holdObjects.erase(it);
				continue;
			}
		}
		else if (obj->type == ObjectType::Single)
		{
			if (obj->time < objectPassTime)
			{
				it = m_holdObjects.erase(it);
				continue;
			}
		}
		it++;
	}
}

void BeatmapPlayback::MakeCalibrationPlayback()
{
	m_isCalibration = true;
	m_timingPoints.clear();

	for (size_t i = 0; i < 50; i++)
	{
		ButtonObjectState* newObject = new ButtonObjectState();
		newObject->index = i % 4;
		newObject->time = i * 500;
		m_calibrationObjects.Add((ObjectState*)newObject);
	}

	TimingPoint* calibrationTiming = new TimingPoint();
	calibrationTiming->beatDuration = 500;
	calibrationTiming->time = 0;
	calibrationTiming->denominator = 4;
	calibrationTiming->numerator = 4;
	m_timingPoints.Add(calibrationTiming);
	m_currentTiming = &m_timingPoints.front();
}

ObjectState* BeatmapPlayback::GetFirstButtonOrHoldAfterTime(MapTime time, int lane)
{
	// Iterate though objects
	for (ObjectState** obj = &m_objects.front(); !IsEndObject(obj); obj++)
	{
		if (obj[0]->time < time)
			continue;

		MultiObjectState* mobj = (MultiObjectState*)obj[0];
		if (obj[0]->type != ObjectType::Hold && obj[0]->type != ObjectType::Single)
			continue;

		if (mobj->button.index != lane)
			continue;
		return *obj;
	}
	return nullptr;
}

Vector<ObjectState*> BeatmapPlayback::GetObjectsInRange(MapTime range)
{
	static const uint32 earlyVisibility = 200;

	const TimingPoint& tp = GetCurrentTimingPoint();

	MapTime begin = (MapTime) (m_playbackTime - earlyVisibility);
	MapTime end = m_playbackTime + range;

	Vector<ObjectState*> ret;

	if (m_isCalibration) {
		for (auto& o : m_calibrationObjects)
		{
			if (o->time < begin)
				continue;
			if (o->time > end)
				break;

			ret.Add(o);
		}
		return ret;
	}

	if (begin < m_viewRange.begin) begin = m_viewRange.begin;
	if (m_viewRange.HasEnd() && end >= m_viewRange.end) end = m_viewRange.end;

	// Add hold objects
	for (auto& ho : m_holdObjects)
	{
		ret.AddUnique(ho);
	}

	// Iterator
	ObjectState** obj = m_currentObj;
	// Return all objects that lie after the currently queued object and fall within the given range
	while (!IsEndObject(obj))
	{
		if ((*obj)->time < begin)
		{
			obj += 1;
			continue;
		}

		if ((*obj)->time >= end)
			break; // No more objects

		ret.AddUnique(*obj);
		obj += 1; // Next
	}

	return ret;
}

const TimingPoint& BeatmapPlayback::GetCurrentTimingPoint() const
{
	if (!m_currentTiming)
		return *m_timingPoints.front();
	return **m_currentTiming;
}
const TimingPoint* BeatmapPlayback::GetTimingPointAt(MapTime time) const
{
	return *const_cast<BeatmapPlayback*>(this)->m_SelectTimingPoint(time);
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
MapTime BeatmapPlayback::ViewDistanceToDuration(float distance)
{
	TimingPoint** tp = m_SelectTimingPoint(m_playbackTime, true);

	double time = 0;

	MapTime currentTime = m_playbackTime;
	while (true)
	{
		if (!IsEndTiming(tp + 1))
		{
			double maxDist = (tp[1]->time - (double)currentTime) / tp[0]->beatDuration;
			if (maxDist < distance)
			{
				// Split up
				time += maxDist * tp[0]->beatDuration;
				distance -= (float)maxDist;
				tp++;
				continue;
			}
		}
		time += distance * tp[0]->beatDuration;
		break;
	}

	/// TODO: Optimize?
	uint32 processedStops = 0;
	Vector<ChartStop*> ignoreStops;
	do
	{
		processedStops = 0;
		for (auto cs : m_SelectChartStops(currentTime, time))
		{
			if (std::find(ignoreStops.begin(), ignoreStops.end(), cs) != ignoreStops.end())
				continue;
			time += cs->duration;
			processedStops++;
			ignoreStops.Add(cs);
		}
	} while (processedStops);

	return (MapTime)time;
}
float BeatmapPlayback::DurationToViewDistance(MapTime duration)
{
	return DurationToViewDistanceAtTime(m_playbackTime, duration);
}

float BeatmapPlayback::DurationToViewDistanceAtTimeNoStops(MapTime time, MapTime duration)
{
	MapTime endTime = time + duration;
	int8 direction = Math::Sign(duration);
	if (duration < 0)
	{
		MapTime temp = time;
		time = endTime;
		endTime = temp;
		duration *= -1;
	}

	// Accumulated value
	double barTime = 0.0f;

	// Split up to see if passing other timing points on the way
	TimingPoint** tp = m_SelectTimingPoint(time, true);
	while (true)
	{
		if (!IsEndTiming(tp + 1))
		{
			if (tp[1]->time < endTime)
			{
				// Split up
				MapTime myDuration = tp[1]->time - time;
				barTime += (double)myDuration / tp[0]->beatDuration;
				duration -= myDuration;
				time = tp[1]->time;
				tp++;
				continue;
			}
		}
		// Whole
		barTime += (double)duration / tp[0]->beatDuration;
		break;
	}

	return (float)barTime * direction;
}

float BeatmapPlayback::DurationToViewDistanceAtTime(MapTime time, MapTime duration)
{
	if (cMod)
	{
		return (float)duration / 480000.0f;
	}
	MapTime endTime = time + duration;
	int8 direction = Math::Sign(duration);
	if (duration < 0)
	{
		MapTime temp = time;
		time = endTime;
		endTime = temp;
		duration *= -1;
	}

	MapTime startTime = time;

	// Accumulated value
	double barTime = 0.0f;

	// Split up to see if passing other timing points on the way
	TimingPoint** tp = m_SelectTimingPoint(time, true);
	while (true)
	{
		if (!IsEndTiming(tp + 1))
		{
			if (tp[1]->time < endTime)
			{
				// Split up
				MapTime myDuration = tp[1]->time - time;
				barTime += (double)myDuration / tp[0]->beatDuration;
				duration -= myDuration;
				time = tp[1]->time;
				tp++;
				continue;
			}
		}
		// Whole
		barTime += (double)duration / tp[0]->beatDuration;
		break;
	}

	// calculate stop ViewDistance
	double stopTime = 0.;
	for (auto cs : m_SelectChartStops(startTime, endTime - startTime))
	{
		MapTime overlap = Math::Min(abs(endTime - startTime), 
			Math::Min(abs(endTime - cs->time), 
				Math::Min(abs((cs->time + cs->duration) - startTime), abs((cs->time + cs->duration) - cs->time))));

		stopTime += DurationToViewDistanceAtTimeNoStops(Math::Max(cs->time, startTime), overlap);
	}
	barTime -= stopTime;


	return (float)barTime * direction;
}

float BeatmapPlayback::TimeToViewDistance(MapTime time)
{
	if (cMod)
		return (float)(time - m_playbackTime) / (480000.f);

	return DurationToViewDistanceAtTime(m_playbackTime, time - m_playbackTime);
}

float BeatmapPlayback::GetBarTime() const
{
	return m_barTime;
}

float BeatmapPlayback::GetBeatTime() const
{
	return m_beatTime;
}

float BeatmapPlayback::GetZoom(uint8 index)
{
	assert(index >= 0 && index <= 4);
	MapTime startTime = m_zoomStartPoints[index] ? m_zoomStartPoints[index]->time : 0;
	float start = m_zoomStartPoints[index] ? m_zoomStartPoints[index]->zoom : 0.0f;
	if (!m_zoomEndPoints[index]) // Last point?
		return start;

	// Interpolate
	MapTime duration = m_zoomEndPoints[index]->time - startTime;
	MapTime currentOffsetInto = m_playbackTime - startTime;
	float zoomDelta = m_zoomEndPoints[index]->zoom - start;
	float f = (float)currentOffsetInto / (float)duration;
	return start + zoomDelta * f;
}

bool BeatmapPlayback::CheckIfManualTiltInstant()
{
	return m_zoomStartPoints[3] ? m_zoomStartPoints[3]->instant : false;
}

MapTime BeatmapPlayback::GetLastTime() const
{
	return m_playbackTime;
}
TimingPoint** BeatmapPlayback::m_SelectTimingPoint(MapTime time, bool allowReset)
{
	TimingPoint** objStart = m_currentTiming;
	if (IsEndTiming(objStart))
		return objStart;

	// Start at front of array if current object lies ahead of given input time
	if (objStart[0]->time > time && allowReset)
		objStart = &m_timingPoints.front();

	// Keep advancing the start pointer while the next object's starting time lies before the input time
	while (true)
	{
		if (!IsEndTiming(objStart + 1) && objStart[1]->time <= time)
		{
			objStart = objStart + 1;
		}
		else
			break;
	}

	return objStart;
}

Vector<ChartStop*> BeatmapPlayback::m_SelectChartStops(MapTime time, MapTime duration)
{
	Vector<ChartStop*> stops;
	for (auto cs : m_chartStops)
	{
		if (time <= cs->time + cs->duration && time + duration >= cs->time)
			stops.Add(cs);
	}
	return stops;
}



LaneHideTogglePoint** BeatmapPlayback::m_SelectLaneTogglePoint(MapTime time, bool allowReset)
{
	LaneHideTogglePoint** objStart = m_currentLaneTogglePoint;

	if (IsEndLaneToggle(objStart))
		return objStart;

	// Start at front of array if current object lies ahead of given input time
	if (objStart[0]->time > time && allowReset)
		objStart = &m_laneTogglePoints.front();

	// Keep advancing the start pointer while the next object's starting time lies before the input time
	while (true)
	{
		if (!IsEndLaneToggle(objStart + 1) && objStart[1]->time <= time)
		{
			objStart = objStart + 1;
		}
		else
			break;
	}

	return objStart;
}


ObjectState** BeatmapPlayback::m_SelectHitObject(MapTime time, bool allowReset)
{
	ObjectState** objStart = m_currentObj;
	if (IsEndObject(objStart))
		return objStart;

	// Start at front of array if current object lies ahead of given input time
	if (objStart[0]->time > time && allowReset)
		objStart = &m_objects.front();

	// Keep advancing the start pointer while the next object's starting time lies before the input time
	while (true)
	{
		if (!IsEndObject(objStart) && objStart[0]->time < time)
		{
			objStart = objStart + 1;
		}
		else
			break;
	}

	return objStart;
}
ZoomControlPoint** BeatmapPlayback::m_SelectZoomObject(MapTime time)
{
	ZoomControlPoint** objStart = m_currentZoomPoint;
	if (IsEndZoomPoint(objStart))
		return objStart;

	// Keep advancing the start pointer while the next object's starting time lies before the input time
	while (true)
	{
		if (!IsEndZoomPoint(objStart) && objStart[0]->time < time)
		{
			objStart = objStart + 1;
		}
		else
			break;
	}

	return objStart;
}

bool BeatmapPlayback::IsEndTiming(TimingPoint** obj)
{
	return obj == (&m_timingPoints.back() + 1);
}
bool BeatmapPlayback::IsEndObject(ObjectState** obj)
{
	return obj == (&m_objects.back() + 1);
}

bool BeatmapPlayback::IsEndLaneToggle(LaneHideTogglePoint** obj)
{
	return obj == (&m_laneTogglePoints.back() + 1);
}

bool BeatmapPlayback::IsEndZoomPoint(ZoomControlPoint** obj)
{
	return obj == (&m_zoomPoints.back() + 1);
}
