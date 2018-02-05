#include "stdafx.h"
#include "ScoreScreen.hpp"
#include "Application.hpp"
#include "GameConfig.hpp"
#include <GUI/GUI.hpp>
#include <GUI/CommonGUIStyle.hpp>
#include <Audio/Audio.hpp>
#include <Beatmap/MapDatabase.hpp>
#include "Scoring.hpp"
#include "Game.hpp"
#include "AsyncAssetLoader.hpp"
#include "HealthGauge.hpp"
#include "SongSelectStyle.hpp"
#include "PerformanceGraph.hpp"
#ifdef _WIN32
#include "SDL_keycode.h"
#else
#include "SDL2/SDL_keycode.h"
#endif

class ScoreScreen_Impl : public ScoreScreen
{
private:
	Ref<CommonGUIStyle> m_guiStyle;
	Ref<Canvas> m_canvas;
	Ref<HealthGauge> m_gauge;
	Ref<Panel> m_jacket;
	Ref<Canvas> m_timingStatsCanvas;
	Ref<LayoutBox> m_itemBox;
	MapDatabase m_mapDatabase;
	// Things for score screen
	Graphics::Font m_specialFont;
	Sample m_applause;
	Texture m_categorizedHitTextures[4];

	bool m_autoplay;
	bool m_autoButtons;
	bool m_startPressed;
	uint32 m_score;
	uint32 m_maxCombo;
	uint32 m_categorizedHits[3];
	float m_finalGaugeValue;
	float* m_gaugeSamples;
	String m_jacketPath;
	uint32 m_timedHits[2];
	float m_meanHitDelta;
	MapTime m_medianHitDelta;

	Ref<SongSelectStyle> m_songSelectStyle;

	BeatmapSettings m_beatmapSettings;
	Texture m_jacketImage;
	Texture m_graphTex;


public:
	ScoreScreen_Impl(class Game* game)
	{
		Scoring& scoring = game->GetScoring();
		m_autoplay = scoring.autoplay;
		m_autoButtons = scoring.autoplayButtons;
		m_score = scoring.CalculateCurrentScore();
		m_maxCombo = scoring.maxComboCounter;
		m_finalGaugeValue = scoring.currentGauge;
		m_gaugeSamples = game->GetGaugeSamples();
		m_timedHits[0] = scoring.timedHits[0];
		m_timedHits[1] = scoring.timedHits[1];

		memcpy(m_categorizedHits, scoring.categorizedHits, sizeof(scoring.categorizedHits));
		m_meanHitDelta = scoring.GetMeanHitDelta();
		m_medianHitDelta = scoring.GetMedianHitDelta();
		// Don't save the score if autoplay was on or if the song was launched using command line
		if(!m_autoplay && !m_autoButtons && game->GetDifficultyIndex().mapId != -1)
			m_mapDatabase.AddScore(game->GetDifficultyIndex(), m_score, m_categorizedHits[2], m_categorizedHits[1], m_categorizedHits[0], m_finalGaugeValue);

		// Used for jacket images
		m_songSelectStyle = SongSelectStyle::Get(g_application);

		m_startPressed = false;

		m_beatmapSettings = game->GetBeatmap()->GetMapSettings();
		m_jacketPath = Path::Normalize(game->GetMapRootPath() + Path::sep + m_beatmapSettings.jacketPath);
		m_jacketImage = game->GetJacketImage();

		// Make texture for performance graph samples
		m_graphTex = TextureRes::Create(g_gl);
		m_graphTex->Init(Vector2i(256, 1), Graphics::TextureFormat::RGBA8);
		Colori graphPixels[256];
		for (uint32 i = 0; i < 256; i++)
		{
			graphPixels[i].x = 255.0f * Math::Clamp(m_gaugeSamples[i], 0.0f, 1.0f);
		}
		m_graphTex->SetData(Vector2i(256, 1), graphPixels);
		m_graphTex->SetWrap(Graphics::TextureWrap::Clamp, Graphics::TextureWrap::Clamp);

	}
	~ScoreScreen_Impl()
	{
		g_rootCanvas->Remove(m_canvas.As<GUIElementBase>());
	}

	AsyncAssetLoader loader;
	virtual bool AsyncLoad() override
	{
		m_guiStyle = g_commonGUIStyle;

		m_canvas = Utility::MakeRef(new Canvas());
		String skin = g_gameConfig.GetString(GameConfigKeys::Skin);
		// Font
		CheckedLoad(m_specialFont = FontRes::Create(g_gl,"skins/" + skin + "/fonts/divlit_custom.ttf"));
		CheckedLoad(m_applause = g_audio->CreateSample("skins/" + skin + "/audio/applause.wav"));

		// Background
		Panel* fullBg = new Panel();
		loader.AddTexture(fullBg->texture, "score/bg.png");
		fullBg->color = Color(1.0f);
		{
			Canvas::Slot* slot = m_canvas->Add(fullBg->MakeShared());
			slot->anchor = Anchors::Full;
			slot->SetZOrder(-2);
		}
		
		Canvas* innerCanvas = new Canvas();
		Canvas::Slot* innerSlot = m_canvas->Add(innerCanvas->MakeShared());
		Vector2 canvasRes = GUISlotBase::ApplyFill(FillMode::Fit, Vector2(640, 480), Rect(0, 0, g_resolution.x, g_resolution.y)).size;
		float scale = Math::Min(canvasRes.x / 640.f, canvasRes.y / 480.f) / 2.f;

		Vector2 topLeft = Vector2(g_resolution / 2 - canvasRes / 2);

		Vector2 bottomRight = topLeft + canvasRes;

		innerSlot->allowOverflow = true;
		topLeft /= g_resolution;
		bottomRight /= g_resolution;

		if (g_aspectRatio < 640.f / 480.f)
			innerSlot->anchor = Anchor(topLeft.x, Math::Min(topLeft.y, 0.20f), bottomRight.x, bottomRight.y);
		else
			innerSlot->anchor = Anchors::Full;

		
		// Border
		Panel* border = new Panel();
		border->color = Color::Black.WithAlpha(0.25f);
		{
			Canvas::Slot* slot = innerCanvas->Add(border->MakeShared());
			slot->anchor = Anchors::Full;
			slot->padding = Margin(0, 30);
			slot->SetZOrder(-1);
		}

		float screenSplit = 0.35f;
		float sidePadding = 40.0f;

		// Song info container
		{
			// Contains all song info
			LayoutBox* songInfoContainer = new LayoutBox();
			songInfoContainer->layoutDirection = LayoutBox::Horizontal;
			{
				Canvas::Slot* slot = innerCanvas->Add(songInfoContainer->MakeShared());
				slot->anchor = Anchor(0.0f, 0.0f, 1.0f, screenSplit);
				slot->padding = Margin(sidePadding * scale);
				slot->padding.top = (30 + 20) * scale;
				slot->padding.bottom = (20) * scale;
				slot->alignment = Vector2(0.0f, 0.5f);
			}

			// Jacket image
			Panel* jacketImage = new Panel();
			m_jacket = Ref<Panel>(jacketImage);
			jacketImage->texture = m_jacketImage;
			jacketImage->imageFillMode = FillMode::Fit;
			{
				LayoutBox::Slot* slot = songInfoContainer->Add(jacketImage->MakeShared());
				slot->fillY = true;
				slot->alignment = Vector2(0.0f, 0.0f);
			}

			// Metadata container
			LayoutBox* metadataContainer = new LayoutBox();
			metadataContainer->layoutDirection = LayoutBox::Vertical;
			{
				LayoutBox::Slot* slot = songInfoContainer->Add(metadataContainer->MakeShared());
				slot->alignment = Vector2(0.0f, 0.5f);
				slot->padding.left = 30 * scale;
			}
			auto AddMetadataLine = [&](const String& text)
			{
				Label* label = new Label();
				label->SetText(Utility::ConvertToWString(text));
				label->SetFontSize(48 * scale);
				LayoutBox::Slot* slot = metadataContainer->Add(label->MakeShared());
			};
			// Title/Artist/Effector/Etc.
			AddMetadataLine(m_beatmapSettings.title + Utility::Sprintf(" [%d]", m_beatmapSettings.level));
			AddMetadataLine(m_beatmapSettings.artist);
			AddMetadataLine("Effected By: " + m_beatmapSettings.effector);
		}

		// Main score container
		{
			Panel* scoreContainerBg = new Panel();
			scoreContainerBg->color = Color::Black.WithAlpha(0.5f);
			{
				Canvas::Slot* slot = innerCanvas->Add(scoreContainerBg->MakeShared());
				slot->anchor = Anchor(0.0f, screenSplit, 1.0f, 1.0f);
				slot->padding = Margin(0, 0, 0, 50);
			}

			LayoutBox* scoreContainer = new LayoutBox();
			scoreContainer->layoutDirection = LayoutBox::Horizontal;
			{
				Canvas::Slot* slot = innerCanvas->Add(scoreContainer->MakeShared());
				slot->anchor = Anchor(0.0f, screenSplit, 1.0f, 1.0f); 
				slot->padding = Margin(0, 0, 0, 50);
			}



			// Score and graph
			LayoutBox* scoreAndGraph = new LayoutBox();
			scoreAndGraph->layoutDirection = LayoutBox::Vertical;
			{
				LayoutBox::Slot* slot = scoreContainer->Add(scoreAndGraph->MakeShared());
				slot->alignment = Vector2(0.0f, 0.5f);
				slot->padding = Margin(20 * scale, 10 * scale);
				slot->fillX = true;
				slot->fillY = true;
				slot->fillAmount = 1.0f;
			}

			Label* score = new Label();
			score->SetText(Utility::WSprintf(L"%08d", m_score));
			score->SetFont(m_specialFont);
			score->SetFontSize(80 * scale);
			score->color = Color(0.75f);
			score->SetTextOptions(FontRes::Monospace);
			{
				LayoutBox::Slot* slot = scoreAndGraph->Add(score->MakeShared());
				slot->padding = Margin(0, 0, 0, 20 * scale);
				slot->fillX = false;
				slot->alignment = Vector2(0.5f, 0.0f);
			}

			Label* perfomanceTitle = new Label();
			perfomanceTitle->SetText(L"Performance");
			perfomanceTitle->SetFont(m_specialFont);
			perfomanceTitle->SetFontSize(40 * scale);
			perfomanceTitle->color = Color(0.6f);
			{
				LayoutBox::Slot* slot = scoreAndGraph->Add(perfomanceTitle->MakeShared());
				slot->padding = Margin(5 * scale, 0, 0, 5 * scale);
				slot->alignment = Vector2(0.0f, 0.0f);
			}

			PerformanceGraph* graphPanel = new PerformanceGraph();
			loader.AddTexture(graphPanel->borderTexture, "ui/button.png");

			graphPanel->graphTex = m_graphTex;

			graphPanel->border = Margini(5);
			{
				LayoutBox::Slot* slot = scoreAndGraph->Add(graphPanel->MakeShared());
				slot->fillY = true;
				slot->fillX = true;
				slot->padding = Margin(0, 0);
			}

			// Grade and hits
			LayoutBox* gradePanel = new LayoutBox();
			gradePanel->layoutDirection = LayoutBox::Vertical;
			{
				LayoutBox::Slot* slot = scoreContainer->Add(gradePanel->MakeShared());
				slot->alignment = Vector2(0.0f, 0.5f);
				slot->padding = Margin(30 * scale, 10 * scale);
				slot->fillX = true;
				slot->fillY = true;
			}

			String gradeImagePath = String("score") + Path::sep + Scoring::CalculateGrade(m_score) + ".png";
			Panel* gradeImage = new Panel();
			loader.AddTexture(gradeImage->texture, gradeImagePath);
			gradeImage->imageFillMode = FillMode::Fit;
			{
				LayoutBox::Slot* slot = gradePanel->Add(gradeImage->MakeShared());
				slot->fillX = true;
				slot->fillY = true;
				slot->fillAmount = 0.7f;
			}

			// Hit items
			m_itemBox = Ref<LayoutBox>(new LayoutBox());
			m_itemBox->layoutDirection = LayoutBox::Vertical;
			{
				LayoutBox::Slot* slot = gradePanel->Add(m_itemBox->MakeShared());
				slot->fillX = true;
				slot->fillY = true;
				slot->padding = Margin(0, 20);
			}


			m_gauge = Ref<HealthGauge>(new HealthGauge());
			loader.AddTexture(m_gauge->fillTexture, "gauge_fill.png");
			loader.AddTexture(m_gauge->frontTexture, "gauge_front.png");
			loader.AddTexture(m_gauge->backTexture, "gauge_back.png");
			loader.AddTexture(m_gauge->maskTexture, "gauge_mask.png");
			loader.AddMaterial(m_gauge->fillMaterial, "gauge");
			m_gauge->rate = m_finalGaugeValue;
			{
				LayoutBox::Slot* slot = scoreContainer->Add(m_gauge.As<GUIElementBase>());
				slot->fillY = true;
			}

			{
				m_timingStatsCanvas = Ref<Canvas>(new Canvas());
				Canvas::Slot* slot = innerCanvas->Add(m_timingStatsCanvas->MakeShared());
				slot->anchor = Anchor(0, 0.75, 0.25, 1);

				Panel* statBackground = new Panel();
				statBackground->color = Color(0, 0, 0).WithAlpha(0.75f);
				slot = m_timingStatsCanvas->Add(statBackground->MakeShared());
				slot->anchor = Anchors::Full;

				LayoutBox* statsList = new LayoutBox();
				statsList->layoutDirection = LayoutBox::LayoutDirection::Vertical;

				{
					Label* timingStat = new Label();
					timingStat->SetText(Utility::WSprintf(L"Early: %d / Late: %d", m_timedHits[0], m_timedHits[1]));
					timingStat->SetFontSize(24 * scale);
					LayoutBox::Slot* boxSlot = statsList->Add(timingStat->MakeShared());
					boxSlot->fillX = true;
					boxSlot->padding = Margin(3 * scale, 3 * scale, 0, 0);
				}
				{
					Label* timingStat = new Label();
					timingStat->SetText(Utility::WSprintf(L"Mean hit delta: %.2fms", m_meanHitDelta));
					timingStat->SetFontSize(24 * scale);
					LayoutBox::Slot* boxSlot = statsList->Add(timingStat->MakeShared());
					boxSlot->fillX = true;
					boxSlot->padding = Margin(3 * scale, 3 * scale, 0, 0);
				}
				{
					Label* timingStat = new Label();
					timingStat->SetText(Utility::WSprintf(L"Median hit delta: %dms", m_medianHitDelta));
					timingStat->SetFontSize(24 * scale);
					LayoutBox::Slot* boxSlot = statsList->Add(timingStat->MakeShared());
					boxSlot->fillX = true;
					boxSlot->padding = Margin(3 * scale, 3 * scale, 0, 0);
				}
				slot = m_timingStatsCanvas->Add(statsList->MakeShared());
				slot->anchor = Anchors::Full;
				m_timingStatsCanvas->visibility = Visibility::Hidden;
			}

		}

		// Load hit textures (Good, Perfect, Miss)
		loader.AddTexture(m_categorizedHitTextures[3], "score/scorec.png"); // Max combo
		loader.AddTexture(m_categorizedHitTextures[2], "score/score2.png");
		loader.AddTexture(m_categorizedHitTextures[1], "score/score1.png");
		loader.AddTexture(m_categorizedHitTextures[0], "score/score0.png");

		return loader.Load();
	}
	virtual bool AsyncFinalize() override
	{
		if(!loader.Finalize())
			return false;

		Vector2 canvasRes = GUISlotBase::ApplyFill(FillMode::Fit, Vector2(640, 480), Rect(0, 0, g_resolution.x, g_resolution.y)).size;
		float scale = Math::Min(canvasRes.x / 640.f, canvasRes.y / 480.f) / 2.f;

		// Make gauge material transparent
		m_gauge->fillMaterial->opaque = false;

		// Add indicators for ammounts of miss/good/perfect hits
		auto AddScoreRow = [&](Texture texture, int32 count)
		{
			Canvas* canvas = new Canvas();
			LayoutBox::Slot* slot4 = m_itemBox->Add(canvas->MakeShared());
			slot4->fillX = true;
			slot4->fillY = false;
			slot4->alignment = Vector2(0.0f, 0.5f);
			slot4->padding = Margin(0, 5);
			slot4->allowOverflow = true;
			Panel* icon = new Panel();
			icon->color = Color::White;
			icon->texture = texture;
			icon->imageFillMode = FillMode::Fit;
			icon->imageAlignment = Vector2(0.5, 0.5);
			Canvas::Slot* canvasSlot = canvas->Add(icon->MakeShared());
			canvasSlot->anchor = Anchor(0.0f, 0.33f, 0.5f, 1.0f); /// TODO: Remove Y offset and center properly
			canvasSlot->autoSizeX = true;
			canvasSlot->autoSizeY = true;
			canvasSlot->alignment = Vector2(0.0f, 0.5f);

			Label* countLabel = new Label();
			countLabel->SetFont(m_specialFont);
			countLabel->SetFontSize(64 * scale);
			countLabel->SetTextOptions(FontRes::Monospace);
			countLabel->color = Color(0.5f);
			countLabel->SetText(Utility::WSprintf(L"%05d", count));
			canvasSlot = canvas->Add(countLabel->MakeShared());
			canvasSlot->anchor = Anchor(0.5f, 0.0f, 1.0f, 1.0f); 
			canvasSlot->autoSizeX = true;
			canvasSlot->autoSizeY = true;
			canvasSlot->alignment = Vector2(1.0f, 0.5f);
		};

		// Add hit statistics
		AddScoreRow(m_categorizedHitTextures[2], m_categorizedHits[2]);
		AddScoreRow(m_categorizedHitTextures[1], m_categorizedHits[1]);
		AddScoreRow(m_categorizedHitTextures[0], m_categorizedHits[0]);
		AddScoreRow(m_categorizedHitTextures[3], m_maxCombo);

		return true;
	}
	bool Init() override
	{
		// Add to screen
		Canvas::Slot* rootSlot = g_rootCanvas->Add(m_canvas.As<GUIElementBase>());
		rootSlot->anchor = Anchors::Full;

		// Play score screen sound
		m_applause->Play();

		return true;
	}


	virtual void OnKeyPressed(int32 key) override
	{
		if(key == SDLK_ESCAPE || key == SDLK_RETURN)
		{
			g_application->RemoveTickable(this);
		}
	}
	virtual void OnKeyReleased(int32 key) override
	{
	}
	virtual void Render(float deltaTime) override
	{
		// Poll for loaded jacket image
		if(m_jacketImage == m_songSelectStyle->loadingJacketImage)
		{
			m_jacketImage = m_songSelectStyle->GetJacketThumnail(m_jacketPath);
			m_jacket->texture = m_jacketImage;
		}
	}
	virtual void Tick(float deltaTime) override
	{
		// Check for button pressed here instead of adding to onbuttonpressed for stability reasons
		//TODO: Change to onbuttonpressed
		if(m_startPressed && !g_input.GetButton(Input::Button::BT_S))
			g_application->RemoveTickable(this);

		m_startPressed = g_input.GetButton(Input::Button::BT_S);

		if (g_input.GetButton(Input::Button::FX_0))
			m_timingStatsCanvas->visibility = Visibility::Visible;
		else
			m_timingStatsCanvas->visibility = Visibility::Hidden;
	}

	virtual void OnSuspend()
	{
		g_rootCanvas->Remove(m_canvas.As<GUIElementBase>());
	}
	virtual void OnRestore()
	{
		Canvas::Slot* slot = g_rootCanvas->Add(m_canvas.As<GUIElementBase>());
		slot->anchor = Anchors::Full;
	}

};

ScoreScreen* ScoreScreen::Create(class Game* game)
{
	ScoreScreen_Impl* impl = new ScoreScreen_Impl(game);
	return impl;
}
