#include "stdafx.h"
#include "ScoreScreen.hpp"
#include "Application.hpp"
#include "GameConfig.hpp"
#include <Audio/Audio.hpp>
#include <Beatmap/MapDatabase.hpp>
#include "Scoring.hpp"
#include "Game.hpp"
#include "AsyncAssetLoader.hpp"
#include "HealthGauge.hpp"
#ifdef _WIN32
#include "SDL_keycode.h"
#else
#include "SDL2/SDL_keycode.h"
#endif
#include "lua.hpp"
#include "Shared/Time.hpp"

class ScoreScreen_Impl : public ScoreScreen
{
private:
	MapDatabase m_mapDatabase;
	// Things for score screen
	Graphics::Font m_specialFont;
	Sample m_applause;
	Texture m_categorizedHitTextures[4];
	lua_State* m_lua = nullptr;
	bool m_autoplay;
	bool m_autoButtons;
	bool m_startPressed;
	bool m_showStats;
	bool m_manualExit;
	uint32 m_score;
	uint32 m_maxCombo;
	uint32 m_categorizedHits[3];
	float m_finalGaugeValue;
	float* m_gaugeSamples;
	String m_jacketPath;
	uint32 m_timedHits[2];
	float m_meanHitDelta;
	MapTime m_medianHitDelta;
	ScoreIndex m_scoredata;

	Vector<ScoreIndex*> m_highScores;
	Vector<SimpleHitStat> m_simpleHitStats;

	BeatmapSettings m_beatmapSettings;
	Texture m_jacketImage;
	Texture m_graphTex;
	GameFlags m_flags;

	void m_PushStringToTable(const char* name, String data)
	{
		lua_pushstring(m_lua, name);
		lua_pushstring(m_lua, data.c_str());
		lua_settable(m_lua, -3);
	}
	void m_PushFloatToTable(const char* name, float data)
	{
		lua_pushstring(m_lua, name);
		lua_pushnumber(m_lua, data);
		lua_settable(m_lua, -3);
	}
	void m_PushIntToTable(const char* name, int data)
	{
		lua_pushstring(m_lua, name);
		lua_pushinteger(m_lua, data);
		lua_settable(m_lua, -3);
	}

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
		m_flags = game->GetFlags();
		m_highScores = game->GetDifficultyIndex().scores;
		m_scoredata.score = m_score;
		m_scoredata.crit = m_categorizedHits[2];
		m_scoredata.almost = m_categorizedHits[1];
		m_scoredata.miss = m_categorizedHits[0];
		m_scoredata.gauge = m_finalGaugeValue;
		m_scoredata.gameflags = (uint32)m_flags;
		m_manualExit = game->GetManualExit();

		memcpy(m_categorizedHits, scoring.categorizedHits, sizeof(scoring.categorizedHits));
		m_meanHitDelta = scoring.GetMeanHitDelta();
		m_medianHitDelta = scoring.GetMedianHitDelta();
		for (HitStat* stat : scoring.hitStats)
		{
			if (!stat->forReplay)
				continue;
			SimpleHitStat shs;
			if (stat->object)
			{
				if (stat->object->type == ObjectType::Hold)
				{
					shs.lane = ((HoldObjectState*)stat->object)->index;
				}
				else if (stat->object->type == ObjectType::Single)
				{
					shs.lane = ((ButtonObjectState*)stat->object)->index;
				}
				else
				{
					shs.lane = ((LaserObjectState*)stat->object)->index + 6;
				}
			}

			shs.rating = (int8)stat->rating;
			shs.time = stat->time;
			shs.delta = stat->delta;
			shs.hold = stat->hold;
			shs.holdMax = stat->holdMax;
			m_simpleHitStats.Add(shs);
		}

		// Don't save the score if autoplay was on or if the song was launched using command line
		// also don't save the score if the song was manually exited
		if (!m_autoplay && !m_autoButtons && game->GetDifficultyIndex().mapId != -1 && !game->GetManualExit())
		{
			m_mapDatabase.AddScore(game->GetDifficultyIndex(),
				m_score,
				m_categorizedHits[2],
				m_categorizedHits[1],
				m_categorizedHits[0],
				m_finalGaugeValue,
				(uint32)m_flags,
				m_simpleHitStats,
				Shared::Time::Now().Data());
		}

		// Used for jacket images

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
		if (m_lua)
			g_application->DisposeLua(m_lua);
	}

	AsyncAssetLoader loader;
	virtual bool AsyncLoad() override
	{
		return true;
	}
	virtual bool AsyncFinalize() override
	{
		if(!loader.Finalize())
			return false;

		m_lua = g_application->LoadScript("result");
		if (!m_lua)
			return false;
		//set lua table
		lua_newtable(m_lua);
		m_PushIntToTable("score", m_score);
		m_PushIntToTable("flags", (int)m_flags);
		m_PushFloatToTable("gauge", m_finalGaugeValue);
		m_PushIntToTable("misses", m_categorizedHits[0]);
		m_PushIntToTable("goods", m_categorizedHits[1]);
		m_PushIntToTable("perfects", m_categorizedHits[2]);
		m_PushIntToTable("maxCombo", m_maxCombo);
		m_PushIntToTable("level", m_beatmapSettings.level);
		m_PushIntToTable("difficulty", m_beatmapSettings.difficulty);
		m_PushStringToTable("title", m_beatmapSettings.title);
		m_PushStringToTable("artist", m_beatmapSettings.artist);
		m_PushStringToTable("effector", m_beatmapSettings.effector);
		m_PushStringToTable("bpm", m_beatmapSettings.bpm);
		m_PushStringToTable("jacketPath", m_jacketPath);
		m_PushIntToTable("medianHitDelta", m_medianHitDelta);
		m_PushFloatToTable("meanHitDelta", m_meanHitDelta);
		m_PushIntToTable("earlies", m_timedHits[0]);
		m_PushIntToTable("lates", m_timedHits[1]);
		m_PushStringToTable("grade", Scoring::CalculateGrade(m_score).c_str());
		if(m_manualExit)
			m_PushIntToTable("badge", 0);
		else
			m_PushIntToTable("badge", Scoring::CalculateBadge(m_scoredata));

		//Push gauge samples
		lua_pushstring(m_lua, "gaugeSamples");
		lua_newtable(m_lua);
		for (size_t i = 0; i < 256; i++)
		{
			lua_pushnumber(m_lua, m_gaugeSamples[i]);
			lua_rawseti(m_lua, -2, i + 1);
		}
		lua_settable(m_lua, -3);

		lua_pushstring(m_lua, "highScores");
		lua_newtable(m_lua);
		int scoreIndex = 1;
		for (auto& score : m_highScores)
		{
			lua_pushinteger(m_lua, scoreIndex++);
			lua_newtable(m_lua);
			m_PushFloatToTable("gauge", score->gauge);
			m_PushIntToTable("flags", score->gameflags);
			m_PushIntToTable("score", score->score);
			m_PushIntToTable("perfects", score->crit);
			m_PushIntToTable("goods", score->almost);
			m_PushIntToTable("misses", score->miss);
			m_PushIntToTable("timestamp", score->timestamp);
			m_PushIntToTable("badge", Scoring::CalculateBadge(*score));
			lua_settable(m_lua, -3);
		}
		lua_settable(m_lua, -3);

		///TODO: maybe push complete hit stats

		lua_setglobal(m_lua, "result");

		return true;
	}
	bool Init() override
	{
		return true;
	}


	virtual void OnKeyPressed(int32 key) override
	{
		if(key == SDLK_ESCAPE || key == SDLK_RETURN)
		{
			g_application->RemoveTickable(this);
		}
		if (key == SDLK_F12)
		{
			auto luaPopInt = [this]
			{
				int a = lua_tonumber(m_lua, lua_gettop(m_lua));
				lua_pop(m_lua, 1);
				return a;
			};
			int x, y, w, h;
			lua_getglobal(m_lua, "get_capture_rect");
			if (lua_isfunction(m_lua, -1))
			{
				if (lua_pcall(m_lua, 0, 4, 0) != 0)
				{
					Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
					g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
				}
				h = luaPopInt();
				w = luaPopInt();
				y = luaPopInt();
				x = luaPopInt();
				if (g_gameConfig.GetBool(GameConfigKeys::ForcePortrait))
				{
					x += g_gameWindow->GetWindowSize().x / 2 - g_resolution.x / 2;
				}
			}
			else
			{
				if (g_gameConfig.GetBool(GameConfigKeys::ForcePortrait))
				{
					x = g_gameWindow->GetWindowSize().x / 2 - g_resolution.x / 2;;
					y = 0;
					w = g_resolution.x;
					h = g_resolution.y;
				}
				else
				{
					x = 0;
					y = 0;
					w = g_resolution.x;
					h = g_resolution.y;
				}
			}
			Vector2i size(w, h);
			Image screenshot = ImageRes::Screenshot(g_gl, size, { x,y });
			String screenshotPath = "screenshots/" + Shared::Time::Now().ToString() + ".png";
			screenshot->SavePNG(screenshotPath);
			screenshot.Release();

			lua_getglobal(m_lua, "screenshot_captured");
			if (lua_isfunction(m_lua, -1))
			{
				lua_pushstring(m_lua, *screenshotPath);
				lua_call(m_lua, 1, 0);
			}
			lua_settop(m_lua, 0);
		}
		if (key == SDLK_F9)
		{
			g_application->ReloadScript("result", m_lua);
		}
	}
	virtual void OnKeyReleased(int32 key) override
	{
	}
	virtual void Render(float deltaTime) override
	{
		lua_getglobal(m_lua, "render");
		lua_pushnumber(m_lua, deltaTime);
		lua_pushboolean(m_lua, m_showStats);
		if (lua_pcall(m_lua, 2, 0, 0) != 0)
		{
			Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
			assert(false);
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
			m_showStats = true;
		else
			m_showStats = false;
	}

	virtual void OnSuspend()
	{
	}
	virtual void OnRestore()
	{
		g_application->DiscordPresenceMenu("Result Screen");
	}

};

ScoreScreen* ScoreScreen::Create(class Game* game)
{
	ScoreScreen_Impl* impl = new ScoreScreen_Impl(game);
	return impl;
}
