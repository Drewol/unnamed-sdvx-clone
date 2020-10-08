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