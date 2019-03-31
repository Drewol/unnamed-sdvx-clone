#pragma once

/*
	Camera shake effect 
*/
struct CameraShake
{
	CameraShake() = default;
	CameraShake(float duration);
	CameraShake(float duration, float amplitude);
	float amplitude;
	float duration;
	float time = 0.0f;
};

static const float KSM_PITCH_UNIT_PRE_168 = 7.0f;
static const float KSM_PITCH_UNIT_POST_168 = 180.0f / 12;

/*
	Camera that hovers above the playfield track and can process camera shake and tilt effects
*/
class Camera
{
public:
	Camera();
	~Camera();

	// Updates the camera's shake effects, movement, etc.
	void Tick(float deltaTime, class BeatmapPlayback& playback);

	void AddCameraShake(CameraShake camerShake);
	void AddRollImpulse(float dir, float strength);

	// Changes the amount of roll applied when lasers are controlled, default = 1
	void SetRollIntensity(float val);
	void SetLasersActive(bool lasersActive);
	void SetTargetRoll(float target);
	void SetSpin(float direction, uint32 duration, uint8 type, class BeatmapPlayback& playback);
	void SetXOffsetBounce(float direction, uint32 duration, uint32 amplitude, uint32 frequency, float decay, class BeatmapPlayback &playback);
	float GetRoll() const;
	float GetLaserRoll() const;
	float GetActualRoll() const;
	float GetHorizonHeight();
	Vector2i GetScreenCenter();
	Vector3 GetShakeOffset();

	// Gets the spin angle for the background shader
	float GetBackgroundSpin() const { return m_bgSpin; }

	Vector2 Project(const Vector3& pos);

	// Generates a new render state for drawing from this cameras Point of View
	// the clipped boolean indicates whenether to clip the cameras clipping planes to the track range
	RenderState CreateRenderState(bool clipped);

	// The track being watched
	class Track* track;

	bool rollKeep = false;

	// Zoom values, both can range from -1 to 1 to control the track zoom
	float pLaneOffset = 0.0f;
	float pLaneZoom = 0.0f;
	float pLanePitch = 0.0f;
	float pLaneTilt = 0.0f;
	bool pManualTiltEnabled = false;

	float pitchUnit = KSM_PITCH_UNIT_POST_168;

	float cameraShakeX = 0.0f;
	float cameraShakeY = 0.4f;
	float cameraShakeZ = 0.0f;

	// Camera variables Landscape, Portrait
	float basePitch[2] = { 0.f, 0.f };
	float baseRadius[2] = { 0.3f, 0.275f };

	float pitchOffsets[2] = { 0.05f, 0.25f }; // how far from the bottom of the screen should the crit line be
	float fovs[2] = { 60.f, 90.0f };

	Transform worldNormal;
	Transform worldNoRoll;
	Transform critOrigin;

private:
	float m_ClampRoll(float in) const;
	// x offset
	float m_totalOffset = 0.0f;
	float m_spinBounceOffset = 0.0f;
	// roll value
	float m_totalRoll = 0.0f;
	float m_laserRoll = 0.0f;
	float m_actualRoll = 0.0f;
	// Target to roll towards
	float m_targetLaserRoll = 0.0f;
	bool m_targetRollSet = false;
	bool m_lasersActive = false;
	// Roll force
	float m_rollVelocity = 0.0f;
	float m_rollIntensity;

	// Spin variables
	int32 m_spinDuration = 1;
	int32 m_spinStart = 0;
	uint8 m_spinType;
	float m_spinDirection = 0.0f;
	float m_spinRoll = 0.0f;
	float m_spinProgress = 0.0f;
	float m_bgSpin = 0.0f;

	float m_spinBounceAmplitude = 0.0f;
	float m_spinBounceFrequency = 0.0f;
	float m_spinBounceDecay = 0.0f;

	float m_actualCameraPitch = 0.0f;

	RenderState m_rsLast;

	CameraShake m_shakeEffect;
	// Base position with shake effects applied after a frame
	Vector3 m_shakeOffset;
};