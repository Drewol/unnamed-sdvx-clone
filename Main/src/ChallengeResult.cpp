#include "stdafx.h"
#include "ChallengeResult.hpp"
#include "ChallengeSelect.hpp"
#include "Application.hpp"
#include "GameConfig.hpp"
#include <Audio/Audio.hpp>
#include <Beatmap/MapDatabase.hpp>
#include "Scoring.hpp"
#include "Game.hpp"
#include "AsyncAssetLoader.hpp"
#include "lua.hpp"
#include "Shared/Time.hpp"
#include "json.hpp"
#include "CollectionDialog.hpp"
#include <Beatmap/TinySHA1.hpp>
#include "MultiplayerScreen.hpp"
#include "ChatOverlay.hpp"

class ChallengeResultScreen_Impl : public ChallengeResultScreen
{
private:
	Graphics::Font m_specialFont;
	Texture m_categorizedHitTextures[4];

	Sample m_applause;
	lua_State* m_lua = nullptr;
	bool m_startPressed;

	String m_jacketPath;
	Texture m_jacketImage;

	ScoreIndex m_scoredata;
	bool m_restored = false;
	float m_scroll = 0.0;
	float m_sensMult = 1.0f;

	bool m_removed = false;
	bool m_hasScreenshot = false;
	bool m_hasRendered = false;

	LuaBindable* m_bindable = nullptr;

    ChallengeManager* m_manager = NULL;

	void m_PushBoolToTable(const char* name, bool data)
	{
		lua_pushstring(m_lua, name);
		lua_pushboolean(m_lua, data);
		lua_settable(m_lua, -3);
	}
	void m_PushStringToTable(const char* name, const String& data)
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
	void m_OnButtonPressed(Input::Button button, int32 delta)
	{
		if ((button == Input::Button::BT_S || button == Input::Button::Back) && m_restored && !m_removed)
		{
			g_application->RemoveTickable(this);
			m_removed = true;
		}

		switch (button) {
			case Input::Button::Result_Continue:
				g_application->RemoveTickable(this);
				m_removed = true;
				break;
			case Input::Button::Result_Screenshot:
				Capture();
				break;
			case Input::Button::ReloadSkin:
				g_application->ReloadScript("challengeresult", m_lua);
				lua_getglobal(m_lua, "result_set");
				if (lua_isfunction(m_lua, -1))
				{
					if (lua_pcall(m_lua, 0, 0, 0) != 0)
					{
						Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
						g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
					}
				}
				lua_settop(m_lua, 0);
				break;
			case Input::Button::ScrollDown:
				m_scroll += 1;
				break;
			case Input::Button::ScrollUp:
				m_scroll += 1;
				break;
		}
	}
public:

	ChallengeResultScreen_Impl(ChallengeManager* man)
	{
		m_manager = man;

		m_startPressed = false;
	}
	~ChallengeResultScreen_Impl()
	{
		if (m_bindable)
			delete m_bindable;

		g_input.OnButtonPressed.RemoveAll(this);
		g_gameWindow->OnMouseScroll.RemoveAll(this);

		if (m_lua)
			g_application->DisposeLua(m_lua);
	}

	AsyncAssetLoader loader;
	virtual bool AsyncLoad() override
	{
		return true;
	}

	void updateLuaData()
	{
		const Vector<ChallengeResult>& res = m_manager->GetResults();
		const Vector<ChartIndex*>& charts = m_manager->GetCharts();
		const OverallChallengeResult& ores = m_manager->GetOverallResults();


		lua_newtable(m_lua);

		// TODO something to tell what failed
		lua_pushstring(m_lua, "charts");

		lua_newtable(m_lua);
		for (unsigned int i = 0; i < res.size(); i++)
		{
			lua_pushnumber(m_lua, (uint64)i+1u);
			lua_newtable(m_lua);
			const ChartIndex* chart = charts[i];
			const ChallengeResult& cres = res[i];

			String jacketPath = Path::RemoveLast(chart->path, nullptr) + Path::sep + chart->jacket_path;
			m_PushStringToTable("jacketPath", jacketPath);
			m_PushStringToTable("title", chart->title);
			m_PushStringToTable("artist", chart->artist);
			m_PushStringToTable("effector", chart->effector);
			m_PushStringToTable("illustrator", chart->illustrator);
			m_PushStringToTable("bpm", chart->bpm);
			m_PushIntToTable("level", chart->level);

			m_PushIntToTable("score", cres.score);
			m_PushIntToTable("percent", cres.percent);
			m_PushFloatToTable("gauge", cres.gauge);

			m_PushIntToTable("gauge_type", (uint32)cres.opts.gaugeType);
			m_PushIntToTable("gauge_option", cres.opts.gaugeOption);
			m_PushBoolToTable("random", cres.opts.random);
			m_PushBoolToTable("mirror", cres.opts.mirror);
			m_PushIntToTable("auto_flags", (uint32)cres.opts.autoFlags);

			m_PushIntToTable("misses", cres.errors);
			m_PushIntToTable("goods", cres.nears);
			m_PushIntToTable("perfects", cres.crits);
			m_PushIntToTable("maxCombo", cres.maxCombo);
			m_PushStringToTable("grade", ToDisplayString(ToGradeMark(cres.score)));
			m_PushIntToTable("badge", static_cast<int>(cres.badge));
			m_PushBoolToTable("passed", cres.passed);
			m_PushStringToTable("failReason", *cres.failString);

			// Scorescreen spcific values
			m_PushBoolToTable("isSelf", true);
			m_PushIntToTable("difficulty", cres.scorescreenInfo.difficulty);
			m_PushStringToTable("realTitle", chart->title);
			m_PushIntToTable("duration", cres.scorescreenInfo.beatmapDuration);
			m_PushIntToTable("medianHitDelta", cres.scorescreenInfo.medianHitDelta);
			m_PushIntToTable("meanHitDelta", cres.scorescreenInfo.meanHitDelta);
			m_PushIntToTable("medianHitDeltaAbs", cres.scorescreenInfo.medianHitDeltaAbs);
			m_PushIntToTable("meanHitDeltaAbs", cres.scorescreenInfo.meanHitDeltaAbs);
			m_PushIntToTable("earlies", cres.scorescreenInfo.earlies);
			m_PushIntToTable("lates", cres.scorescreenInfo.lates);
			m_PushBoolToTable("autoplay", false);
			m_PushFloatToTable("playbackSpeed", 1.0);
			// Todo do we want to put more info here
			m_PushStringToTable("mission", "");
			m_PushIntToTable("retryCount", 0);
			lua_pushstring(m_lua, "hitWindow");
			cres.scorescreenInfo.hitWindow.ToLuaTable(m_lua);
			lua_settable(m_lua, -3);

			//Push gauge samples
			lua_pushstring(m_lua, "gaugeSamples");
			lua_newtable(m_lua);
			for (size_t i = 0; i < 256; i++)
			{
				lua_pushnumber(m_lua, cres.scorescreenInfo.gaugeSamples[i]);
				lua_rawseti(m_lua, -2, i + 1);
			}
			lua_settable(m_lua, -3);

			lua_pushstring(m_lua, "highScores");
			lua_newtable(m_lua);
			int scoreIndex = 1;
			for (auto& score : chart->scores)
			{
				lua_pushinteger(m_lua, scoreIndex++);
				lua_newtable(m_lua);
				m_PushFloatToTable("gauge", score->gauge);

				m_PushIntToTable("gauge_type", (uint32)score->gaugeType);
				m_PushIntToTable("gauge_option", score->gaugeOption);
				m_PushIntToTable("random", score->random);
				m_PushIntToTable("mirror", score->mirror);
				m_PushIntToTable("auto_flags", (uint32)score->autoFlags);

				m_PushIntToTable("score", score->score);
				m_PushIntToTable("perfects", score->crit);
				m_PushIntToTable("goods", score->almost);
				m_PushIntToTable("misses", score->miss);
				m_PushIntToTable("timestamp", score->timestamp);
				m_PushIntToTable("badge", static_cast<int>(Scoring::CalculateBadge(*score)));
				lua_pushstring(m_lua, "hitWindow");
				HitWindow(score->hitWindowPerfect, score->hitWindowGood, score->hitWindowHold, score->hitWindowSlam).ToLuaTable(m_lua);
				lua_settable(m_lua, -3);
				lua_settable(m_lua, -3);
			}
			lua_settable(m_lua, -3);

			m_PushIntToTable("speedModType", cres.scorescreenInfo.speedMod);
			m_PushFloatToTable("speedModValue", cres.scorescreenInfo.speedModValue);

			lua_pushstring(m_lua, "noteHitStats");
			lua_newtable(m_lua);
			for (size_t i = 0; i < cres.scorescreenInfo.simpleNoteHitStats.size(); ++i)
			{
				const SimpleHitStat simpleHitStat = cres.scorescreenInfo.simpleNoteHitStats[i];

				lua_newtable(m_lua);
				m_PushIntToTable("rating", simpleHitStat.rating);
				m_PushIntToTable("lane", simpleHitStat.lane);
				m_PushIntToTable("time", simpleHitStat.time);
				m_PushFloatToTable("timeFrac",
					Math::Clamp(static_cast<float>(simpleHitStat.time) / (
						cres.scorescreenInfo.beatmapDuration > 0 ? cres.scorescreenInfo.beatmapDuration : 1),
						0.0f, 1.0f));
				m_PushIntToTable("delta", simpleHitStat.delta);

				lua_rawseti(m_lua, -2, i + 1);
			}
			lua_settable(m_lua, -3);

			lua_settable(m_lua, -3);
		}

		lua_settable(m_lua, -3);

		m_PushBoolToTable("isSelf", true);
		m_PushBoolToTable("passed", ores.passed);
		m_PushStringToTable("failReason", *ores.failString);
		m_PushStringToTable("grade", ToDisplayString(ToGradeMark(ores.averageScore)));
		m_PushIntToTable("badge", static_cast<int>(ores.clearMark));
		m_PushIntToTable("avgScore", ores.averageScore);
		m_PushIntToTable("avgPercentage", ores.averagePercent);
		m_PushIntToTable("avgGauge", ores.averageGauge);
		m_PushIntToTable("avgErrors", ores.averageErrors);
		m_PushIntToTable("avgNears", ores.averageNears);
		m_PushIntToTable("avgCrits", ores.averageCrits);
		m_PushIntToTable("overallErrors", ores.overallErrors);
		m_PushIntToTable("overallNears", ores.overallNears);
		m_PushIntToTable("overallCrits", ores.overallCrits);

		m_PushStringToTable("title", m_manager->GetChallenge()->title);
		m_PushIntToTable("level", m_manager->GetChallenge()->level);
		m_PushStringToTable("requirement_text", m_manager->GetChallenge()->reqText);

		lua_setglobal(m_lua, "result");

		lua_getglobal(m_lua, "result_set");
		if (lua_isfunction(m_lua, -1))
		{
			if (lua_pcall(m_lua, 0, 0, 0) != 0)
			{
				Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
				g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
			}
		}
		lua_settop(m_lua, 0);
	}
	virtual bool AsyncFinalize() override
	{
		if(!loader.Finalize())
			return false;
		m_lua = g_application->LoadScript("challengeresult");
		if (!m_lua)
			return false;

		m_bindable = new LuaBindable(m_lua, "Challenge");
		m_bindable->AddFunction("GetJSON", this, &ChallengeResultScreen_Impl::LGetCurrentChallengeJSON);
		m_bindable->Push();
		lua_settop(m_lua, 0);

		updateLuaData();
		g_input.OnButtonPressed.Add(this, &ChallengeResultScreen_Impl::m_OnButtonPressed);
		g_gameWindow->OnMouseScroll.Add(this, &ChallengeResultScreen_Impl::m_OnMouseScroll);

		m_sensMult = g_gameConfig.GetFloat(GameConfigKeys::SongSelSensMult);

		return true;
	}
	bool Init() override
	{
		return true;
	}

	int LGetCurrentChallengeJSON(lua_State* L)
	{
		ChallengeIndex* chal = m_manager->GetChallenge();
		if (chal == nullptr)
			TCPSocket::PushJsonValue(L, "{}"_json);
		else
			TCPSocket::PushJsonValue(L, chal->GetSettings());
		return 1;
	}

	void m_OnMouseScroll(int32 steps)
	{
		m_scroll += steps;
	}

	void OnKeyPressed(SDL_Scancode code, int32 delta) override
	{
	}
	void OnKeyReleased(SDL_Scancode code, int32 delta) override
	{
	}
	void Render(float deltaTime) override
	{
		lua_getglobal(m_lua, "render");
		lua_pushnumber(m_lua, deltaTime);
		lua_pushnumber(m_lua, m_scroll);
		if (lua_pcall(m_lua, 2, 0, 0) != 0)
		{
			if (!g_application->ScriptError("challengeresult", m_lua))
			{
				g_application->RemoveTickable(this);
			}
		}
		m_hasRendered = true;
	}
	void Tick(float deltaTime) override
	{
		if (!m_hasScreenshot && m_hasRendered && !IsSuspended())
		{
			AutoScoreScreenshotSettings screensetting = g_gameConfig.GetEnum<Enum_AutoScoreScreenshotSettings>(GameConfigKeys::AutoScoreScreenshot);
			if (screensetting == AutoScoreScreenshotSettings::Always
				|| (screensetting == AutoScoreScreenshotSettings::Highscore
					&& m_manager->GetOverallResults().goodScore))
			{
				Capture();
			}
			m_hasScreenshot = true;
		}

		// Scroll using knobs
		m_scroll += g_input.GetInputLaserDir(0) * m_sensMult;
		m_scroll += g_input.GetInputLaserDir(1) * m_sensMult;
	}

	void OnSuspend() override
	{
		m_restored = false;
	}
	void OnRestore() override
	{
		g_application->DiscordPresenceMenu("Result Screen");
		m_restored = true;
	}

	void Capture()
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
				Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
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
		if (screenshot.get() != nullptr)
		{
			screenshot->SavePNG(screenshotPath);
			screenshot.reset();
		}
		else
		{
			screenshotPath = "Failed to capture screenshot";
		}

		lua_getglobal(m_lua, "screenshot_captured");
		if (lua_isfunction(m_lua, -1))
		{
			lua_pushstring(m_lua, *screenshotPath);
			lua_call(m_lua, 1, 0);
		}
		lua_settop(m_lua, 0);
	}

};

ChallengeResultScreen* ChallengeResultScreen::Create(class ChallengeManager* man)
{
	ChallengeResultScreen_Impl* impl = new ChallengeResultScreen_Impl(man);
	return impl;
}
