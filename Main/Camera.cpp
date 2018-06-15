#include "stdafx.h"
#include "Camera.hpp"
#include "Application.hpp"
#include "Track.hpp"

Camera::Camera()
{

}
Camera::~Camera()
{

}
void Camera::Tick(float deltaTime, class BeatmapPlayback& playback)
{
	const TimingPoint& currentTimingPoint = playback.GetCurrentTimingPoint();
	//Calculate camera roll
	//Follow the laser track exactly but with a roll speed limit
	float rollSpeedLimit = (m_rollIntensity / (currentTimingPoint.beatDuration / 1000.0f)) * deltaTime;
	rollSpeedLimit /= m_targetRoll != 0.0f ? 1.0f : 2.0f;

	float rollDelta = m_targetRoll - m_laserRoll;
	rollSpeedLimit *= Math::Sign(rollDelta);
	m_laserRoll += (fabs(rollDelta) < fabs(rollSpeedLimit)) ? rollDelta : rollSpeedLimit;

	float spinProgress = (float)(playback.GetLastTime() - m_spinStart) / m_spinDuration;
	// Calculate camera spin
	if (spinProgress < 2.0f)
	{
		if (m_spinType == SpinStruct::SpinType::Full)
		{
			if(spinProgress <= 1.0f)
				m_spinRoll = -m_spinDirection * (1.0 - spinProgress);
			else
			{
				float amplitude = (15.0f / 360.0f) / (spinProgress + 1);
				m_spinRoll = sin(spinProgress * Math::pi * 2.0f) * amplitude * m_spinDirection;
			}
		}
		else if (m_spinType == SpinStruct::SpinType::Quarter)
		{
			float amplitude = (80.0f / 360.0f) / ((spinProgress) + 1);
			m_spinRoll = sin(spinProgress * Math::pi) * amplitude * m_spinDirection;
		}
	}
	else
	{
		m_spinRoll = 0.0f;
	}

	m_roll = m_spinRoll + m_laserRoll;

	if(!rollKeep)
	{
		m_targetRollSet = false;
		m_targetRoll = 0.0f;
	}

	// Update camera shake effects
	m_shakeOffset = Vector3(0.0f);

	m_shakeEffect.time -= deltaTime;
	if (m_shakeEffect.time >= 0.f)
	{
		float shakeProgress = m_shakeEffect.time / m_shakeEffect.duration;
		float shakeIntensity = sinf(powf(shakeProgress, 1.6) * Math::pi);

		Vector3 shakeVec = Vector3(m_shakeEffect.amplitude * shakeIntensity) * Vector3(cameraShakeX, cameraShakeY, cameraShakeZ);

		m_shakeOffset += shakeVec;
	}
}
void Camera::AddCameraShake(CameraShake cameraShake)
{
	m_shakeEffect = cameraShake;
}
void Camera::AddRollImpulse(float dir, float strength)
{
	m_rollVelocity += dir * strength;
}

void Camera::SetRollIntensity(float val)
{
	m_rollIntensity = val;
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

static float Lerp(float a, float b, float alpha)
{
	return a + (b - a) * alpha;
}

RenderState Camera::CreateRenderState(bool clipped)
{
#if false
	// Extension of clipping planes in outward direction
	float viewRangeExtension = clipped ? 0.0f : 5.0f;

	RenderState rs = g_application->GetRenderStateBase();
	
	uint8 portrait = g_aspectRatio > 1.0f ? 0 : 1;
	float fov = fovs[portrait];
	float pitchOffset = ( 0.5 - pitchOffsets[portrait]) * fov / 1.0f;

	// Tilt, Height and Near calculated from zoom values
	float base_pitch;
	if (pPitch <= 0)
		base_pitch = Lerp(minPitch[portrait], basePitch[portrait], pPitch + 1);
	else base_pitch = Lerp(basePitch[portrait], maxPitch[portrait], pPitch);

	float base_radius;
	if (pZoom <= 0)
		base_radius = Lerp(minRadius[portrait], baseRadius[portrait], pZoom + 1);
	else base_radius = Lerp(baseRadius[portrait], maxRadius[portrait], pZoom);

	base_radius *= 4;

	float targetHeight = base_radius * sin(Math::degToRad * base_pitch);
	float targetNear = base_radius * cos(Math::degToRad * base_pitch);

	Transform cameraTransform;

	// Set track origin
	track->trackOrigin = Transform();
	track->trackOrigin *= Transform::Rotation({ 0.0f, -m_roll * 360.0f,0.0f });
	track->trackOrigin *= Transform::Translation(Vector3(0.0f, -targetHeight, -targetNear));

	// Calculate clipping distances
	Vector3 cameraPos = cameraTransform.TransformDirection(-Vector3(cameraTransform.GetPosition()));
	Vector3 cameraDir = cameraTransform.TransformDirection(Vector3(0.0f, 0.0f, 1.0f));
	float offset = VectorMath::Dot(cameraPos, cameraDir);

	Vector3 toTrackEnd = Vector3(0, 0, 0) - Vector3(0.0f, targetNear + track->trackLength, -targetHeight);
	float distToTrackEnd = sqrtf(toTrackEnd.x * toTrackEnd.x + toTrackEnd.y * toTrackEnd.y + toTrackEnd.z * toTrackEnd.z);
	float angleToTrackEnd = atan2f(toTrackEnd.y, toTrackEnd.z);

	cameraTransform *= Transform::Rotation(Vector3(base_pitch - pitchOffset,
											0.0f,
											0.0f ) + m_shakeOffset);

	m_pitch = base_pitch - pitchOffset + m_shakeOffset.x;

	float d0 = VectorMath::Dot(Vector3(0.0f, 0.0f, 0.0f), cameraDir) + offset;
	float d1 = fabsf(cosf(angleToTrackEnd - (Math::degToRad * m_pitch)) * distToTrackEnd);
	rs.cameraTransform = cameraTransform;

	rs.projectionTransform = ProjectionMatrix::CreatePerspective(fov, g_aspectRatio, 0.1f, d1 + viewRangeExtension);

	m_rsLast = rs;

	return rs;
#else
	const float ROLL_AMT = 8;
	const float PITCH_AMT = 10;
	const float ZOOM_POW = 1.65f;
	const float LENGTH_BASE = 12;
	const float ROT = 0;

	auto GetOriginTransform = [&](float pitch, float roll)
	{
		auto origin = Transform::Rotation({ 0, 0, 0 });
		auto anchor = Transform::Rotation({ 1.5f, 0, 0 })
			* Transform::Translation({ 0, -0.825f, 0 });
		auto contnr = Transform::Scale({ 1, 1, 1 })
			* Transform::Rotation({ -90, 0, 0, })
			* Transform::Translation({ 0, 0, -1.1f });

		origin = Transform::Rotation({ 0, 0, roll });
		anchor = Transform::Translation({ 0, -1.0f, 0 })
			* Transform::Rotation({ 1.5f, 0, 0 });
		contnr = Transform::Translation({ 0, 0, -0.9f })
			* Transform::Rotation({ -90 + pitch, 0, 0, });

		return origin * anchor * contnr;
		return contnr * anchor * origin;
	};

	// Extension of clipping planes in outward direction
	float viewRangeExtension = clipped ? 0.0f : 5.0f;

	RenderState rs = g_application->GetRenderStateBase();

	auto worldNormal = GetOriginTransform(pPitch * PITCH_AMT, m_roll * 360.0f);
	auto worldNoRoll = GetOriginTransform(pPitch * PITCH_AMT, 0);

	auto zoomDir = worldNormal.GetPosition();
	float highwayDist = zoomDir.Length();
	zoomDir = zoomDir.Normalized();

	float zoomAmt;
	if (pZoom <= 0) zoomAmt = pow(ZOOM_POW, -pZoom) - 1;
	else zoomAmt = highwayDist * (pow(ZOOM_POW, -pow(pZoom, 1.35f)) - 1);

	//m_calcZoomBottom = zoomAmt / highwayDist + 1;

	track->trackOrigin = Transform::Translation(zoomDir * zoomAmt) * worldNormal;

	auto critDir = worldNoRoll.GetPosition().Normalized();
	float rotToCrit = -atan2(critDir.y, -critDir.z) * Math::radToDeg;

	float fov = fovs[0];
	float cameraRot = fov / 2 - fov * pitchOffsets[0];

	m_pitch = rotToCrit - cameraRot + basePitch[0];
	auto cameraTransform = Transform::Rotation(Vector3(m_pitch, 0, 0) + m_shakeOffset);

	// Calculate clipping distances
	Vector3 toTrackEnd = worldNormal.TransformPoint(Vector3(0.0f, track->trackLength, 0));
	float distToTrackEnd = sqrtf(toTrackEnd.x * toTrackEnd.x + toTrackEnd.y * toTrackEnd.y + toTrackEnd.z * toTrackEnd.z);

	rs.cameraTransform = cameraTransform;
	rs.projectionTransform = ProjectionMatrix::CreatePerspective(fov, g_aspectRatio, 0.1f, distToTrackEnd + viewRangeExtension);

	m_rsLast = rs;
	return rs;
#endif
}

void Camera::SetTargetRoll(float target)
{
	float actualTarget = target * m_rollIntensity;
	if(!rollKeep)
	{
		m_targetRoll = actualTarget;
		m_targetRollSet = true;
	}
	else
	{
		if (m_targetRoll == 0.0f || Math::Sign(m_targetRoll) == Math::Sign(actualTarget))
		{
			if (m_targetRoll == 0)
				m_targetRoll = actualTarget;
			if (m_targetRoll < 0 && actualTarget < m_targetRoll)
				m_targetRoll = actualTarget;
			else if (actualTarget > m_targetRoll)
				m_targetRoll = actualTarget;
		}
		m_targetRollSet = true;
	}
}

void Camera::SetSpin(float direction, uint32 duration, uint8 type, class BeatmapPlayback& playback)
{
	const TimingPoint& currentTimingPoint = playback.GetCurrentTimingPoint();

	m_spinDirection = direction;
	m_spinDuration = (duration / 192.0f) * (currentTimingPoint.beatDuration) * 4;
	m_spinStart = playback.GetLastTime();
	m_spinType = type;
}

void Camera::SetLasersActive(bool lasersActive)
{
	m_lasersActive = lasersActive;
}

float Camera::GetRoll() const
{
	return m_roll;
}

float Camera::GetHorizonHeight()
{
	return (0.5 + ((-90.f - m_pitch) / fovs[g_aspectRatio > 1.0f ? 0 : 1])) * m_rsLast.viewportSize.y;
}

Vector2i Camera::GetScreenCenter()
{
	Vector2i ret = Vector2i(0, GetHorizonHeight());

	uint8 portrait = g_aspectRatio > 1.0f ? 0 : 1;
	float fov = fovs[portrait];

	ret.x = m_rsLast.viewportSize.x / 2;
	ret.x -= (m_shakeOffset.y / (fov * g_aspectRatio)) * m_rsLast.viewportSize.x;

	return ret;
}

Vector3 Camera::GetShakeOffset()
{
	return m_shakeOffset;
}

float Camera::m_ClampRoll(float in) const
{
	float ain = fabs(in);
	if(ain < 1.0f)
		return in;
	bool odd = ((uint32)fabs(in) % 2) == 1;
	float sign = Math::Sign(in);
	if(odd)
	{
		// Swap sign and modulo
		return -sign * (1.0f-fmodf(ain, 1.0f));
	}
	else
	{
		// Keep sign and modulo
		return sign * fmodf(ain, 1.0f);
	}
}

CameraShake::CameraShake(float duration) : duration(duration)
{
	time = duration;
}
CameraShake::CameraShake(float duration, float amplitude) : duration(duration), amplitude(amplitude)
{
	time = duration;
}

