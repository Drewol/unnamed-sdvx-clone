#pragma once
#include "stdafx.h"
#include "Shared/Thread.hpp"
#include "cpr/cpr.h"
#include "json.hpp"
#include <stack>

#ifdef _WIN32
/* See http://stackoverflow.com/questions/12765743/getaddrinfo-on-win32 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501  /* Windows XP. */
#endif
#include <winsock2.h>
#include <Ws2tcpip.h>
// XXX should be tracked elsewhere (not in source)?
#pragma comment( lib, "ws2_32.lib")

#define invalid_socket(s) (s == INVALID_SOCKET)
#else
/* Assume that any non-Windows platform uses POSIX-style sockets instead. */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>  /* Needed for getaddrinfo() and freeaddrinfo() */
#include <unistd.h> /* Needed for close() */

#define INVALID_SOCKET -1

#define invalid_socket(s) (s < 0)

typedef int SOCKET;
#endif

enum TCPPacketMode
{
	NOT_READING = 0,
	JSON_LINE,
	UNKNOWN
};

struct LuaTCPHandler
{
	struct lua_State* L;
	int callback;
};

class TCPSocket
{
public:
	TCPSocket();
	~TCPSocket();

	int lConnect(struct lua_State* L);
	int lSendLine(struct lua_State* L);
	int lClose(struct lua_State* L);
	int lSetTopicHandler(struct lua_State* L);

	void SendLine(String);
	void SendJSON(nlohmann::json packet);

	void PushFunctions(struct lua_State* L);
	void ClearState(struct lua_State* L);

	void ProcessSocket();

	bool Connect(String host);
	void Close();

	// Add a new bound member function for a given topic
	template<typename Class>
	void SetTopicHandler(String topic, Class* object, bool (Class::* func)(nlohmann::json&))
	{
		auto binding = new ObjectBinding<Class, bool, nlohmann::json&>(object, func);
		m_topicHandlers.Add(topic, binding);
	}

	// Add a bound member function to call on socket close
	template<typename Class>
	void SetCloseHandler(Class* object, void (Class::* func)(void))
	{
		m_closeCallback = new ObjectBinding<Class, void>(object, func);
	}

	// Clear handles for a given topic
	void ClearTopicHandler(String topic)
	{
		m_topicHandlers.erase(topic);
	}

	bool IsOpen()
	{
		return m_open;
	}

private:
	void m_processPacket(char* data, size_t length, TCPPacketMode mode);
	void m_eraseBuffer(size_t end);

	void m_pushJsonObject(lua_State* L, const nlohmann::json& packet);
	void m_pushJsonValue(lua_State* L, const nlohmann::json& val);

	bool m_readyToRead();

	// Bound lua states
	Map<struct lua_State*, class LuaBindable*> m_boundStates;

	// TODO(itszn) maybe support a stack of handlers per topic
	Map<String, IFunctionBinding<bool, nlohmann::json&>*> m_topicHandlers;
	Map<String, LuaTCPHandler*> m_luaTopicHandlers;
	IFunctionBinding<void>* m_closeCallback = nullptr;

	// Underlying socket for the class
	SOCKET m_socket;

	// Tell if the socket is currently open and connected
	bool m_open;

	// Internal variables for data buffering and packet reading
	uint32 m_dataLength = 0;
	uint32 m_amountRead = 0;
	char* m_dataBuff = nullptr;
	TCPPacketMode m_readingMode = TCPPacketMode::NOT_READING;
};
