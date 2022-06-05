#include "stdafx.h"
#include "Shared/LuaBindable.hpp"
#include "SkinIR.hpp"
#include "IR.hpp"

//this file mostly based on SkinHttp
//but it seems easier to me to have these be separate functions rather than updating the http requests to support json
//especially since that'd then require the skin to specify the authorization headers, json Content-Type, etc.
//this approach makes it easier for the skin creator

void SkinIR::m_PushArray(lua_State* L, const nlohmann::json& json)
{
	lua_newtable(L);
	int index = 1;

	for (auto& el : json.items())
	{
		lua_pushinteger(L, index++);
		m_PushJSON(L, el.value());
		lua_settable(L, -3);
	}
}

void SkinIR::m_PushObject(lua_State* L, const nlohmann::json& json)
{
	lua_newtable(L);


	for (auto& el : json.items())
	{
		lua_pushstring(L, el.key().c_str());
		m_PushJSON(L, el.value());
		lua_settable(L, -3);
	}
}

void SkinIR::m_PushJSON(lua_State* L, const nlohmann::json& json)
{
	if (json.is_array()) m_PushArray(L, json);
	else if (json.is_object()) m_PushObject(L, json);
	else if (json.is_boolean()) lua_pushboolean(L, json);
	else if (json.is_string()) lua_pushstring(L, json.get_ref<const std::string&>().c_str());
	else if (json.is_number()) lua_pushnumber(L, json);
	else if (json.is_null()) lua_pushnil(L);
}

void SkinIR::m_requestLoop()
{
	while (m_running)
	{
		m_mutex.lock(); ///TODO: use semaphore?
		if (m_requests.size() > 0)
		{
			//get request
			AsyncRequest* r = m_requests.front();
			m_mutex.unlock();

			//process request
			CompleteRequest cr;
			cr.L = r->L;
			cr.callback = r->callback;
			cr.r = r->r.get();

			//push result and pop request
			m_mutex.lock();
			delete r;
			m_callbackQueue.push(cr);
			m_requests.pop();
			m_mutex.unlock();
		}
		else
		{
			m_mutex.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
	}
}

void SkinIR::m_PushResponse(lua_State* L, const cpr::Response& r)
{
	if (r.status_code != 200)
	{
		Logf("Lua IR request failed with status code: %d", Logger::Severity::Warning, r.status_code);

		m_PushJSON(L, nlohmann::json{
			{"statusCode", 60},
			{"description", "The request to the IR failed."}
			});
	}
	else
	{
		try {
			nlohmann::json json = nlohmann::json::parse(r.text);

			if (!IR::ValidateReturn(json))
			{
				m_PushJSON(L, nlohmann::json{
					{"statusCode", 60},
					{"description", "The IR response was malformed."}
					});
			}
			else m_PushJSON(L, json);

		}
		catch (nlohmann::json::parse_error& e) {
			Log("Parsing JSON returned from IR failed.", Logger::Severity::Warning);

			m_PushJSON(L, nlohmann::json{
				{"statusCode", 60},
				{"description", "The IR response was malformed."}
				});
		}
	}
}


SkinIR::SkinIR()
{
	m_running = true;
	m_requestThread = Thread(&SkinIR::m_requestLoop, this);
}

SkinIR::~SkinIR()
{
	m_running = false;
	if (m_requestThread.joinable())
		m_requestThread.join();

	while (!m_requests.empty())
	{
		delete m_requests.front();
		m_requests.pop();
	}

	for (auto& s : m_boundStates)
	{
		delete s.second;
	}
	m_boundStates.clear();
}

int SkinIR::lHeartbeat(struct lua_State* L)
{
	int callback = luaL_ref(L, LUA_REGISTRYINDEX);
	AsyncRequest* r = new AsyncRequest();
	r->r = IR::Heartbeat();
	r->callback = callback;
	r->L = L;
	m_mutex.lock();
	m_requests.push(r);
	m_mutex.unlock();
	return 0;
}

int SkinIR::lChartTracked(struct lua_State* L)
{
	String hash = luaL_checkstring(L, 2);
	int callback = luaL_ref(L, LUA_REGISTRYINDEX);
	AsyncRequest* r = new AsyncRequest();
	r->r = IR::ChartTracked(hash);
	r->callback = callback;
	r->L = L;
	m_mutex.lock();
	m_requests.push(r);
	m_mutex.unlock();
	return 0;
}

int SkinIR::lRecord(struct lua_State* L)
{
	String hash = luaL_checkstring(L, 2);
	int callback = luaL_ref(L, LUA_REGISTRYINDEX);
	AsyncRequest* r = new AsyncRequest();
	r->r = IR::Record(hash);
	r->callback = callback;
	r->L = L;
	m_mutex.lock();
	m_requests.push(r);
	m_mutex.unlock();
	return 0;
}

int SkinIR::lLeaderboard(struct lua_State* L)
{
	String hash = luaL_checkstring(L, 2);
	String mode = luaL_checkstring(L, 3);
	int n = luaL_checkinteger(L, 4);
	int callback = luaL_ref(L, LUA_REGISTRYINDEX);
	AsyncRequest* r = new AsyncRequest();
	r->r = IR::Leaderboard(hash, mode, n);
	r->callback = callback;
	r->L = L;
	m_mutex.lock();
	m_requests.push(r);
	m_mutex.unlock();
	return 0;
}


void SkinIR::ProcessCallbacks()
{
	m_mutex.lock();
	if (m_callbackQueue.size() > 0)
	{
		//get response
		CompleteRequest& cr = m_callbackQueue.front();
		m_mutex.unlock();

		if (m_boundStates.Contains(cr.L))
		{
			//process response
			lua_rawgeti(cr.L, LUA_REGISTRYINDEX, cr.callback);
			m_PushResponse(cr.L, cr.r);
			if (lua_pcall(cr.L, 1, 0, 0) != 0)
			{
				Logf("Lua error on calling IR callback: %s", Logger::Severity::Error, lua_tostring(cr.L, -1));
			}
			lua_settop(cr.L, 0);
			luaL_unref(cr.L, LUA_REGISTRYINDEX, cr.callback);
		}
		//pop response
		m_mutex.lock();
		m_callbackQueue.pop();
	}
	m_mutex.unlock();
}

void SkinIR::PushFunctions(lua_State* L)
{
	auto bindable = new LuaBindable(L, "IR");
	bindable->AddFunction("Heartbeat", this, &SkinIR::lHeartbeat);
	bindable->AddFunction("ChartTracked", this, &SkinIR::lChartTracked);
	bindable->AddFunction("Record", this, &SkinIR::lRecord);
	bindable->AddFunction("Leaderboard", this, &SkinIR::lLeaderboard);
	bindable->Push();
	lua_settop(L, 0);
	m_boundStates.Add(L, bindable);
}

void SkinIR::ClearState(lua_State* L)
{
	if (!m_boundStates.Contains(L))
		return;
	delete m_boundStates.at(L);
	m_boundStates.erase(L);
}
