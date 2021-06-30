#include "stdafx.h"
#include "Application.hpp"
#include "FastGUI/FastGuiGame.hpp"
#include "Game.hpp"
#include "Track.hpp"
#include "Graphics/MeshGenerators.hpp"
#include "Gauge.hpp"
#include "Shared/Interpolation.hpp"
#include "GameConfig.hpp"

using Interpolation::Lerp;
using Interpolation::Predefined;

void FastGuiGame::Render(float deltaTime)
{
	RenderQueue rq(g_gl, g_application->GetRenderStateBase());

	//Song info
	{
		Transform t;
		if (m_jacket)
		{
			Transform jacketTransform;
			jacketTransform *= Transform::Translation({ 5, 5, 0 });
			jacketTransform *= Transform::Scale({ m_jacketWidth, m_jacketWidth, 0 });
			rq.Draw(jacketTransform, m_guiMesh, g_application->GetGuiTexMaterial(), m_jacketParams);
		}

		rq.Draw(t * Transform::Translation({ m_jacketWidth + 10, 5, 0 }), m_title, m_fontMat);
		rq.Draw(t * Transform::Translation({ m_jacketWidth + 10, 5 + m_title->size.y, 0 }), m_artist, m_fontMat);
	}

	// TODO: gauge % display, multiplayer(? probably not)


	//gauge
	{
		Transform gaugeTransform;
		MaterialParameterSet gaugeBorderParams;
		gaugeBorderParams.SetParameter("color", Color::White);

		float width = 30 * m_scale;
		float height = GAUGE_ASPECT * width;

		gaugeTransform *= Transform::Translation({ g_resolution.x * 0.95f - width, m_critLine * 0.80f * g_resolution.y - height, 0 });
		gaugeTransform *= Transform::Scale({ width, width, 0 });
		rq.Draw(gaugeTransform, m_gaugeBorderMesh, m_fillMat, gaugeBorderParams);

		gaugeBorderParams.SetParameter("color", Color::Black.WithAlpha(3.0f / 4.0f));
		rq.Draw(gaugeTransform * Transform::Scale({ 1, GAUGE_ASPECT, 1 }), m_guiMesh, m_fillMat, gaugeBorderParams);

		gaugeBorderParams.SetParameter("color", Color(Lerp<Vector4>(m_clearColors[0], m_clearColors[1], m_clearTransition, Predefined::EaseOutQuad)));

		float gaugeDisplay = Lerp(m_gauge[0], m_gauge[1], m_gaugeTransition, Predefined::EaseOutCubic);
		gaugeTransform *= Transform::Translation({ 0, GAUGE_ASPECT - gaugeDisplay * GAUGE_ASPECT, 0 });
		gaugeTransform *= Transform::Scale({ 1, gaugeDisplay * GAUGE_ASPECT, 1 });
		rq.Draw(gaugeTransform, m_guiMesh, m_fillMat, gaugeBorderParams);
	}

	if (m_combo)
	{
		Transform t;
		MaterialParameterSet p;
		p.SetParameter("color", Color::White);

		float easedScale = Lerp(1.3f, 1.0f, m_comboScale, Predefined::EaseOutCubic);

		t *= Transform::Translation({
			static_cast<float>(g_resolution.x - m_comboText->size.x * easedScale) / 2.0f,
			m_critLine * g_resolution.y - (m_comboText->size.y * 0.5f * easedScale) - (m_comboText->size.y * 1.5f),
			0.0f
			});
		t *= Transform::Scale(Vector3(easedScale));
		rq.Draw(t, m_comboText, m_fontMat, p);
	}


	//Cursors
	for (size_t i = 0; i < 2; i++)
	{
		Transform t;
		MaterialParameterSet p;

		t *= Transform::Translation({ m_critPos.x, m_critPos.y, 0.f });
		t *= Transform::Rotation({ 0.0f, 0.0f, -Math::radToDeg * m_critLineAngle });
		t *= Transform::Translation({ m_laserPos[i], 0, 0 });

		p.SetParameter("color", Color::Black.WithAlpha(m_laserAlpha[i] * 0.5));
		t *= Transform::Scale(Vector3(75.0f * m_scale));
		rq.Draw(t, m_cursorMesh, m_fillMat, p);

		p.SetParameter("color", m_laserColors[i].WithAlpha(m_laserAlpha[i]));
		t *= Transform::Scale(Vector3(0.75f));
		rq.Draw(t, m_cursorMesh, m_fillMat, p);
	}


	//alerts
	for (size_t i = 0; i < 2; i++)
	{
		Transform t;
		MaterialParameterSet p;
		Color c = m_laserColors[i];
		float w = 150.0f;




		float h = m_laserAlerts[i] - 0.5f;
		h = fmaf(h, -h, 0.25f);
		h = fmin(h * 20.0f, 1.0f);

		p.SetParameter("color", c);

		t *= Transform::Translation({
			g_resolution.x / 2.0f + g_resolution.x * 0.3f * (i == 0 ? -1 : 1) - (w * m_scale) / 2.0f,
			m_critLine * 0.80f * g_resolution.y,
			0.0f });

		Transform textTransform(t);

		t *= Transform::Scale({ m_scale * w, m_scale * w * h, 1.0f });
		t *= Transform::Translation({ 0.0f, -0.5f * h, 0.0f });



		rq.Draw(t, m_guiMesh, m_fillMat, p);


		p.SetParameter("color", Color::White);
		textTransform *= Transform::Translation({
			m_scale * w * 0.5f - m_alertText[i]->size.x * 0.5f,
			-m_alertText[i]->size.y * 0.5f * h,
			0.0f });

		textTransform *= Transform::Scale({ 1.0f, h, 1.0f });
		rq.Draw(textTransform, m_alertText[i], m_fontMat, p);
	}

	//Score
	{
		float width = m_scoreText[0]->size.x + m_scoreText[1]->size.x;
		float height = m_scoreText[0]->size.y;

		Transform t;
		MaterialParameterSet p;
		p.SetParameter("color", Color::White);

		t *= Transform::Translation({ g_resolution.x - width - 20.0f, 5.0f + m_scoreText[0]->size.z, 0 });
		Transform t2(t);

		t *= Transform::Translation({ 0, -m_scoreText[0]->size.z, 0 });
		rq.Draw(t, m_scoreText[0], m_fontMat, p);

		t2 *= Transform::Translation({ m_scoreText[0]->size.x + 5.0f * m_scale,-m_scoreText[1]->size.z, 0 });
		p.SetParameter("color", Color::White.WithAlpha(0.75f));
		rq.Draw(t2, m_scoreText[1], m_fontMat, p);
	}

	rq.Process();
}

void FastGuiGame::Update(float deltaTime, float critHeight, Vector2 leftCrit, Vector2 rightCrit, Vector2 critCenter, Gauge* gauge)
{
	m_comboScale = Math::Min(m_comboScale + deltaTime * 6.0f, 1.0f);
	m_critPos = critCenter;
	m_critEdge[0] = leftCrit;
	m_critEdge[1] = rightCrit;

	Vector2 line = rightCrit - leftCrit;
	m_critLineAngle = -atan2f(line.y, line.x);

	m_critLine = critHeight;
	float newGaugeValue = gauge->GetValue();
	if (abs(m_gauge[1] - newGaugeValue) > std::numeric_limits<float>::epsilon()) {
		m_gauge[0] = Lerp(m_gauge[0], m_gauge[1], m_gaugeTransition, Predefined::EaseOutCubic);
		m_gauge[1] = newGaugeValue;
		m_gaugeTransition = 0.0f;
	}

	bool useClearColor = false;
	if (gauge->GetType() == GaugeType::Normal) {
		useClearColor = gauge->GetValue() >= 0.7f;
	}
	else {
		useClearColor = gauge->GetValue() >= 0.3f;
	}

	if (useClearColor) {
		m_clearTransition += deltaTime / 0.3f;
	}
	else {
		m_clearTransition -= deltaTime / 0.3f;
	}

	m_clearTransition = Math::Clamp(m_clearTransition, 0.0f, 1.0f);

	m_gaugeTransition += deltaTime / 0.2f;
	m_gaugeTransition = Math::Min(1.0f, m_gaugeTransition);


	GaugeType newType = gauge->GetType();
	if (newType != m_currentGaugeType)
	{
		SetGaugeColor(newType);
	}

	for (size_t i = 0; i < 2; i++)
	{
		m_laserAlerts[i] -= deltaTime / 1.5f;
		m_laserAlerts[i] = Math::Max(0.0f, m_laserAlerts[i]);
	}

}

void FastGuiGame::OnComboChanged(uint32 combo)
{
	if (m_font && combo != m_combo)
	{
		if (combo > m_combo)
		{
			m_comboScale = 0.0f;
		}
		m_combo = combo;
		m_comboText = m_font->CreateText(Utility::WSprintf(L"%d", combo), 60 * m_scale);
	}
}

void FastGuiGame::OnLaserAlert(uint8 index)
{
	m_laserAlerts[index] = 1.0f;
}

void FastGuiGame::SetLaserCursor(uint32 index, float pos, float alpha)
{
	m_laserAlpha[index] = alpha;
	m_laserPos[index] = pos;
}

void FastGuiGame::UpdateScore(uint32 score)
{
	if (m_font)
	{
		m_scoreText[0] = m_font->CreateText(Utility::WSprintf(L"%04d", score / 10000), 60 * m_scale, FontRes::TextOptions::Monospace);
		m_scoreText[1] = m_font->CreateText(Utility::WSprintf(L"%04d", score % 10000), 50 * m_scale, FontRes::TextOptions::Monospace);
	}
}

bool FastGuiGame::SetGaugeColor(GaugeType type)
{
	switch (type)
	{
	case GaugeType::Normal: {
		m_clearColors[0] = Color::FromHSV(180.0f, .8f, .8f);
		m_clearColors[1] = Color::FromHSV(340.0f, 0.8f, 1.0f);
		break;
	}
	case GaugeType::Hard: {
		m_clearColors[0] = Color::FromHSV(20.0f, .8f, 0.7f);
		m_clearColors[1] = Color::FromHSV(20.0f, .9f, 1.0f);
		break;
	}
	case GaugeType::Permissive: {
		m_clearColors[0] = Color::FromHSV(31.0f, .91f, 0.76f);
		m_clearColors[1] = Color::FromHSV(31.0f, .87f, 1.0f);
		break;
	}
	case GaugeType::Blastive: {
		m_clearColors[0] = Color::FromHSV(168.0f, .6f, .5f);
		m_clearColors[1] = Color::FromHSV(168.0f, .65f, .69f);
		break;
	}

	default:
		Logf("Unknown GaugeType %d", Logger::Severity::Error, static_cast<int>(type));
		assert(false);
		return false;
		break;
	}

	m_currentGaugeType = type;
	return true;
}

bool FastGuiGame::Init(Game* game)
{
	//calculate scale
	{
		float designWidth = g_aspectRatio >= 1.0f ? 1920.f : 1080.f;
		m_scale = g_resolution.x / designWidth;
	}



	const auto& settings = game->GetBeatmap()->GetMapSettings();
	const auto jacketPath = Path::Absolute(game->GetChartRootPath() + Path::sep + settings.jacketPath);
	if (Path::FileExists(jacketPath))
	{
		m_jacketImage = ImageRes::Create(jacketPath);
		if (m_jacketImage)
		{
			m_jacket = TextureRes::Create(g_gl, m_jacketImage);
			m_jacketParams.SetParameter("color", Color::White);
			m_jacketParams.SetParameter("mainTex", m_jacket);
			if (m_jacket)
			{
				m_jacketWidth = 200.0f * m_scale;
			}
		}
	}

	String fontpath = Path::Normalize(Path::Absolute("fonts/settings/NotoSans-Regular.ttf"));
	m_font = g_application->LoadFont(fontpath, true);
	if (!m_font)
		return false;

	UpdateScore(0);
	//TODO: Make dynamic scale
	m_title = m_font->CreateText(Utility::ConvertToWString(settings.title), 50 * m_scale);
	m_artist = m_font->CreateText(Utility::ConvertToWString(settings.artist), 40 * m_scale);

	m_comboText = m_font->CreateText(L"0", 12);
	m_guiMesh = MeshGenerators::Quad(g_gl, { 0,0 });

	//Generate gauge border
	{
		m_gaugeBorderMesh = MeshRes::Create(g_gl);
		m_gaugeBorderMesh->SetPrimitiveType(Graphics::PrimitiveType::TriangleStrip);
		const Vector<MeshGenerators::SimpleVertex> verts = {
			{{0.0f, 0.0f, 0.0f}, {0.0f,0.0f}},
			{{-GAUGE_BORDER, -GAUGE_BORDER, 0.0f}, {0.f,0.f}},
			{{0.0f, GAUGE_ASPECT, 0.0f}, {0.f,0.f}},
			{{-GAUGE_BORDER, GAUGE_ASPECT + GAUGE_BORDER, 0.0f}, {0.f,0.f}},
			{{1.0f, GAUGE_ASPECT, 0.0f}, {0.f,0.f}},
			{{1.0f + GAUGE_BORDER, GAUGE_ASPECT + GAUGE_BORDER, 0.0}, {0.f,0.f}},
			{{1.0f, 0.0f, 0.0f}, {0,0}},
			{{1.0f + GAUGE_BORDER, -GAUGE_BORDER, 0.0f}, {0.f,0.f}},
			{{0.0f, 0.0f, 0.0f}, {0,0}},
			{{-GAUGE_BORDER, -GAUGE_BORDER, 0.0f}, {0.f,0.f}}
		};
		m_gaugeBorderMesh->SetData(verts);
	}

	m_fontMat = g_application->GetFontMaterial();
	m_fillMat = g_application->GetGuiFillMaterial();


	//Cursor mesh
	{
		m_cursorMesh = MeshRes::Create(g_gl);
		m_cursorMesh->SetPrimitiveType(PrimitiveType::TriangleList);
		const Vector<MeshGenerators::SimpleVertex> verts = {
			{{-0.5f, 0.5f, 0.0f}, {0.f,0.f}},
			{{0.0f, -0.5f, 0.0f}, {0.f,0.f}},
			{{0.5f, 0.5f, 0.0f}, {0.f,0.f}}
		};

		m_cursorMesh->SetData(verts);
	}


	m_laserColors[0] = Color::FromHSV(g_gameConfig.GetFloat(GameConfigKeys::Laser0Color), 0.75f, 0.75f);
	m_laserColors[1] = Color::FromHSV(g_gameConfig.GetFloat(GameConfigKeys::Laser1Color), 0.75f, 0.75f);



	m_alertText[0] = m_font->CreateText(L"L", 60 * m_scale);
	m_alertText[1] = m_font->CreateText(L"R", 60 * m_scale);

	return SetGaugeColor(game->GetScoring().GetTopGauge()->GetType());
}
