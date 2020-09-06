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
#include "lua.hpp"
#include "Shared/Time.hpp"
#include "json.hpp"
#include "CollectionDialog.hpp"
#include <Beatmap/TinySHA1.hpp>
#include "MultiplayerScreen.hpp"
#include "ChatOverlay.hpp"
#include <cpr/cpr.h>
#include "cryptopp/cryptlib.h"
#include "cryptopp/hex.h"
#include "cryptopp/sha3.h"
#include <cryptopp/filters.h>
#include <cryptopp/files.h>

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
	uint8 m_badge;
	uint32 m_score;
	uint32 m_maxCombo;
	uint32 m_categorizedHits[3];
	float m_finalGaugeValue;
	float* m_gaugeSamples;
	String m_jacketPath;
	uint32 m_timedHits[2];

	//0 = normal, 1 = absolute
	float m_meanHitDelta[2] = {0.f, 0.f};
	MapTime m_medianHitDelta[2] = {0, 0};

	ScoreIndex m_scoredata;
	bool m_restored = false;
	bool m_removed = false;
	bool m_hasScreenshot = false;
	bool m_hasRendered = false;
    MultiplayerScreen* m_multiplayer = NULL;
	String m_playerName;
	String m_playerId;
	String m_displayId;
	int m_displayIndex = 0;
	Vector<nlohmann::json> const* m_stats;
	int m_numPlayersSeen = 0;

	Vector<ScoreIndex*> m_highScores;
	Vector<SimpleHitStat> m_simpleHitStats;

	BeatmapSettings m_beatmapSettings;
	Texture m_jacketImage;
	Texture m_graphTex;
	GameFlags m_flags;
	CollectionDialog m_collDiag;
	ChartIndex* m_chartIndex;

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
	void m_OnButtonPressed(Input::Button button)
	{
		if (m_multiplayer && m_multiplayer->GetChatOverlay()->IsOpen())
			return;
		if (m_collDiag.IsActive())
			return;

		if ((button == Input::Button::BT_S || button == Input::Button::Back) && m_restored && !m_removed)
		{
			g_application->RemoveTickable(this);
			m_removed = true;
		}

		if ((button == Input::Button::BT_1 && g_input.GetButton(Input::Button::BT_2)) ||
			(button == Input::Button::BT_2 && g_input.GetButton(Input::Button::BT_1)))
		{
			if (m_collDiag.IsInitialized())
			{
				m_collDiag.Open(*m_chartIndex);
			}
		}

		// Switch between shown scores
		if (m_multiplayer &&
			(button == Input::Button::FX_0 || button == Input::Button::FX_1) &&
			m_restored && !m_removed)
		{
			if (m_stats->size() == 0)
				return;

			m_displayIndex += (button == Input::Button::FX_0) ? -1 : 1;

			if (m_displayIndex >= (int)m_stats->size())
				m_displayIndex = 0;

			if (m_displayIndex < 0)
				m_displayIndex = (int)m_stats->size() - 1;

			loadScoresFromMultiplayer();
			updateLuaData();
		}
	}

public:

	void loadScoresFromGame(class Game* game)
	{
		Scoring& scoring = game->GetScoring();
		// Calculate hitstats
		memcpy(m_categorizedHits, scoring.categorizedHits, sizeof(scoring.categorizedHits));

		m_score = scoring.CalculateCurrentScore();
		m_maxCombo = scoring.maxComboCounter;
		m_finalGaugeValue = scoring.currentGauge;
		m_timedHits[0] = scoring.timedHits[0];
		m_timedHits[1] = scoring.timedHits[1];
		m_flags = game->GetFlags();
		m_scoredata.score = m_score;
		memcpy(m_categorizedHits, scoring.categorizedHits, sizeof(scoring.categorizedHits));
		m_scoredata.crit = m_categorizedHits[2];
		m_scoredata.almost = m_categorizedHits[1];
		m_scoredata.miss = m_categorizedHits[0];
		m_scoredata.gauge = m_finalGaugeValue;
		m_scoredata.gameflags = (uint32)m_flags;
		if (game->GetManualExit())
		{
			m_badge = 0;
		}
		else
		{
			m_badge = Scoring::CalculateBadge(m_scoredata);
		}

		m_meanHitDelta[0] = scoring.GetMeanHitDelta();
		m_medianHitDelta[0] = scoring.GetMedianHitDelta();

		m_meanHitDelta[1] = scoring.GetMeanHitDelta(true);
		m_medianHitDelta[1] = scoring.GetMedianHitDelta(true);

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

	void loadScoresFromMultiplayer() {
		if (m_displayIndex >= (int)m_stats->size())
			return;

		const nlohmann::json& data= (*m_stats)[m_displayIndex];

		m_score = data["score"];
		m_maxCombo = data["combo"];
		m_finalGaugeValue = data["gauge"];
		m_timedHits[0] = data["early"];
		m_timedHits[1] = data["late"];
		m_flags = data["flags"];

		m_categorizedHits[0] = data["miss"];
		m_categorizedHits[1] = data["near"];
		m_categorizedHits[2] = data["crit"];

		m_scoredata.score = data["score"];
		m_scoredata.crit = m_categorizedHits[2];
		m_scoredata.almost = m_categorizedHits[1];
		m_scoredata.miss = m_categorizedHits[0];
		m_scoredata.gauge = m_finalGaugeValue;

		m_scoredata.gameflags = data["flags"];
		m_badge = data["clear"];

		m_meanHitDelta[0] = data["mean_delta"];
		m_medianHitDelta[0] = data["median_delta"];

		m_playerName = static_cast<String>(data.value("name",""));

		auto samples = data["graph"];

		// Make texture for performance graph samples
		m_graphTex = TextureRes::Create(g_gl);
		m_graphTex->Init(Vector2i(256, 1), Graphics::TextureFormat::RGBA8);
		Colori graphPixels[256];
		for (uint32 i = 0; i < 256; i++)
		{
			m_gaugeSamples[i] = samples[i].get<float>();
			graphPixels[i].x = 255.0f * Math::Clamp(m_gaugeSamples[i], 0.0f, 1.0f);
		}
		m_graphTex->SetData(Vector2i(256, 1), graphPixels);
		m_graphTex->SetWrap(Graphics::TextureWrap::Clamp, Graphics::TextureWrap::Clamp);

		m_numPlayersSeen = m_stats->size();
		m_displayId = static_cast<String>((*m_stats)[m_displayIndex].value("uid", ""));

	}
	
	void SubmitScoreToIR(class Game* game) {
		// login to ir
		// TODO: remove this in favor of a class that handles this
		String bearer; bool err;
		std::tie(bearer, err) = LoginToIR();
		if (err == true) {return;}

		// get scoring info
		Scoring& m_scoring = game->GetScoring();

		// check if we can login
		String url = g_gameConfig.GetString(GameConfigKeys::IRBaseURL);

		// hash the file of the chart we played
		CryptoPP::SHA3_512 hash;
		String digest;

		CryptoPP::FileSource f(
			std::istringstream(game->GetChartIndex()->path),
			true,
			new CryptoPP::HashFilter(
				hash,
				new CryptoPP::HexEncoder(
					new CryptoPP::StringSink(digest), false)));

		// get the ids from the file hash
		auto res = nlohmann::json::parse(cpr::Get(cpr::Url{ url + "/api/v0/board/sha3/" + digest }).text);

		uint64 track_id = res["track_id"];
		uint64 board_id = res["id"];

		//auto replay = String(nlohmann::json::parse(m_simpleHitStats));

		// post the score
		nlohmann::json score_info = {
			{"track", track_id, },
			{"board", board_id, },
			{"score", m_scoring.CalculateCurrentScore(), },
			{"combo", m_scoring.maxComboCounter, },
			{"rate", m_scoring.currentGauge, },
			{"criticals", m_scoring.categorizedHits[2], },
			{"nears", m_scoring.categorizedHits[1] , },
			{"errors", m_scoring.categorizedHits[0], },
			{"mods", m_flags, },
			//{"replaydata", replay, },
		};
		cpr::Post(cpr::Url{ url + "/api/v0/score" },
			cpr::Body{ score_info.dump() },
			cpr::Header{ {"Content-Type", "application/json"},
						 {"Authorization", "Bearer "+bearer },
			});
	}

	std::pair<String, bool> LoginToIR() {
		// get internet ranking settings info
		String base_url = g_gameConfig.GetString(GameConfigKeys::IRBaseURL);
		String user = g_gameConfig.GetString(GameConfigKeys::IRUsername);
		String pass = g_gameConfig.GetString(GameConfigKeys::IRPassword);

		nlohmann::json login_creds = {
			{"username", user, },
			{"password", pass, },
		};

		// attempt to login
		cpr::Response resp = cpr::Post(cpr::Url{ base_url + "/api/v0/authorize/basic" },
			cpr::Body{ login_creds.dump() },
			cpr::Header{ {"Content-Type", "application/json"} });

		// on success
		if (resp.status_code == 202) {
			nlohmann::json tkns = nlohmann::json::parse(resp.text);
			String refresh = tkns["refresh"].get<std::string>(); // TODO: use this for heartbeats
			String bearer = tkns["bearer"].get<std::string>();
			return { bearer, false };
		}

		// on failure
		return { NULLPTR, true };
	}

	ScoreScreen_Impl(class Game* game, MultiplayerScreen* multiplayer,
		String uid, Vector<nlohmann::json> const* multistats)
	{
		m_displayIndex = 0;
		Scoring& scoring = game->GetScoring();
		m_autoplay = scoring.autoplay;
		m_autoButtons = scoring.autoplayButtons;

		if (ChartIndex* chart = game->GetChartIndex())
		{
			m_chartIndex = chart;
			m_highScores = chart->scores;
		}

		// XXX add data for multi
		m_gaugeSamples = game->GetGaugeSamples();

		m_multiplayer = multiplayer;

		if (m_multiplayer && multistats != nullptr)
		{
			m_stats = multistats;
			m_playerId = uid;

			// Show the player's score first
			for (size_t i=0; i<m_stats->size(); i++)
			{
				if (m_playerId == (*m_stats)[i].value("uid", ""))
				{
					m_displayIndex = i;
					break;
				}
			}

			loadScoresFromMultiplayer();
		}
		else
		{
			loadScoresFromGame(game);
		}

		// TODO: check if Internet Ranking is enabled
		SubmitScoreToIR(game);

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
		if (!m_autoplay && !m_autoButtons && game->GetChartIndex() && !game->GetManualExit())
		{
			ScoreIndex* newScore = new ScoreIndex();
			auto chart = game->GetChartIndex();
			String hash = chart->hash; //If chart file can't be opened, use existing hash.


			File chartFile;
			if (chartFile.OpenRead(chart->path))
			{
				char data_buffer[0x80];
				uint32_t digest[5];
				sha1::SHA1 s;

				size_t amount_read = 0;
				size_t read_size;
				do
				{
					read_size = chartFile.Read(data_buffer, sizeof(data_buffer));
					amount_read += read_size;
					s.processBytes(data_buffer, read_size);
				} while (read_size != 0);

				s.getDigest(digest);
				hash = Utility::Sprintf("%08x%08x%08x%08x%08x", digest[0], digest[1], digest[2], digest[3], digest[4]);
			}
			else 
			{
				Log("Couldn't open the chart file for hashing, using existing hash.", Logger::Severity::Warning);
			}

			Path::CreateDir(Path::Absolute("replays/" + hash));
			String replayPath = Path::Normalize(Path::Absolute("replays/" + chart->hash + "/" + Shared::Time::Now().ToString() + ".urf"));
			File replayFile;

			if (replayFile.OpenWrite(replayPath))
			{
				FileWriter fw(replayFile);
				fw.SerializeObject(m_simpleHitStats);
			}

			newScore->score = m_score;
			newScore->crit = m_categorizedHits[2];
			newScore->almost = m_categorizedHits[1];
			newScore->miss = m_categorizedHits[0];
			newScore->gauge = m_finalGaugeValue;
			newScore->gameflags = (uint32)m_flags;
			newScore->timestamp = Shared::Time::Now().Data();
			newScore->replayPath = replayPath;
			newScore->chartHash = hash;
			newScore->userName = g_gameConfig.GetString(GameConfigKeys::MultiplayerUsername);
			newScore->localScore = true;

			m_mapDatabase.AddScore(newScore);

		 	chart->scores.Add(newScore);
			chart->scores.Sort([](ScoreIndex* a, ScoreIndex* b)
			{
				return a->score > b->score;
			});
		}


		m_startPressed = false;

		// Used for jacket images
		m_beatmapSettings = game->GetBeatmap()->GetMapSettings();
		m_jacketPath = Path::Normalize(game->GetChartRootPath() + Path::sep + m_beatmapSettings.jacketPath);
		m_jacketImage = game->GetJacketImage();

	}
	~ScoreScreen_Impl()
	{
		g_input.OnButtonPressed.RemoveAll(this);

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
		if (m_multiplayer)
		{
			m_PushStringToTable("title", "<"+m_playerName+"> " + m_beatmapSettings.title);
		}
		else
		{
			m_PushStringToTable("title", m_beatmapSettings.title);
		}
		m_PushStringToTable("artist", m_beatmapSettings.artist);
		m_PushStringToTable("effector", m_beatmapSettings.effector);
		m_PushStringToTable("bpm", m_beatmapSettings.bpm);
		m_PushStringToTable("jacketPath", m_jacketPath);
		m_PushIntToTable("medianHitDelta", m_medianHitDelta[0]);
		m_PushFloatToTable("meanHitDelta", m_meanHitDelta[0]);
		m_PushIntToTable("medianHitDeltaAbs", m_medianHitDelta[1]);
		m_PushFloatToTable("meanHitDeltaAbs", m_meanHitDelta[1]);
		m_PushIntToTable("earlies", m_timedHits[0]);
		m_PushIntToTable("lates", m_timedHits[1]);
		m_PushStringToTable("grade", Scoring::CalculateGrade(m_score).c_str());
		m_PushIntToTable("badge", m_badge);

		if (m_multiplayer)
		{
			m_PushIntToTable("displayIndex", m_displayIndex);
			m_PushStringToTable("uid", m_playerId);
		}

		lua_pushstring(m_lua, "autoplay");
		lua_pushboolean(m_lua, m_autoplay);
		lua_settable(m_lua, -3);

		//Push gauge samples
		lua_pushstring(m_lua, "gaugeSamples");
		lua_newtable(m_lua);
		for (size_t i = 0; i < 256; i++)
		{
			lua_pushnumber(m_lua, m_gaugeSamples[i]);
			lua_rawseti(m_lua, -2, i + 1);
		}
		lua_settable(m_lua, -3);

		if (m_multiplayer)
		{
			// For multiplayer show other player's scores
			lua_pushstring(m_lua, "highScores");
			lua_newtable(m_lua);
			int scoreIndex = 1;
			for (auto& score : *m_stats)
			{
				lua_pushinteger(m_lua, scoreIndex++);
				lua_newtable(m_lua);
				m_PushFloatToTable("gauge", score["gauge"]);
				m_PushIntToTable("flags", score["flags"]);
				m_PushIntToTable("score", score["score"]);
				m_PushIntToTable("perfects", score["crit"]);
				m_PushIntToTable("goods", score["near"]);
				m_PushIntToTable("misses", score["miss"]);
				m_PushIntToTable("timestamp", 0);
				m_PushIntToTable("badge", score["clear"]);
				m_PushStringToTable("name", score["name"]);
				m_PushStringToTable("uid", score["uid"]);
				lua_settable(m_lua, -3);
			}
			lua_settable(m_lua, -3);
		}
		else
		{
			// For single player, just show highscores
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
		}

		///TODO: maybe push complete hit stats

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

		m_lua = g_application->LoadScript("result");
		if (!m_lua)
			return false;

		updateLuaData();
		m_collDiag.Init(&m_mapDatabase);
		g_input.OnButtonPressed.Add(this, &ScoreScreen_Impl::m_OnButtonPressed);

		return true;
	}
	bool Init() override
	{
		return true;
	}


	virtual void OnKeyPressed(SDL_Scancode code) override
	{
		if (m_multiplayer &&
				m_multiplayer->GetChatOverlay()->OnKeyPressedConsume(code))
			return;

		if (m_collDiag.IsActive())
			return;

		if(code == SDL_SCANCODE_RETURN && !m_removed)
		{
			g_application->RemoveTickable(this);
			m_removed = true;
		}
		if (code == SDL_SCANCODE_F12)
		{
			Capture();
		}
		if (code == SDL_SCANCODE_F9)
		{
			g_application->ReloadScript("result", m_lua);
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
	}
	virtual void OnKeyReleased(SDL_Scancode code) override
	{
	}
	virtual void Render(float deltaTime) override
	{
		lua_getglobal(m_lua, "render");
		lua_pushnumber(m_lua, deltaTime);
		lua_pushboolean(m_lua, m_showStats);
		if (lua_pcall(m_lua, 2, 0, 0) != 0)
		{
			Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
			assert(false);
		}
		m_hasRendered = true;
		if (m_collDiag.IsActive())
		{
			m_collDiag.Render(deltaTime);
		}

		if (m_multiplayer)
			m_multiplayer->GetChatOverlay()->Render(deltaTime);
	}
	virtual void Tick(float deltaTime) override
	{
		if (!m_hasScreenshot && m_hasRendered && !IsSuspended())
		{
			AutoScoreScreenshotSettings screensetting = g_gameConfig.GetEnum<Enum_AutoScoreScreenshotSettings>(GameConfigKeys::AutoScoreScreenshot);
			if (screensetting == AutoScoreScreenshotSettings::Always ||
				(screensetting == AutoScoreScreenshotSettings::Highscore && m_highScores.empty()) ||
				(screensetting == AutoScoreScreenshotSettings::Highscore && m_score > (uint32)m_highScores.front()->score))
			{
				Capture();
			}
			m_hasScreenshot = true;
		}


        

		m_showStats = g_input.GetButton(Input::Button::FX_0);

		// Check for new scores
		if (m_multiplayer && m_numPlayersSeen != (int)m_stats->size())
		{
			// Reselect the player we were looking at before
			for (size_t i = 0; i < m_stats->size(); i++)
			{
				if (m_displayId == static_cast<String>((*m_stats)[i].value("uid", "")))
				{
					m_displayIndex = i;
					break;
				}
			}

			loadScoresFromMultiplayer();
			updateLuaData();
		}

		if (m_collDiag.IsActive())
		{
			m_collDiag.Tick(deltaTime);
		}

		if (m_multiplayer)
			m_multiplayer->GetChatOverlay()->Tick(deltaTime);
	}

	virtual void OnSuspend()
	{
		m_restored = false;
	}
	virtual void OnRestore()
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

ScoreScreen* ScoreScreen::Create(class Game* game)
{
	ScoreScreen_Impl* impl = new ScoreScreen_Impl(game, NULL, String(""), nullptr);
	return impl;
}

ScoreScreen* ScoreScreen::Create(class Game* game, String uid, Vector<nlohmann::json> const* stats, MultiplayerScreen* multi)
{
	ScoreScreen_Impl* impl = new ScoreScreen_Impl(game, multi, uid, stats);
	return impl;
}
