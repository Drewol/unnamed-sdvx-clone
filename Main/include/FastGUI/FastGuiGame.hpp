#pragma once
#include "Graphics/Texture.hpp"
#include "Graphics/Font.hpp"
#include "Shared/Shared.hpp"

class FastGuiGame {
public:
	bool Init(class Game* game);
	void Render(float deltaTime);
	void Update(float deltaTime, float critHeight, Vector2 leftCrit, Vector2 rightCrit, Vector2 critCenter, class Gauge* gauge);
	void OnComboChanged(uint32 combo);
	void OnLaserAlert(uint8 object);
	void SetLaserCursor(uint32 index, float pos, float alpha);
	void UpdateScore(uint32 score);
private:

	bool SetGaugeColor(GaugeType type);

	const float GAUGE_ASPECT = 16.0f;
	const float GAUGE_BORDER = 0.05f;
	class Game* m_game;
	Graphics::Texture m_jacket;
	Graphics::Image m_jacketImage;
	int m_scoreDisplay;
	int m_scoreTarget;
	float m_comboScale;
	uint32 m_combo = 0;
	Graphics::Text m_title;
	Graphics::Text m_artist;
	Graphics::Text m_alertText[2];

	//0 = first 4, 1 = last 4;
	Graphics::Text m_scoreText[2];
	Graphics::Text m_comboText;
	Graphics::Font m_font;

	float m_critLine = 0.5f;
	float m_critLineAngle = 0.5f;
	Vector2 m_critEdge[2] = { Vector2(0.f) };
	Vector2 m_critPos = Vector2(0.0f);

	float m_jacketWidth = -5.0f;
	float m_scale = 1.0f;

	float m_laserAlpha[2] = { 0.0f };
	float m_laserPos[2] = { 0.0f };
	Color m_laserColors[2] = { Color::Blue, Color::Red };
	float m_laserAlerts[2] = { 0.0f };

	float m_gauge[2] = { 0.0f };
	float m_gaugeTransition = 1.0f;
	float m_clearTransition = 0.0f;
	//0 = fail, 1 = clear
	Color m_clearColors[2] = { Color::White };
	
	GaugeType m_currentGaugeType = GaugeType::Normal;
	Mesh m_guiMesh;
	Mesh m_gaugeBorderMesh;
	Mesh m_cursorMesh;
	MaterialParameterSet m_jacketParams;
	Material m_fontMat;
	Material m_fillMat;
};