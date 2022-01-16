#include "stdafx.h"
#include "ScoreScreen.hpp"
#include "Application.hpp"
#include "GameConfig.hpp"
#include <Audio/Audio.hpp>
#include <Beatmap/MapDatabase.hpp>
#include "Scoring.hpp"
#include "Game.hpp"
#include "AsyncAssetLoader.hpp"
#include "ChallengeSelect.hpp"
#include "lua.hpp"
#include "Shared/Time.hpp"
#include "json.hpp"
#include "CollectionDialog.hpp"
#include <Beatmap/TinySHA1.hpp>
#include "MultiplayerScreen.hpp"
#include "ChatOverlay.hpp"
#include "Gauge.hpp"
#include "IR.hpp"
#include <future>
#include <chrono>

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
	ClearMark m_badge;
	uint32 m_score;
	uint32 m_maxCombo;
	uint32 m_categorizedHits[3];
	float m_finalGaugeValue;
	std::array<float, 256> m_gaugeSamples;
	String m_jacketPath;
	uint32 m_timedHits[2];
	int m_irState = IR::ResponseState::Unused;
	String m_chartHash;

	//promote this to higher scope so i can use it in tick
	String m_replayPath;

	cpr::AsyncResponse m_irResponse;
	nlohmann::json m_irResponseJson;

	HitWindow m_hitWindow = HitWindow::NORMAL;

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
	int m_selfDisplayIndex = 0;
	Vector<nlohmann::json> const* m_stats;
	int m_numPlayersSeen = 0;

	ChallengeManager* m_challengeManager;

	String m_mission = "";
	int m_retryCount = 0;
	float m_playbackSpeed = 1.0f;

	Vector<ScoreIndex*> m_highScores;
	Vector<SimpleHitStat> m_simpleHitStats;
	Vector<SimpleHitStat> m_simpleNoteHitStats; ///< For notes only

	// For scaling simpleHitStats
	MapTime m_beatmapDuration = 0;

	BeatmapSettings m_beatmapSettings;
	Texture m_jacketImage;
	PlaybackOptions m_options;
	GaugeType m_gaugeType;
	uint32 m_gaugeOption;
	CollectionDialog m_collDiag;
	ChartIndex* m_chartIndex;

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
				m_collDiag.Open(m_chartIndex);
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

		switch (button) {
			case Input::Button::Result_Continue:
				g_application->RemoveTickable(this);
				m_removed = true;
				break;
			case Input::Button::Result_Screenshot:
				Capture();
				break;
			case Input::Button::ReloadSkin:
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
				break;
		}
	}

	void m_PushIRScores()
	{
		lua_pushstring(m_lua, "irScores");
		lua_newtable(m_lua);
		int scoreIndex = 1;

		//we don't need to display the server record separately if we just set it
		//we also don't need to display the server record separately if our PB is the server record
		if(!m_irResponseJson["body"]["isServerRecord"] && m_irResponseJson["body"]["serverRecord"]["score"] != m_irResponseJson["body"]["score"]["score"])
		{
			auto& record = m_irResponseJson["body"]["serverRecord"];

			m_PushIRScoreToTable(scoreIndex++, record, false);
		}

		//scores above ours
		for (auto& scoreA : m_irResponseJson["body"]["adjacentAbove"].items())
			m_PushIRScoreToTable(scoreIndex++, scoreA.value(), false);


		//our score
		auto& ours = m_irResponseJson["body"]["score"];

		m_PushIRScoreToTable(scoreIndex++, ours, true);

		//scores below ours
		for (auto& scoreB : m_irResponseJson["body"]["adjacentBelow"].items())
			m_PushIRScoreToTable(scoreIndex++, scoreB.value(), false);

		lua_settable(m_lua, -3);
	}

	void m_PushIRScoreToTable(int i, nlohmann::json& score, bool yours)
	{
		lua_pushinteger(m_lua, i);
		lua_newtable(m_lua);
		m_PushIntToTable("score", score["score"]);
		m_PushIntToTable("crit", score["crit"]);
		m_PushIntToTable("near", score["near"]);
		m_PushIntToTable("error", score["error"]);
		m_PushIntToTable("lamp", score["lamp"]);
		m_PushIntToTable("ranking", score["ranking"]);
		m_PushIntToTable("timestamp", score["timestamp"]);
		m_PushStringToTable("username", score["username"]);

		if(yours)
		{
			lua_pushstring(m_lua, "yours");
			lua_pushboolean(m_lua, true);
			lua_settable(m_lua, -3);
			lua_pushstring(m_lua, "justSet");
			lua_pushboolean(m_lua, m_irResponseJson["body"]["isPB"]);
			lua_settable(m_lua, -3);
		}

		lua_settable(m_lua, -3);
	}

	void m_AddNewScore(class Game* game)
	{
		ScoreIndex* newScore = new ScoreIndex();
		ChartIndex* chart = game->GetChartIndex();

		// If chart file can't be opened, use existing hash.
		String hash = chart->hash;

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

		m_chartHash = hash;

		Path::CreateDir(Path::Absolute("replays/" + hash));
		m_replayPath = Path::Normalize(Path::Absolute("replays/" + chart->hash + "/" + Shared::Time::Now().ToString() + ".urf"));
		File replayFile;

		if (replayFile.OpenWrite(m_replayPath))
		{
			FileWriter fw(replayFile);
			fw.SerializeObject(m_simpleHitStats);
			fw.Serialize(&(m_hitWindow.perfect), 4);
			fw.Serialize(&(m_hitWindow.good), 4);
			fw.Serialize(&(m_hitWindow.hold), 4);
			fw.Serialize(&(m_hitWindow.miss), 4);
			fw.Serialize(&m_hitWindow.slam, 4);
		}

		newScore->score = m_score;
		newScore->crit = m_categorizedHits[2];
		newScore->almost = m_categorizedHits[1];
		newScore->miss = m_categorizedHits[0];
		newScore->gauge = m_finalGaugeValue;

		newScore->gaugeType = m_gaugeType;
		newScore->gaugeOption = m_gaugeOption;
		newScore->mirror = m_options.mirror;
		newScore->random = m_options.random;
		newScore->autoFlags = m_options.autoFlags;

		newScore->timestamp = Shared::Time::Now().Data();
		newScore->replayPath = m_replayPath;
		newScore->chartHash = hash;
		newScore->userName = g_gameConfig.GetString(GameConfigKeys::MultiplayerUsername);
		newScore->localScore = true;

		newScore->hitWindowPerfect = m_hitWindow.perfect;
		newScore->hitWindowGood = m_hitWindow.good;
		newScore->hitWindowHold = m_hitWindow.hold;
		newScore->hitWindowMiss = m_hitWindow.miss;
		newScore->hitWindowSlam = m_hitWindow.slam;

		m_mapDatabase.AddScore(newScore);

		if (g_gameConfig.GetString(GameConfigKeys::IRBaseURL) != "")
		{
			m_irState = IR::ResponseState::Pending;
			m_irResponse = IR::PostScore(*newScore, m_beatmapSettings);
		}

		const bool cleared = Scoring::CalculateBadge(*newScore) >= ClearMark::NormalClear;
		const bool wholeChartPlayed = cleared || newScore->gaugeType == GaugeType::Normal;

		bool firstClear = cleared;
		bool firstPlayWholeChart = wholeChartPlayed;
		bool firstPlay = chart->scores.empty();

		if (wholeChartPlayed)
		{
			for (const ScoreIndex* oldScore : chart->scores)
			{
				if (oldScore->gaugeType == GaugeType::Normal)
				{
					firstPlayWholeChart = false;
				}

				const ClearMark clearMark = Scoring::CalculateBadge(*oldScore);
				if (clearMark >= ClearMark::NormalClear)
				{
					firstPlayWholeChart = false;
					firstClear = false;
					break;
				}
			}
		}

		chart->scores.Add(newScore);
		chart->scores.Sort([](ScoreIndex* a, ScoreIndex* b)
			{
				return a->score > b->score;
			});

		// Update chart song offset
		bool updateSongOffset = false;
		switch (g_gameConfig.GetEnum<Enum_SongOffsetUpdateMethod>(GameConfigKeys::UpdateSongOffsetAfterFirstPlay))
		{
		case SongOffsetUpdateMethod::None:
			updateSongOffset = false;
			break;
		case SongOffsetUpdateMethod::Play:
			updateSongOffset = firstPlay;
			break;
		case SongOffsetUpdateMethod::PlayWholeChart:
			updateSongOffset = firstPlayWholeChart;
			break;
		case SongOffsetUpdateMethod::Clear:
			updateSongOffset = firstClear;
			break;
		default:
			break;
		}

		if (!updateSongOffset)
		{
			switch (g_gameConfig.GetEnum<Enum_SongOffsetUpdateMethod>(GameConfigKeys::UpdateSongOffsetAfterEveryPlay))
			{
			case SongOffsetUpdateMethod::None:
				updateSongOffset = false;
				break;
			case SongOffsetUpdateMethod::Play:
				updateSongOffset = true;
				break;
			case SongOffsetUpdateMethod::PlayWholeChart:
				updateSongOffset = wholeChartPlayed;
				break;
			case SongOffsetUpdateMethod::Clear:
				updateSongOffset = cleared;
				break;
			default:
				break;
			}
		}

		if (updateSongOffset)
		{
			const int oldOffset = chart->custom_offset;
			chart->custom_offset = oldOffset + m_medianHitDelta[0];

			Logf("Updating song offset %d -> %d based on hitstat", Logger::Severity::Info, oldOffset, chart->custom_offset);
			m_mapDatabase.UpdateChartOffset(chart);
		}
	}

public:

	void loadScoresFromGame(class Game* game)
	{
		Scoring& scoring = game->GetScoring();
		Gauge* gauge = scoring.GetTopGauge();
		// Calculate hitstats
		memcpy(m_categorizedHits, scoring.categorizedHits, sizeof(scoring.categorizedHits));
		m_score = scoring.CalculateCurrentScore();
		m_maxCombo = scoring.maxComboCounter;
		m_finalGaugeValue = gauge->GetValue();
		m_gaugeOption = gauge->GetOpts();
		m_gaugeType = gauge->GetType();
		m_timedHits[0] = scoring.timedHits[0];
		m_timedHits[1] = scoring.timedHits[1];
		m_options = game->GetPlaybackOptions();
		m_scoredata.score = m_score;
		memcpy(m_categorizedHits, scoring.categorizedHits, sizeof(scoring.categorizedHits));
		m_scoredata.crit = m_categorizedHits[2];
		m_scoredata.almost = m_categorizedHits[1];
		m_scoredata.miss = m_categorizedHits[0];
		m_scoredata.gauge = m_finalGaugeValue;
		m_scoredata.gaugeType = m_gaugeType;
		m_scoredata.gaugeOption = m_gaugeOption;
		m_scoredata.mirror = m_options.mirror;
		m_scoredata.random = m_options.random;
		m_scoredata.autoFlags = m_options.autoFlags;
		if (!game->IsStorableScore())
		{
			m_badge = ClearMark::NotPlayed;
		}
		else
		{
			m_badge = Scoring::CalculateBadge(m_scoredata);
		}

		m_playbackSpeed = game->GetPlayOptions().playbackSpeed;

		m_retryCount = game->GetRetryCount();
		m_mission = game->GetMissionStr();

		m_meanHitDelta[0] = scoring.GetMeanHitDelta();
		m_medianHitDelta[0] = scoring.GetMedianHitDelta();

		m_meanHitDelta[1] = scoring.GetMeanHitDelta(true);
		m_medianHitDelta[1] = scoring.GetMedianHitDelta(true);

		m_hitWindow = scoring.hitWindow;
	}

	void loadScoresFromMultiplayer() {
		if (m_displayIndex >= (int)m_stats->size())
			return;

		const nlohmann::json& data= (*m_stats)[m_displayIndex];

		//TODO(gauge refactor): options are from flags, multi server needs update for the new options

		uint32 flags = data["flags"];
		m_options = PlaybackOptions::FromFlags(flags);

		m_gaugeType = m_options.gaugeType;
		m_gaugeOption = m_options.gaugeOption;

		m_score = data["score"];
		m_maxCombo = data["combo"];
		m_finalGaugeValue = data["gauge"];
		m_timedHits[0] = data["early"];
		m_timedHits[1] = data["late"];


		m_categorizedHits[0] = data["miss"];
		m_categorizedHits[1] = data["near"];
		m_categorizedHits[2] = data["crit"];

		m_scoredata.score = data["score"];
		m_scoredata.crit = m_categorizedHits[2];
		m_scoredata.almost = m_categorizedHits[1];
		m_scoredata.miss = m_categorizedHits[0];
		m_scoredata.gauge = m_finalGaugeValue;
		m_scoredata.gaugeType = m_gaugeType;
		m_scoredata.gaugeOption = m_gaugeOption;
		m_scoredata.mirror = m_options.mirror;
		m_scoredata.random = m_options.random;
		m_scoredata.autoFlags = m_options.autoFlags;

		m_badge = static_cast<ClearMark>(data["clear"]);

		m_meanHitDelta[0] = data["mean_delta"];
		m_medianHitDelta[0] = data["median_delta"];

		m_playerName = static_cast<String>(data.value("name",""));

		auto samples = data["graph"];


		Colori graphPixels[256];
		for (uint32 i = 0; i < 256; i++)
		{
			m_gaugeSamples[i] = samples[i].get<float>();
		}

		m_numPlayersSeen = m_stats->size();
		m_displayId = static_cast<String>((*m_stats)[m_displayIndex].value("uid",""));
	}

	ScoreScreen_Impl(class Game* game, MultiplayerScreen* multiplayer,
		String uid, Vector<nlohmann::json> const* multistats, ChallengeManager* manager)
	{
		m_challengeManager = manager;
		m_displayIndex = 0;
		m_selfDisplayIndex = 0;
		Scoring& scoring = game->GetScoring();
		m_autoplay = scoring.autoplayInfo.autoplay;
		m_autoButtons = scoring.autoplayInfo.autoplayButtons;

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
					m_selfDisplayIndex = i;
					break;
				}
			}

			loadScoresFromMultiplayer();
		}
		else
		{
			loadScoresFromGame(game);
		}

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

			if (stat->object && stat->object->type == ObjectType::Single)
			{
				m_simpleNoteHitStats.Add(shs);
			}
			else {
				assert(shs.lane >= 6 || shs.hold > 0);
			}
		}

		//this has been moved to the top so that it is instantiated in time for IR submission

		m_beatmapDuration = game->GetBeatmap()->GetLastObjectTime();

		// Used for jacket images
		m_beatmapSettings = game->GetBeatmap()->GetMapSettings();
		m_jacketPath = Path::Normalize(game->GetChartRootPath() + Path::sep + m_beatmapSettings.jacketPath);
		m_jacketImage = game->GetJacketImage();

		// Don't save the score if autoplay was on or if the song was launched using command line
		// also don't save the score if the song was manually exited
		if (!m_autoplay && !m_autoButtons && game->GetChartIndex() && game->IsStorableScore())
		{
			m_AddNewScore(game);
		}

		m_startPressed = false;

		if (m_challengeManager != nullptr)
		{
			// Save some score screen info for challenge results later
			ChallengeResult& res = m_challengeManager->GetCurrentResultForUpdating();
			res.scorescreenInfo.difficulty = m_beatmapSettings.difficulty;
			res.scorescreenInfo.beatmapDuration = m_beatmapDuration;
			res.scorescreenInfo.medianHitDelta = m_medianHitDelta[0];
			res.scorescreenInfo.meanHitDelta = m_meanHitDelta[0];
			res.scorescreenInfo.medianHitDeltaAbs = m_medianHitDelta[1];
			res.scorescreenInfo.meanHitDeltaAbs = m_meanHitDelta[1];
			res.scorescreenInfo.earlies = m_timedHits[0];
			res.scorescreenInfo.lates = m_timedHits[1];
			res.scorescreenInfo.hitWindow = m_hitWindow;
			res.scorescreenInfo.showCover = g_gameConfig.GetBool(GameConfigKeys::ShowCover);
			res.scorescreenInfo.suddenCutoff = g_gameConfig.GetFloat(GameConfigKeys::SuddenCutoff);
			res.scorescreenInfo.hiddenCutoff = g_gameConfig.GetFloat(GameConfigKeys::HiddenCutoff);
			res.scorescreenInfo.suddenFade = g_gameConfig.GetFloat(GameConfigKeys::SuddenFade);
			res.scorescreenInfo.hiddenFade =  g_gameConfig.GetFloat(GameConfigKeys::HiddenFade);
			res.scorescreenInfo.simpleNoteHitStats = m_simpleNoteHitStats;
			SpeedMods speedMod = g_gameConfig.GetEnum<Enum_SpeedMods>(GameConfigKeys::SpeedMod);
			res.scorescreenInfo.speedMod = static_cast<int>(speedMod);
			res.scorescreenInfo.speedModValue = g_gameConfig.GetFloat(speedMod == SpeedMods::XMod ? GameConfigKeys::HiSpeed : GameConfigKeys::ModSpeed);
			memcpy(res.scorescreenInfo.gaugeSamples, m_gaugeSamples.data(), sizeof(res.scorescreenInfo.gaugeSamples));
		}

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
		const bool isSelf = m_displayIndex == m_selfDisplayIndex;

		lua_newtable(m_lua);
		m_PushIntToTable("score", m_score);

		m_PushIntToTable("gauge_type", (uint32)m_gaugeType);
		m_PushIntToTable("gauge_option", m_gaugeOption);

		lua_pushstring(m_lua, "random");
		lua_pushboolean(m_lua, m_options.random);
		lua_settable(m_lua, -3);

		lua_pushstring(m_lua, "mirror");
		lua_pushboolean(m_lua, m_options.mirror);
		lua_settable(m_lua, -3);

		m_PushIntToTable("auto_flags", (uint32)m_options.autoFlags);

		m_PushFloatToTable("gauge", m_finalGaugeValue);
		m_PushIntToTable("misses", m_categorizedHits[0]);
		m_PushIntToTable("goods", m_categorizedHits[1]);
		m_PushIntToTable("perfects", m_categorizedHits[2]);
		m_PushIntToTable("maxCombo", m_maxCombo);
		m_PushIntToTable("level", m_beatmapSettings.level);
		m_PushIntToTable("difficulty", m_beatmapSettings.difficulty);
		if (m_multiplayer)
		{
			m_PushStringToTable("playerName", m_playerName);
			m_PushStringToTable("title", "<"+m_playerName+"> " + m_beatmapSettings.title);
		}
		else
		{
			m_PushStringToTable("title", m_beatmapSettings.title);
		}
		lua_pushstring(m_lua, "isSelf");
		lua_pushboolean(m_lua, isSelf);
		lua_settable(m_lua, -3);
		m_PushStringToTable("realTitle", m_beatmapSettings.title);
		m_PushStringToTable("artist", m_beatmapSettings.artist);
		m_PushStringToTable("effector", m_beatmapSettings.effector);
		m_PushStringToTable("illustrator", m_beatmapSettings.illustrator);

		m_PushStringToTable("bpm", m_beatmapSettings.bpm);
		m_PushIntToTable("duration", m_beatmapDuration);

		m_PushStringToTable("jacketPath", m_jacketPath);
		m_PushIntToTable("medianHitDelta", m_medianHitDelta[0]);
		m_PushFloatToTable("meanHitDelta", m_meanHitDelta[0]);
		m_PushIntToTable("medianHitDeltaAbs", m_medianHitDelta[1]);
		m_PushFloatToTable("meanHitDeltaAbs", m_meanHitDelta[1]);
		m_PushIntToTable("earlies", m_timedHits[0]);
		m_PushIntToTable("lates", m_timedHits[1]);
		m_PushStringToTable("grade", ToDisplayString(ToGradeMark(m_score)));
		m_PushIntToTable("badge", static_cast<int>(m_badge));

		if (m_multiplayer)
		{
			m_PushIntToTable("displayIndex", m_displayIndex);
			m_PushStringToTable("uid", m_playerId);
		}

		lua_pushstring(m_lua, "autoplay");
		lua_pushboolean(m_lua, m_autoplay);
		lua_settable(m_lua, -3);

		m_PushIntToTable("irState", m_irState);
		m_PushStringToTable("chartHash", m_chartHash);

		//description (for displaying any errors, etc)
		if(m_irState >= 20)
		{
			if(m_irState == IR::ResponseState::RequestFailure)
				m_PushStringToTable("irDescription", "The request to the IR failed.");
			else
				m_PushStringToTable("irDescription", m_irResponseJson["description"]);
		}


		m_PushFloatToTable("playbackSpeed", m_playbackSpeed);

		m_PushStringToTable("mission", m_mission);
		m_PushIntToTable("retryCount", m_retryCount);

		lua_pushstring(m_lua, "hitWindow");
		m_hitWindow.ToLuaTable(m_lua);
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
		}

		//ir scores moved to be in multiplayer too, not yet tested
		if(m_irState == IR::ResponseState::Success)
			m_PushIRScores();

		if (isSelf)
		{
			SpeedMods speedMod = g_gameConfig.GetEnum<Enum_SpeedMods>(GameConfigKeys::SpeedMod);
			m_PushIntToTable("speedModType", static_cast<int>(speedMod));
			m_PushFloatToTable("speedModValue", g_gameConfig.GetFloat(speedMod == SpeedMods::XMod ? GameConfigKeys::HiSpeed : GameConfigKeys::ModSpeed));

			if (g_gameConfig.GetBool(GameConfigKeys::EnableHiddenSudden)) {
				lua_pushstring(m_lua, "hidsud");
				lua_newtable(m_lua);

				lua_pushstring(m_lua, "showCover");
				lua_pushboolean(m_lua, g_gameConfig.GetBool(GameConfigKeys::ShowCover));
				lua_settable(m_lua, -3);

				m_PushFloatToTable("suddenCutoff", g_gameConfig.GetFloat(GameConfigKeys::SuddenCutoff));
				m_PushFloatToTable("hiddenCutoff", g_gameConfig.GetFloat(GameConfigKeys::HiddenCutoff));
				m_PushFloatToTable("suddenFade", g_gameConfig.GetFloat(GameConfigKeys::SuddenFade));
				m_PushFloatToTable("hiddenFade", g_gameConfig.GetFloat(GameConfigKeys::HiddenFade));

				lua_settable(m_lua, -3);
			}

			lua_pushstring(m_lua, "noteHitStats");
			lua_newtable(m_lua);
			for (size_t i = 0; i < m_simpleNoteHitStats.size(); ++i)
			{
				const SimpleHitStat simpleHitStat = m_simpleNoteHitStats[i];

				lua_newtable(m_lua);
				m_PushIntToTable("rating", simpleHitStat.rating);
				m_PushIntToTable("lane", simpleHitStat.lane);
				m_PushIntToTable("time", simpleHitStat.time);
				m_PushFloatToTable("timeFrac",
					Math::Clamp(static_cast<float>(simpleHitStat.time) / (m_beatmapDuration > 0 ? m_beatmapDuration : 1), 0.0f, 1.0f));
				m_PushIntToTable("delta", simpleHitStat.delta);
				m_PushIntToTable("hold", 0);

				lua_rawseti(m_lua, -2, i + 1);
			}
			lua_settable(m_lua, -3);

			int index = 1;
			lua_pushstring(m_lua, "holdHitStats");
			lua_newtable(m_lua);
			for (size_t i = 0; i < m_simpleHitStats.size(); ++i)
			{
				const SimpleHitStat simpleHitStat = m_simpleHitStats[i];
				if (simpleHitStat.hold == 0)
					continue;

				lua_newtable(m_lua);
				m_PushIntToTable("rating", simpleHitStat.rating);
				m_PushIntToTable("lane", simpleHitStat.lane);
				m_PushIntToTable("time", simpleHitStat.time);
				m_PushFloatToTable("timeFrac",
					Math::Clamp(static_cast<float>(simpleHitStat.time) / (m_beatmapDuration > 0 ? m_beatmapDuration : 1), 0.0f, 1.0f));
				m_PushIntToTable("delta", simpleHitStat.delta);
				m_PushIntToTable("hold", simpleHitStat.hold);

				lua_rawseti(m_lua, -2, index++);
			}
			lua_settable(m_lua, -3);

			index = 1;
			lua_pushstring(m_lua, "laserHitStats");
			lua_newtable(m_lua);
			for (size_t i = 0; i < m_simpleHitStats.size(); ++i)
			{
				const SimpleHitStat simpleHitStat = m_simpleHitStats[i];
				if (simpleHitStat.lane < 6)
					continue;

				lua_newtable(m_lua);
				m_PushIntToTable("rating", simpleHitStat.rating);
				m_PushIntToTable("lane", simpleHitStat.lane);
				m_PushIntToTable("time", simpleHitStat.time);
				m_PushFloatToTable("timeFrac",
					Math::Clamp(static_cast<float>(simpleHitStat.time) / (m_beatmapDuration > 0 ? m_beatmapDuration : 1), 0.0f, 1.0f));
				m_PushIntToTable("delta", simpleHitStat.delta);
				m_PushIntToTable("hold", 0);

				lua_rawseti(m_lua, -2, index++);
			}
			lua_settable(m_lua, -3);

		}

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
		lua_pushboolean(m_lua, m_showStats);
		if (lua_pcall(m_lua, 2, 0, 0) != 0)
		{
			if (!g_application->ScriptError("result", m_lua)) {
				g_application->RemoveTickable(this);
			}
		}
		m_hasRendered = true;
		if (m_collDiag.IsActive())
		{
			m_collDiag.Render(deltaTime);
		}

		if (m_multiplayer)
			m_multiplayer->GetChatOverlay()->Render(deltaTime);
	}
	void Tick(float deltaTime) override
	{
		if (!m_hasScreenshot && m_hasRendered && !IsSuspended())
		{
			AutoScoreScreenshotSettings screensetting = g_gameConfig.GetEnum<Enum_AutoScoreScreenshotSettings>(GameConfigKeys::AutoScoreScreenshot);
			if (screensetting == AutoScoreScreenshotSettings::Always ||
				(screensetting == AutoScoreScreenshotSettings::Highscore && m_highScores.empty()) ||
				(screensetting == AutoScoreScreenshotSettings::Highscore && m_score > (uint32)m_highScores.front()->score))
			{
				if (!m_autoplay && !m_autoButtons)
				{
					Capture();
				}
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

		//handle ir score submission request
		if (m_irState == IR::ResponseState::Pending)
		{
			try {


				if(m_irResponse.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
				{
					cpr::Response response = m_irResponse.get();

		        	if(response.status_code != 200)
					{
						Logf("Submitting score to IR failed with code: %d", Logger::Severity::Error, response.status_code);
						m_irState = IR::ResponseState::RequestFailure;
					}
					else
					{
						try {
							m_irResponseJson = nlohmann::json::parse(response.text);

							if(!IR::ValidatePostScoreReturn(m_irResponseJson)) m_irState = IR::ResponseState::RequestFailure;
							else
							{
								m_irState = m_irResponseJson["statusCode"];

								//if we are allowed to send replays
								if(!g_gameConfig.GetBool(GameConfigKeys::IRLowBandwidth))
								{
									//and server wants us to send replay
									if(m_irResponseJson["body"].find("sendReplay") != m_irResponseJson["body"].end() && m_irResponseJson["body"]["sendReplay"].is_string())
									{
										//don't really care about the return of this, if it fails it's not the end of the world
										IR::PostReplay(m_irResponseJson["body"]["sendReplay"].get<String>(), m_replayPath).get();
									}
								}
							}


						} catch(nlohmann::json::parse_error& e) {
							Log("Parsing JSON returned from IR failed.", Logger::Severity::Error);
						}
					}

					updateLuaData();
				}

			} catch(std::future_error& e) {
				Logf("future_error when submitting score to IR: %s", Logger::Severity::Error, e.what());
			}
		}
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
		String screenshotPath = Path::Absolute("screenshots/" + Shared::Time::Now().ToString() + ".png");
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
	ScoreScreen_Impl* impl = new ScoreScreen_Impl(game, nullptr, "", nullptr, nullptr);
	return impl;
}

ScoreScreen* ScoreScreen::Create(class Game* game, ChallengeManager* manager)
{
	ScoreScreen_Impl* impl = new ScoreScreen_Impl(game, nullptr, "", nullptr, manager);
	return impl;
}

ScoreScreen* ScoreScreen::Create(class Game* game, String uid, Vector<nlohmann::json> const* stats, MultiplayerScreen* multi)
{
	ScoreScreen_Impl* impl = new ScoreScreen_Impl(game, multi, uid, stats, nullptr);
	return impl;
}
