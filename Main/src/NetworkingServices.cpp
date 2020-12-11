#include "stdafx.h"
#include "NetworkingServices.hpp"
#include "GameConfig.hpp"
#include <json.hpp>
#include <cpr\cpr.h>
#include <chrono>
#include <thread>
#include "cryptopp/cryptlib.h"
#include "cryptopp/hex.h"
#include "cryptopp/sha3.h"
#include <cryptopp/filters.h>
#include <cryptopp/files.h>
#include "Game.hpp"
#include "Scoring.hpp"
#include "lua.hpp"
#include "Shared/LuaBindable.hpp"

NetworkingServices::NetworkingServices()
{
	m_isConnected = false;
	m_refreshToken = "";
	m_bearerToken = "";

	Logf("[NetworkingServices] Module Enabled", Logger::Severity::Normal);
}

void NetworkingServices::Init(GameConfig* config)
{
	m_serviceUrl = config->GetString(GameConfigKeys::IRBaseURL);
	m_username = config->GetString(GameConfigKeys::IRUsername);
	m_password = config->GetString(GameConfigKeys::IRPassword);

	Logf("[NetworkingServices] Module Initialized", Logger::Severity::Normal);
}

bool NetworkingServices::TryLogin()
{
	Logf("[NetworkingServices] Attempting to login", Logger::Severity::Normal);
	// assemble json request body data
	nlohmann::json login_creds = {
		{"username", m_username, },
		{"password", m_password, },
	};

	// attempt to login
	cpr::Response resp = cpr::Post(cpr::Url{ m_serviceUrl + "/api/v0/authorize/basic" },
		cpr::Body{ login_creds.dump() },
		cpr::Header{ {"Content-Type", "application/json"} });
	Logf("[NetworkingServices] Login request sent.", Logger::Severity::Normal);

	// on success
	if (resp.status_code == 202) {
		nlohmann::json tkns = nlohmann::json::parse(resp.text);
		m_refreshToken = tkns["refresh"].get<String>();
		m_bearerToken = tkns["bearer"].get<String>();
		Logf("[NetworkingServices] Logged In", Logger::Severity::Normal);
		return m_isConnected = true;
	}

	// on failure
	return m_isConnected = false;
}

bool NetworkingServices::Heartbeat()
{
	// assemble json request body data
	nlohmann::json login_creds = {
		{"refresh_token", m_refreshToken, },
	};

	// attempt to refresh credentials
	cpr::Response resp = cpr::Post(cpr::Url{ m_serviceUrl + "/api/v0/authorize/refresh" },
		cpr::Body{ login_creds.dump() },
		cpr::Header{ {"Content-Type", "application/json"} });

	// on success
	if (resp.status_code == 202) {
		nlohmann::json tkns = nlohmann::json::parse(resp.text);
		m_refreshToken = tkns["refresh"].get<String>();
		m_bearerToken = tkns["bearer"].get<String>();
		return m_isConnected = true;
	}

	// on failure
	return false;
}

void NetworkingServices::WaitForHeartbeat()
{
	m_heartbeat = std::this_thread::get_id();
	std::this_thread::sleep_for(std::chrono::minutes(2));
	NetworkingServices::Heartbeat();
	std::thread(&NetworkingServices::WaitForHeartbeat, this).detach();
}

void NetworkingServices::QueueHeartbeat()
{
	std::thread (&NetworkingServices::WaitForHeartbeat, this).detach();
}

bool NetworkingServices::ConnectionStatus()
{
	return m_isConnected;
}

bool NetworkingServices::SubmitScore(class Game* game, GameFlags m_flags)
{
	// get scoring info
	Scoring& m_scoring = game->GetScoring();

	// check if we can login
	String url = m_serviceUrl;

	// hash the file of the chart we played
	CryptoPP::SHA3_512 hash;
	String digest;

	CryptoPP::FileSource f(
		String(game->GetChartIndex()->path).c_str(),
		true,
		new CryptoPP::HashFilter(
			hash,
			new CryptoPP::HexEncoder(
				new CryptoPP::StringSink(digest), false)), true);

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
	cpr::Response resp = cpr::Post(cpr::Url{ url + "/api/v0/score" },
		cpr::Body{ score_info.dump() },
		cpr::Header{ {"Content-Type", "application/json"},
					 {"Authorization", "Bearer " + m_bearerToken },
		});
	if (resp.status_code == 200)
	{
		return true;
	}
	else 
	{
		return false;
	}
}

void m_PushStringToTable(lua_State* m_lua, const char* name, const char* data)
{
	lua_pushstring(m_lua, name);
	lua_pushstring(m_lua, data);
	lua_settable(m_lua, -3);
}

void m_PushFloatToTable(lua_State* m_lua, const char* name, float data)
{
	lua_pushstring(m_lua, name);
	lua_pushnumber(m_lua, data);
	lua_settable(m_lua, -3);
}

void m_PushIntToTable(lua_State* m_lua, const char* name, int data)
{
	lua_pushstring(m_lua, name);
	lua_pushinteger(m_lua, data);
	lua_settable(m_lua, -3);
}

int NetworkingServices::lGetScoresForTrack(lua_State* L)
{
	// TODO: cache the info so we aren't always wasting bandwidth
	// get track we are talking about
	String hash = luaL_checkstring(L, 2);
	cpr::Response respTrackInfo = cpr::Get(cpr::Url{ m_serviceUrl + "/api/v0/board/sha3/" + hash });
	if (respTrackInfo.status_code != 200)
	{
		return 0;
	}

	auto res = nlohmann::json::parse(respTrackInfo.text);
	uint64 track_id = res["track_id"];
	uint64 board_id = res["id"];

	uint8 limit = luaL_checkinteger(L, 3);
	uint16 offset = luaL_checkinteger(L, 4);

	// get scores from board
	cpr::Response respScoreList = cpr::Get(cpr::Url{ m_serviceUrl + 
												"/api/v0/score?track=" + std::to_string(track_id) + 
												"&board=" + std::to_string(board_id) + 
												"&limit=" + std::to_string(limit) + 
												"&offset=" + std::to_string(offset)});

	if (respScoreList.status_code != 200)
	{
		return 0;
	}

	lua_pushstring(L, "scores");
	lua_newtable(L);
	int idx = 0;
	for (auto scoreEntry : nlohmann::json::parse(respScoreList.text))
	{
		lua_pushinteger(L, ++idx);
		lua_newtable(L);
		uint64 profileID = scoreEntry["profile"];
		cpr::Response respPlayerInfo = cpr::Get(cpr::Url{ m_serviceUrl + "/api/v0/profile/" + std::to_string(profileID) });
		String playerName;
		if (respPlayerInfo.status_code != 200)
		{
			playerName = "UNINITALIZEZD";
		}
		else 
		{
			nlohmann::json resp = nlohmann::json::parse(respPlayerInfo.text);
			String pplayerName = resp["name"]; // QUEST: Why do I need to do this and can't assign to playerName directly??
			playerName = pplayerName;
		}
		String dateCreated = scoreEntry["date_created"];
		float perfRating;
		if (scoreEntry["performance"].is_null())
		{
			perfRating = 0;
		}
		else
		{
			perfRating = scoreEntry["performance"];
		}
		uint64 score;
		if (scoreEntry["score"].is_null())
		{
			score = 0;
		}
		else
		{
			score = scoreEntry["score"];
		}
		uint32 combo;
		if (scoreEntry["combo"].is_null())
		{
			combo = 0;
		}
		else
		{
			combo = scoreEntry["combo"];
		}
		uint8 status = scoreEntry["status"];
		float rate;
		if (scoreEntry["rate"].is_null())
		{
			rate = 0;
		}
		else
		{
			rate = scoreEntry["rate"];
		}
		double accuracy;
		if (scoreEntry["accuracy"].is_null())
		{
			accuracy = 0;
		}
		else
		{
			accuracy = scoreEntry["accuracy"];
		}
		uint32 crits;
		if (scoreEntry["criticals"].is_null())
		{
			crits = 0;
		}
		else
		{
			crits = scoreEntry["criticals"];
		}
		uint32 nears;
		if (scoreEntry["nears"].is_null())
		{
			nears = 0;
		}
		else
		{
			nears = scoreEntry["nears"];
		}
		uint32 errors;
		if (scoreEntry["errors"].is_null())
		{
			errors = 0;
		}
		else
		{
			errors = scoreEntry["errors"];
		}
		Logf("[NetworkingServices] Date Created: %s", Logger::Severity::Normal, dateCreated);
		Logf("[NetworkingServices] perfRating: %g", Logger::Severity::Normal, perfRating);
		Logf("[NetworkingServices] Score: %d", Logger::Severity::Normal, score);
		Logf("[NetworkingServices] Combo: %d", Logger::Severity::Normal, combo);
		Logf("[NetworkingServices] Status: %d", Logger::Severity::Normal, status);
		Logf("[NetworkingServices] Rate: %g", Logger::Severity::Normal, rate);
		Logf("[NetworkingServices] Accuracy: %g", Logger::Severity::Normal, accuracy);
		m_PushStringToTable(L, "player", playerName.c_str());
		m_PushStringToTable(L, "date_created", dateCreated.c_str());
		m_PushFloatToTable(L, "performance", perfRating);
		m_PushIntToTable(L, "score", score);
		m_PushIntToTable(L, "combo", combo);
		m_PushIntToTable(L, "status", status);
		m_PushFloatToTable(L, "rate", rate);
		m_PushFloatToTable(L, "score", accuracy);
		m_PushIntToTable(L, "criticals", crits);
		m_PushIntToTable(L, "nears", nears);
		m_PushIntToTable(L, "errors", errors);

		lua_settable(L, -3);
	}

	return 1;
}

void NetworkingServices::PushLuaFunctions(lua_State* L)
{
	auto bindable = new LuaBindable(L, "NetServ");
	bindable->AddFunction("GetScoresForTrack", this, &NetworkingServices::lGetScoresForTrack);
	bindable->Push();
	lua_settop(L, 0);
	//m_boundStates.Add(L, bindable);
}