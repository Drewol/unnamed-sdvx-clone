#include "stdafx.h"
#include "CalibrationScreen.hpp"

#include "Application.hpp"
#include "Audio/Audio.hpp"
#include "GameConfig.hpp"
#include "SettingsScreen.hpp"

CalibrationScreen::CalibrationScreen(nk_context* nk_ctx)
{
	m_ctx = nk_ctx;
}

CalibrationScreen::~CalibrationScreen()
{
	g_input.OnButtonPressed.RemoveAll(this);
	g_input.OnButtonReleased.RemoveAll(this);
}

bool CalibrationScreen::AsyncLoad()
{
	m_metronome = SampleRes::Create(g_audio, Path::Normalize(Path::Absolute("audio/metronome120.wav")));
	m_playback.MakeCalibrationPlayback();
	m_audioOffset = g_gameConfig.GetInt(GameConfigKeys::GlobalOffset);
	m_inputOffset = g_gameConfig.GetInt(GameConfigKeys::InputOffset);
	m_trackCover = g_gameConfig.GetBool(GameConfigKeys::ShowCover);
	m_fpsTarget = g_gameConfig.GetInt(GameConfigKeys::FPSTarget);
	m_bounceGuard = g_gameConfig.GetInt(GameConfigKeys::InputBounceGuard);
	if (g_gameConfig.GetEnum<Enum_SpeedMods>(GameConfigKeys::SpeedMod) != SpeedMods::XMod)
	{
		m_hispeed = g_gameConfig.GetFloat(GameConfigKeys::ModSpeed) / 120.0;
	}
	else
	{
		m_hispeed = g_gameConfig.GetFloat(GameConfigKeys::HiSpeed);
	}

	for (size_t i = 0; i < 4; i++)
	{
		m_buttonGuardTime[i] = 0;
	}

	return m_track.AsyncLoad();
}

bool CalibrationScreen::AsyncFinalize()
{
	m_track.suddenCutoff = g_gameConfig.GetFloat(GameConfigKeys::SuddenCutoff);
	m_track.suddenFadewindow = g_gameConfig.GetFloat(GameConfigKeys::SuddenFade);
	m_track.hiddenCutoff = g_gameConfig.GetFloat(GameConfigKeys::HiddenCutoff);
	m_track.hiddenFadewindow = g_gameConfig.GetFloat(GameConfigKeys::HiddenFade);
	m_track.distantButtonScale = g_gameConfig.GetFloat(GameConfigKeys::DistantButtonScale);
	m_timer.Restart();
	m_metronome->Play(true);
	m_camera.track = &m_track;

	//reinitialize input to apply any changes to button bindings
	g_input.Cleanup();
	g_input.Init(*g_gameWindow);

	g_input.OnButtonPressed.Add(this, &CalibrationScreen::m_OnButtonPressed);
	g_input.OnButtonReleased.Add(this, &CalibrationScreen::m_OnButtonReleased);

	return m_track.AsyncFinalize();
}

void CalibrationScreen::Render(float deltaTime)
{
	const int windowFlag = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE;
	m_track.SetViewRange(8.0f / m_hispeed);

	RenderState rs = m_camera.CreateRenderState(true);
	RenderQueue renderQueue(g_gl, rs);

	Vector<ObjectState*> currentObjectSet;
	m_playback.GetObjectsInViewRange(m_track.GetViewRange(), currentObjectSet);

	m_track.DrawBase(renderQueue);
	std::unordered_set<MapTime> chipFXTimes[2];

	for (auto& object : currentObjectSet)
	{
		m_track.DrawObjectState(renderQueue, m_playback, object, false, chipFXTimes);
	}
	if (m_trackCover)
	{
		m_track.DrawTrackCover(renderQueue);
	}
	m_track.DrawHitEffects(renderQueue);
	m_track.DrawOverlays(renderQueue);
	m_track.DrawCalibrationCritLine(renderQueue);
	renderQueue.Process();

	//Draw nuklear GUI
	{
		const int OPTIONS_WINDOW_WIDTH = Math::Clamp(g_resolution.x - 50, 100, 400);
		const int OPTIONS_WINDOW_HEIGHT = Math::Clamp(g_resolution.y - 50, 200, 520);

		if (nk_begin(m_ctx, "Options", nk_rect(50, 50, OPTIONS_WINDOW_WIDTH, OPTIONS_WINDOW_HEIGHT), windowFlag))
		{
			nk_layout_row_dynamic(m_ctx, 30, 1);
			m_audioOffset = nk_propertyi(m_ctx, "Global Offset", -1000, m_audioOffset, 1000, 1, 1);
			m_inputOffset = nk_propertyi(m_ctx, "Input Offset", -1000, m_inputOffset, 1000, 1, 1);

			int boolValue = m_autoCalibrate ? 1 : 0;
			nk_checkbox_label(m_ctx, "Auto Calibrate Input offset", &boolValue);
			m_autoCalibrate = boolValue > 0;

			nk_label(m_ctx, *Utility::Sprintf("HiSpeed (%.2f x 120 = %.1f):", m_hispeed, m_hispeed * 120.0), nk_text_alignment::NK_TEXT_LEFT);
			nk_slider_float(m_ctx, 0.5, &m_hispeed, 10.0f, 0.01f);
			nk_label(m_ctx, *Utility::Sprintf("Distant Button Scale: %.2f", m_track.distantButtonScale), NK_TEXT_LEFT);
			nk_slider_float(m_ctx, 1.0f, &m_track.distantButtonScale, 5.0f, 0.01f);
			nk_layout_row_dynamic(m_ctx, 150, 2);
			if (nk_group_begin(m_ctx, "Hidden", NK_WINDOW_NO_SCROLLBAR))
			{
				nk_layout_row_dynamic(m_ctx, 30, 1);
				nk_label(m_ctx, "Hidden Cutoff:", nk_text_alignment::NK_TEXT_LEFT);
				nk_slider_float(m_ctx, 0.0f, &m_track.hiddenCutoff, 1.0f, 0.005f);
				nk_label(m_ctx, "Hidden Fade:", nk_text_alignment::NK_TEXT_LEFT);
				nk_slider_float(m_ctx, 0.0f, &m_track.hiddenFadewindow, 1.0f, 0.005f);
				nk_group_end(m_ctx);
			}
			if (nk_group_begin(m_ctx, "Sudden", NK_WINDOW_NO_SCROLLBAR))
			{
				nk_layout_row_dynamic(m_ctx, 30, 1);
				nk_label(m_ctx, "Sudden Cutoff:", nk_text_alignment::NK_TEXT_LEFT);
				nk_slider_float(m_ctx, 0.0f, &m_track.suddenCutoff, 1.0f, 0.005f);
				nk_label(m_ctx, "Sudden Fade:", nk_text_alignment::NK_TEXT_LEFT);
				nk_slider_float(m_ctx, 0.0f, &m_track.suddenFadewindow, 1.0f, 0.005f);
				nk_group_end(m_ctx);
			}
			nk_layout_row_dynamic(m_ctx, 30, 1);
			boolValue = m_trackCover ? 1 : 0;
			nk_checkbox_label(m_ctx, "Show Track Cover", &boolValue);
			m_trackCover = boolValue > 0;


			nk_layout_row_dynamic(m_ctx, 30, 2);
			if (nk_button_label(m_ctx, "Cancel")) {
				g_application->RemoveTickable(this);
			}
			if (nk_button_label(m_ctx, "Ok")) {
				//Save settings
				g_gameConfig.Set(GameConfigKeys::HiSpeed, m_hispeed);
				g_gameConfig.Set(GameConfigKeys::ModSpeed, m_hispeed * 120.0f);
				g_gameConfig.Set(GameConfigKeys::HiddenCutoff, m_track.hiddenCutoff);
				g_gameConfig.Set(GameConfigKeys::HiddenFade, m_track.hiddenFadewindow);
				g_gameConfig.Set(GameConfigKeys::SuddenCutoff, m_track.suddenCutoff);
				g_gameConfig.Set(GameConfigKeys::SuddenFade, m_track.suddenFadewindow);
				g_gameConfig.Set(GameConfigKeys::InputOffset, m_inputOffset);
				g_gameConfig.Set(GameConfigKeys::GlobalOffset, m_audioOffset);
				g_gameConfig.Set(GameConfigKeys::ShowCover, m_trackCover);
				g_gameConfig.Set(GameConfigKeys::DistantButtonScale, m_track.distantButtonScale);
				g_application->RemoveTickable(this);
			}
		}
		nk_end(m_ctx);

		if (nk_begin(m_ctx, "Hit Deltas", nk_rect(g_resolution.x - 450, 50, 400, 300), windowFlag))
		{
			int count = 0;
			double sum = 0.0;
			double avg = 0.0;
			for (auto v = m_hitDeltas.rbegin(); v != m_hitDeltas.rend(); v++) {
				sum += *v;
				if (count++ >= 50)
					break;
			}
			if (count > 0) {
				avg = sum / (double)count;
			}


			nk_layout_row_dynamic(m_ctx, 30, 2);
			nk_label(m_ctx, *Utility::Sprintf("Average: %0.1fms", avg), NK_TEXT_LEFT);
			if (nk_button_label(m_ctx, "Reset")) {
				m_metronome->Play(true);
				m_timer.Restart();
				m_hitDeltas.clear();
				m_zeroOffsetDeltas.clear();
				m_hitcount = 0;
			}
			nk_layout_row_dynamic(m_ctx, 20, 1);
			for (auto it = m_hitDeltas.rbegin(); it != m_hitDeltas.rend(); ++it)
			{
				double hue = 120.0 - ((double)abs(*it) / 50.0) * 60.0;
				auto c = Color::FromHSV(fmax(hue, 0.0) , 1.0, 1.0).ToRGBA8();
				nk_label_colored(m_ctx, *Utility::Sprintf("%d", *it), NK_TEXT_RIGHT, nk_color{ c.x, c.y, c.z, 255 });
			}
		}
		nk_end(m_ctx);

		if (nk_begin(m_ctx, "Guide", nk_rect(g_resolution.x / 2.0 - 300, 0, 600, 300), NK_WINDOW_BORDER | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
		{
			nk_layout_set_min_row_height(m_ctx, 20);
			nk_layout_row_dynamic(m_ctx, 0, 1);
			nk_label(m_ctx, "First adjust your global offset until the bottom of the", NK_LEFT);
			nk_label(m_ctx, "notes hit the critical line at the exact time of the audio tick.", NK_LEFT);
			nk_label(m_ctx, "", NK_LEFT);
			nk_label(m_ctx, "Once you have set your global offset it's time", NK_TEXT_LEFT);
			nk_label(m_ctx, "to adjust your input offset, either use the automatic", NK_TEXT_LEFT);
			nk_label(m_ctx, "calibration or do it manually. If you are doing it manually", NK_TEXT_LEFT);
			nk_label(m_ctx, "you want to subtract the average delta you are hitting from", NK_TEXT_LEFT);
			nk_label(m_ctx, "your current input offset, for the automatic calibration ", NK_TEXT_LEFT);
			nk_label(m_ctx, "keep hitting notes until your average hits very close to 0.", NK_TEXT_LEFT);

		}
		nk_end(m_ctx);

		if (!m_hasRenderedOnce) {
			nk_window_collapse(m_ctx, "Guide", NK_MINIMIZED);
			m_hasRenderedOnce = true;
		}

		nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
	}
}

void CalibrationScreen::Tick(float deltaTime)
{
	m_lastTime = 2000 + (m_timer.Milliseconds() % 2000);
	m_lastTime -= m_audioOffset;


	m_playback.Update(m_lastTime);
	m_camera.Tick(deltaTime, m_playback);
	m_track.Tick(m_playback, deltaTime);
}

bool CalibrationScreen::GetTickRate(int32& rate)
{
	rate = m_fpsTarget;
	return true;
}

void CalibrationScreen::m_OnButtonPressed(Input::Button buttonCode, int32 delta)
{
	if (buttonCode == Input::Button::Back) {
		g_application->RemoveTickable(this);
		return;
	}
	if (buttonCode < Input::Button::FX_0)
	{
		int32 guardDelta = m_timer.Milliseconds() - m_buttonGuardTime[(uint32)buttonCode];
		if (guardDelta < m_bounceGuard && guardDelta >= 0)
		{
			return;
		}
		m_buttonGuardTime[(uint32)buttonCode] = m_timer.Milliseconds();
		int correctedTime = m_lastTime - delta;
		int hitDelta = correctedTime % 500 > 250 ? (correctedTime % 500) - 500 : correctedTime % 500;
		int zod = hitDelta;
		m_zeroOffsetDeltas.Add(zod);
		hitDelta += m_inputOffset;
		m_hitDeltas.Add(hitDelta);
		m_hitcount++;

		if (m_autoCalibrate)
		{
			m_inputOffset = -m_average(m_zeroOffsetDeltas);
		}

		// TODO: use HitWindow based on current setting
		HitWindow hitWindow = HitWindow::NORMAL;

		if (hitDelta <= hitWindow.perfect) {
			m_track.AddHitEffect((int)buttonCode, m_track.hitColors[2]);
		}
		else if (hitDelta <= hitWindow.good) {
			m_track.AddHitEffect((int)buttonCode, m_track.hitColors[1]);
		}
		else {
			m_track.AddHitEffect((int)buttonCode, m_track.hitColors[3]);
		}

	}
}

void CalibrationScreen::m_OnButtonReleased(Input::Button buttonCode, int32 delta)
{
	if (buttonCode < Input::Button::FX_0)
	{
		int32 guardDelta = m_timer.Milliseconds() - m_buttonGuardTime[(uint32)buttonCode];
		if (guardDelta < m_bounceGuard && guardDelta >= 0)
		{
			return;
		}
		m_buttonGuardTime[(uint32)buttonCode] = m_timer.Milliseconds();
	}
}

float CalibrationScreen::m_average(const Vector<int>& values)
{
	int sum = 0;
	float average = 0;
	for (auto v : values)
	{
		sum += v;
	}

	if (m_hitcount > 0) {
		average = (double)sum / (double)m_hitcount;
	}

	return average;
}
