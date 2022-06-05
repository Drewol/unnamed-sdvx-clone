#pragma once
#include "Shared/Timer.hpp"
#include "ApplicationTickable.hpp"
#include "AsyncLoadable.hpp"
#include "Track.hpp"
#include "Camera.hpp"
#include "Audio/Sample.hpp"
#include "Beatmap/BeatmapPlayback.hpp"

class CalibrationScreen : public IAsyncLoadableApplicationTickable
{
public:
	CalibrationScreen(struct nk_context* nk_ctx);
	~CalibrationScreen();
	bool AsyncLoad() override;
	bool AsyncFinalize() override;
	void Render(float deltaTime) override;
	void Tick(float deltaTime) override;
	bool GetTickRate(int32& rate) override;

private:
	Timer m_timer;
	Track m_track;
	Camera m_camera;
	Sample m_metronome;
	BeatmapPlayback m_playback;
	float m_hispeed = 2.0;
	int m_audioOffset = 0;
	int m_inputOffset = 0;
	MapTime m_lastTime = 0;
	bool m_trackCover = false;
	Vector<int> m_hitDeltas;
	Vector<int> m_zeroOffsetDeltas;
	struct nk_context* m_ctx;
	MapTime m_buttonGuardTime[4] = { 0, 0, 0, 0 };
	MapTime m_bounceGuard = 10;
	int32 m_fpsTarget = 0;
	int32 m_hitcount = 0;
	bool m_autoCalibrate = false;
	bool m_hasRenderedOnce = false;

	void m_OnButtonPressed(Input::Button buttonCode, int32 delta);
	void m_OnButtonReleased(Input::Button buttonCode, int32 delta);
	float m_average(const Vector<int>& values);
};
