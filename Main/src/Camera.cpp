#include "stdafx.h"
#include "Camera.hpp"
#include "Application.hpp"
#include "Track.hpp"

const float ZOOM_POW = 1.65f;

Camera::Camera()
{
	m_spinType = SpinStruct::SpinType::None;
}
Camera::~Camera()
{

}

static float DampedSin(float t, float amplitude, float frequency, float decay)
{
	return amplitude * (float)pow(Math::e, -decay * t) * sin(frequency * 2 * t * Math::pi);
}

static float Swing(float time) { return DampedSin(time, 120.0f / 360, 1, 3.5f); }

static void Spin(float time, float& roll, float& bgAngle, float dir)
{
	const float TSPIN = 0.75f / 2.0f;
	const float TRECOV = 0.75f / 2.0f;

	bgAngle = Math::Clamp(time * 4.0f, 0.0f, 2.0f) * dir;
	if (time <= TSPIN)
		roll = -dir * (TSPIN - time) / TSPIN;
	else
	{
		if (time < TSPIN + TRECOV)
			roll = Swing((time - TSPIN) / TRECOV) * 0.25f * dir;
		else roll = 0.0f;
	}
}

static float PitchScaleFunc(float input)
{
	float kLower = -4, uLower;
	float kUpper = 5.59, uUpper;

	if (g_aspectRatio < 1.0f)
	{
		uLower = -2.8195f;
		uUpper = 4.675f;
	}
	else
	{
		uLower = -3.05f;
		uUpper = 4.75f;
	}

	int rot = 0, dir = input < 0 ? -1 : 1;
	if (dir == -1)
	{
		while (input < -12.00)
		{
			input += 24.00;
			rot++;
		}
	}
	else
	{
		while (input > 12.00)
		{
			input -= 24.00;
			rot++;
		}
	}

	double scaled = input;
	if (input < kLower)
		scaled = -(-(input - kLower) / (12 + kLower)) * (12 + uLower) + uLower;
	else if (input < 0.00)
		scaled = (input / kLower) * uLower;
	else if (input < kUpper)
		scaled = (input / kUpper) * uUpper;
	else scaled = ((input - kUpper) / (12 - kUpper)) * (12 - uUpper) + uUpper;

	return rot * dir * 24.00 + scaled;
};


static Transform GetOriginTransform(float pitch, float offs, float roll)
{
	if (g_aspectRatio < 1.0f)
	{
		auto origin = Transform::Rotation({ 1, 0, roll }); // Reduce rotation radius
		auto anchor = Transform::Translation({ offs, -0.8f, 0 })
			* Transform::Rotation({ 1.5f, 0, 0 });
		auto contnr = Transform::Translation({ 0, 0, -0.9f })
			* Transform::Rotation({ -90 + pitch, 0, 0, });
		return origin * anchor * contnr;
	}
	else
	{
		auto origin = Transform::Rotation({ 0, 0, roll });
		auto anchor = Transform::Translation({ offs, -0.9f, 0 })
			* Transform::Rotation({ 1.5f, 0, 0 });
		auto contnr = Transform::Translation({ 0, 0, -0.9f })
			* Transform::Rotation({ -90 + pitch, 0, 0, });
		return origin * anchor * contnr;
	}

};

void Camera::Tick(float deltaTime, class BeatmapPlayback& playback)
{
	auto LerpTo = [&](float& value, float target, float speed = 0.5f)
	{
		float change = deltaTime * speed;

		if (target < value)
			value = Math::Max(value - change, target);
		else value = Math::Min(value + change, target);
	};

	const TimingPoint& currentTimingPoint = playback.GetCurrentTimingPoint();
	// Percentage of m_rollIntensity where camera rolls at its slowest rate
	const float slowestTiltThreshold = 0.1f;
	const float rollSpeed = 4;
	float speedLimit = MAX_ROLL_ANGLE * rollSpeed;
	float actualRollTarget = 0;

	// Lerp crit line position
	if (m_slowTilt)
		// Roll even slower when roll is less than 1 / 10 of tilt
		speedLimit /= fabsf(m_critLineRoll) > MAX_ROLL_ANGLE * slowestTiltThreshold ? 4.f : 8.f;
	LerpTo(m_critLineRoll, m_targetCritLineRoll, speedLimit);

	if (pManualTiltEnabled)
	{
		if (m_manualTiltInstant)
			m_actualRoll = pLaneTilt;
		else
			// Lerp to manual tilt value
			actualRollTarget = pLaneTilt;
	}
	else if (m_rollKeep)
	{
		actualRollTarget = m_rollKeepTargetRoll * m_rollIntensity;
	}
	else
	{
		// Get highway tilt target based off of crit line position
		actualRollTarget = (m_critLineRoll / MAX_ROLL_ANGLE) * m_rollIntensity;
	}

	// Roll to crit line position or roll keep value with respect to roll intensity
	// 2.5 corresponds to BIGGEST roll speed
	// Don't respect roll intensity if manual tilt is on, was recently toggled (off) or if roll speed is somehow 0
	speedLimit = MAX_ROLL_ANGLE * rollSpeed *
		Math::Max(m_rollIntensity, m_oldRollIntensity) / MAX_ROLL_ANGLE;
	if (speedLimit == 0 || pManualTiltEnabled || m_manualTiltRecentlyToggled)
		speedLimit = MAX_ROLL_ANGLE * rollSpeed * 2.5f;

	if (pManualTiltEnabled || m_manualTiltRecentlyToggled)
	{
		// If there's more than a 10 degree delta between the tilt target and the current roll,
		// increase roll speed to catch up
		float delta = fabsf(m_actualRoll - actualRollTarget) - MAX_ROLL_ANGLE;
		if (delta > 0)
			speedLimit *= 1 + (delta * 360 / 2.5f);
	}

	if (!m_manualTiltInstant)
		// Lerp highway tilt
		LerpTo(m_actualRoll, actualRollTarget, speedLimit);

	if (m_manualTiltRecentlyToggled)
		// Check if roll has met target
		m_manualTiltRecentlyToggled = m_actualRoll != actualRollTarget;

	for (int index = 0; index < 2; ++index)
	{
		m_rollIgnoreTimer[index] -= deltaTime;

		// Apply slam roll for 100ms
		if (m_rollIgnoreTimer[index] <= m_rollIgnoreDuration)
			m_slamRoll[index] = 0;
	}

	m_spinProgress = (float)(playback.GetLastTime() - m_spinStart) / m_spinDuration;
	// Calculate camera spin
	// TODO(local): spins need a progress of 1
	if (m_spinProgress < 2.0f)
	{
		if (m_spinType == SpinStruct::SpinType::Full)
		{
			Spin(m_spinProgress / 2.0f, m_spinRoll, m_bgSpin, m_spinDirection);
		}
		else if (m_spinType == SpinStruct::SpinType::Quarter)
		{
			const float BG_SPIN_SPEED = 4.0f / 3.0f;
			m_bgSpin = Math::Clamp(m_spinProgress * BG_SPIN_SPEED / 2, 0.0f, 1.0f) * m_spinDirection;
			m_spinRoll = Swing(m_spinProgress / 2) * m_spinDirection;
		}
		else if (m_spinType == SpinStruct::SpinType::Bounce)
		{
			m_bgSpin = 0.0f;
			m_spinBounceOffset = DampedSin(m_spinProgress / 2, m_spinBounceAmplitude,
				m_spinBounceFrequency / 2, m_spinBounceDecay) * m_spinDirection;
		}

		m_spinProgress = Math::Clamp(m_spinProgress, 0.0f, 2.0f);
	}
	else
	{
		m_bgSpin = 0.0f;
		m_spinRoll = 0.0f;
		m_spinProgress = 0.0f;
		m_spinBounceOffset = 0.0f;
	}

	m_totalRoll = m_spinRoll + m_actualRoll;
	m_totalOffset = (pLaneOffset * (5 * 100) / (6 * 116)) / 2.0f + m_spinBounceOffset;

	// Update camera shake effects
	if (m_shakeEffect.amplitudeToBeAdded != 0)
	{
		m_shakeEffect.amplitude += m_shakeEffect.amplitudeToBeAdded;
		m_shakeEffect.amplitudeToBeAdded = 0;
		m_shakeEffect.guard = m_shakeEffect.guardDuration;
	}
	else if (fabsf(m_shakeEffect.amplitude) > 0)
	{
		float shakeDecrement = 0.2f * (deltaTime / (1 / 60.f)); // Reduce shake by constant amount
		m_shakeEffect.amplitude = Math::Max(fabsf(m_shakeEffect.amplitude) - shakeDecrement, 0.f) * Math::Sign(m_shakeEffect.amplitude);
	}
	m_shakeOffset = m_shakeEffect.amplitude;
	m_shakeEffect.guard -= deltaTime;

	float lanePitch = PitchScaleFunc(pLanePitch) * pitchUnit;

	worldNormal = GetOriginTransform(lanePitch, m_totalOffset, m_totalRoll * 360.0f);
	worldNoRoll = GetOriginTransform(lanePitch, 0, 0);

	auto GetZoomedTransform = [&](Transform t)
	{
		auto zoomDir = t.GetPosition();
		float highwayDist = zoomDir.Length();
		zoomDir = zoomDir.Normalized();

		float zoomAmt;
		if (pLaneZoom <= 0) zoomAmt = pow(ZOOM_POW, -pLaneZoom) - 1;
		else zoomAmt = highwayDist * (pow(ZOOM_POW, -pow(pLaneZoom, 1.35f)) - 1);

		return Transform::Translation(zoomDir * zoomAmt) * t;
	};

	track->trackOrigin = GetZoomedTransform(worldNormal);

	critOrigin = GetZoomedTransform(GetOriginTransform(lanePitch, m_totalOffset, m_actualRoll * 360.0f + sin(m_spinRoll * Math::pi * 2) * 20));
}
void Camera::AddCameraShake(float cameraShake)
{
	// Ensures the red laser's slam shake is prioritised
	// Shake guard is set after this function is called
	if (m_shakeEffect.guard <= 0)
		m_shakeEffect.amplitudeToBeAdded = -cameraShake;
}
void Camera::AddRollImpulse(float dir, float strength)
{
	m_rollVelocity += dir * strength;
}

void Camera::SetRollIntensity(float val)
{
	m_oldRollIntensity = m_rollIntensity;
	m_rollIntensity = val;
}

bool Camera::GetRollKeep()
{
	return m_rollKeep;
}

void Camera::SetRollKeep(bool rollKeep)
{
	m_rollKeep = rollKeep;
}

void Camera::SetSlowTilt(bool tilt)
{
	m_slowTilt = tilt;
}

void Camera::SetSlamAmount(uint32 index, float amount)
{
	assert(index <= 1);
	if (m_slamDuration != 0)
	{
		m_slamRoll[index] = amount;
		SetRollIgnore(index, true);
	}
}

void Camera::SetRollIgnore(uint32 index, bool slam)
{
	assert(index <= 1);
	m_rollIgnoreTimer[index] = m_rollIgnoreDuration + (slam ? m_slamDuration : 0);
}

float Camera::GetRollIgnoreTimer(uint32 index)
{
	assert(index <= 1);
	return m_rollIgnoreTimer[index];
}

float Camera::GetSlamAmount(uint32 index)
{
	assert(index <= 1);
	return m_slamRoll[index];
}

void Camera::SetSlamShakeGuardDuration(int refreshRate)
{
	m_shakeEffect.guardDuration = 1.f / refreshRate;
}

void Camera::SetManualTilt(bool manualTilt)
{
	if (pManualTiltEnabled != manualTilt)
		m_manualTiltRecentlyToggled = true;

	pManualTiltEnabled = manualTilt;
}

void Camera::SetManualTiltInstant(bool instant)
{
	m_manualTiltInstant = instant;
}

void Camera::SetFancyHighwayTilt(bool fancyHighWaySetting)
{
	if (!fancyHighWaySetting)
	{
		m_rollIgnoreDuration = 0;
		m_slamDuration = 0;
	}
}

Vector2 Camera::Project(const Vector3& pos)
{
	Vector3 cameraSpace = m_rsLast.cameraTransform.TransformPoint(pos);
	Vector3 screenSpace = m_rsLast.projectionTransform.TransformPoint(cameraSpace);
	screenSpace.y = -screenSpace.y;
	screenSpace *= 0.5f;
	screenSpace += Vector2(0.5f, 0.5f);
	screenSpace *= m_rsLast.viewportSize;
	return screenSpace.xy();
}

RenderState Camera::CreateRenderState(bool clipped)
{
	int portrait = g_aspectRatio > 1 ? 0 : 1;

	// Extension of clipping planes in outward direction
	float viewRangeExtension = clipped ? 0.0f : 5.0f;

	RenderState rs = g_application->GetRenderStateBase();

	auto critDir = worldNoRoll.GetPosition().Normalized();
	float rotToCrit = -atan2(critDir.y, -critDir.z) * Math::radToDeg;

	float fov = fovs[portrait];
	float cameraRot = fov / 2 - fov * pitchOffsets[portrait];

	m_actualCameraPitch = rotToCrit - cameraRot + basePitch[portrait];
	auto cameraTransform = Transform::Rotation(Vector3(m_actualCameraPitch, m_shakeOffset, 0));

	// Calculate clipping distances
	Vector3 toTrackEnd = (track->trackOrigin).TransformPoint(Vector3(0.0f, track->trackLength, 0));
	Vector3 toTrackBegin = (track->trackOrigin).TransformPoint(Vector3(0.0f, -1.f, 0.f));

	float radPitch = Math::degToRad * m_actualCameraPitch;
	float endDist = -VectorMath::Dot(toTrackEnd, { 0, sinf(radPitch) ,cosf(radPitch) });
	float beginDist = -VectorMath::Dot(toTrackBegin, { 0, sinf(radPitch) ,cosf(radPitch) });
	float clipFar = Math::Max(endDist, beginDist);
	float clipNear = Math::Min(endDist, beginDist);

	rs.cameraTransform = cameraTransform;
	rs.projectionTransform = ProjectionMatrix::CreatePerspective(fov, g_aspectRatio, Math::Max(clipNear, 0.1f), clipFar + viewRangeExtension);

	m_rsLast = rs;
	return rs;
}

void Camera::SetTargetRoll(float target)
{
	auto ShouldRollDuringKeep = [](float target, float roll)
	{
		return (roll == 0) || (Math::Sign(roll) == Math::Sign(target) && fabsf(roll) < fabsf(target));
	};

	float slamRollTotal = m_slamRoll[0] + m_slamRoll[1];
	m_targetCritLineRoll = Math::Clamp(target + slamRollTotal, -1.f, 1.f) * MAX_ROLL_ANGLE;

	// Always keep track of roll target without slams to prevent 
	// inconsistent roll keep behaviour at lower frame rates
	float actualTarget = Math::Clamp(target, -1.f, 1.f);
	if (!m_rollKeep || ShouldRollDuringKeep(actualTarget, m_rollKeepTargetRoll))
		m_rollKeepTargetRoll = actualTarget;
}

void Camera::SetSpin(float direction, uint32 duration, uint8 type, class BeatmapPlayback& playback)
{
	const TimingPoint& currentTimingPoint = playback.GetCurrentTimingPoint();

	m_spinDirection = direction;
	m_spinDuration = (duration / 192.0f) * (currentTimingPoint.beatDuration) * 4;
	m_spinStart = playback.GetLastTime();
	m_spinType = type;
}

void Camera::SetXOffsetBounce(float direction, uint32 duration, uint32 amplitude, uint32 frequency, float decay, class BeatmapPlayback& playback)
{
	const TimingPoint& currentTimingPoint = playback.GetCurrentTimingPoint();

	m_spinDirection = direction;
	// since * 2 and stuff
	m_spinDuration = 0.5f * (duration / 192.0f) * (currentTimingPoint.beatDuration) * 4;
	m_spinStart = playback.GetLastTime();
	m_spinType = SpinStruct::SpinType::Bounce;

	m_spinBounceAmplitude = amplitude / 250.0f;
	m_spinBounceFrequency = frequency;
	m_spinBounceDecay = decay == 0 ? 0 : (decay == 1 ? 1.5f : 3.0f);
}

void Camera::SetLasersActive(bool lasersActive)
{
	m_lasersActive = lasersActive;
}

float Camera::GetRoll() const
{
	return m_totalRoll;
}

float Camera::GetCritLineRoll() const
{
	return m_critLineRoll;
}

float Camera::GetActualRoll() const
{
	return m_actualRoll;
}

float Camera::GetHorizonHeight()
{
	float angle = fmodf(m_actualCameraPitch + PitchScaleFunc(pLanePitch) * pitchUnit, 360.0f);
	return (0.5 + (-angle / fovs[g_aspectRatio > 1.0f ? 0 : 1])) * m_rsLast.viewportSize.y;
}

Vector2i Camera::GetScreenCenter()
{
	Vector2i ret = Vector2i(0, GetHorizonHeight());

	uint8 portrait = g_aspectRatio > 1.0f ? 0 : 1;
	float fov = fovs[portrait];

	ret.x = m_rsLast.viewportSize.x / 2;
	ret.x -= (m_shakeOffset / (fov * g_aspectRatio)) * m_rsLast.viewportSize.x;

	return ret;
}

float Camera::GetShakeOffset()
{
	return m_shakeOffset;
}

float Camera::m_ClampRoll(float in) const
{
	float ain = fabs(in);
	if (ain < 1.0f)
		return in;
	bool odd = ((uint32)fabs(in) % 2) == 1;
	float sign = Math::Sign(in);
	if (odd)
	{
		// Swap sign and modulo
		return -sign * (1.0f - fmodf(ain, 1.0f));
	}
	else
	{
		// Keep sign and modulo
		return sign * fmodf(ain, 1.0f);
	}
}
