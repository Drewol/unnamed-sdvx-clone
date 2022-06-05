#pragma once
#include "LuaRequests.hpp"

//mostly based on SkinHttp, explained in .cpp

//structs have been moved into shared LuaRequests header to stop IR from including Http or vice versa

class SkinIR
{
public:
	SkinIR();
	~SkinIR();
	int lHeartbeat(struct lua_State* L);
	int lChartTracked(struct lua_State* L);
	int lRecord(struct lua_State* L);
	int lLeaderboard(struct lua_State* L);
	void ProcessCallbacks();
	void PushFunctions(struct lua_State* L);
	void ClearState(struct lua_State* L);

private:
	Thread m_requestThread;
	Mutex m_mutex;
	std::queue<AsyncRequest*> m_requests;
	std::queue<CompleteRequest> m_callbackQueue;
	bool m_running;
	Map<struct lua_State*, class LuaBindable*> m_boundStates;

	void m_requestLoop();
	void m_PushResponse(struct lua_State* L, const cpr::Response& r);
	void m_PushJSON(struct lua_State* L, const nlohmann::json& json);
	void m_PushArray(struct lua_State* L, const nlohmann::json& json);
	void m_PushObject(struct lua_State* L, const nlohmann::json& json);
};
