#pragma once

#include <string>
#include <thread>
#include "GameConfig.hpp"
#include "Game.hpp"
#include "lua.hpp"

class NetworkingServices
{
public:
	NetworkingServices();
	void Init(GameConfig* config);
	bool TryLogin();
	void QueueHeartbeat();
	virtual bool SubmitScore(class Game* game, GameFlags m_flags);
	bool ConnectionStatus();
	void PushLuaFunctions(lua_State* L);
	int lGetScoresForTrack(lua_State* L);
	int lIsConnected(lua_State* L);

private:
	bool Heartbeat();
	void WaitForHeartbeat();

	// Configurable
	String m_serviceUrl;
	String m_username;
	String m_password;

	// Connection Dependant
	bool m_isConnected;
	String m_refreshToken;
	String m_bearerToken;

	// Static
	std::thread::id m_heartbeat;

	// Lua
	Map<struct lua_State*, class LuaBindable*> m_boundStates;
};

extern class NetworkingServices;