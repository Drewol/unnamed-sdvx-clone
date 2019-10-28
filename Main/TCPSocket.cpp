#include "stdafx.h"
#include "TCPSocket.hpp"
#include "lua.hpp"
#include "Application.hpp"
#include "Shared/LuaBindable.hpp"

#include <algorithm>

TCPSocket::TCPSocket()
{
	m_open = false;
	m_socket = INVALID_SOCKET;

#ifdef _WIN32
	// Startup winsock
	WSADATA wsa_data;
	if (WSAStartup(MAKEWORD(1, 1), &wsa_data) != 0)
	{
		Logf("[Socket] Unable to start winsock!", Logger::Error);
	}
#endif
}

TCPSocket::~TCPSocket()
{
	if (m_open)
		Close();

	// Clear bound states
	for (auto& s : m_boundStates)
	{
		delete s.second;
	}
	m_boundStates.clear();

#ifdef _WIN32
	// Clean up winsock
	WSACleanup();
#endif
}

// Connect this TCP socket to a given host and port
bool TCPSocket::Connect(String host)
{
	if (m_open)
		return false;

	size_t port_index = host.find_first_of(":");
	String port = host.substr(port_index+1, host.length());

	host = host.substr(0, port_index);

	Logf("[Socket] Connecting to %s:%s", Logger::Info, host.c_str(), port.c_str());

	struct addrinfo* result = nullptr;
	struct addrinfo* ptr = nullptr;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;


	// Resolve the ip of the host
	int res = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
	if (res != 0)
	{
		Logf("[Socket] Unable to resolve address %s:%s %d", Logger::Error, host.c_str(), port.c_str(), res);
		return false;
	}

	// Create a TCP socket
	m_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (invalid_socket(m_socket))
	{
		Logf("[Socket] Unable to create socket to address %s port %s", Logger::Error, host.c_str(), port.c_str());
		return false;
	}

	// Try to connect to host (blocking)
	if (connect(m_socket, result->ai_addr, (int)result->ai_addrlen) != 0)
	{
		Logf("[Socket] Unable to connect to address %s port %s", Logger::Error, host.c_str(), port.c_str());
		return false;
	}

	m_open = true;
	return true;
}


// Lua binding to connect to a given server
int TCPSocket::lConnect(struct lua_State* L)
{
	String host = luaL_checkstring(L, 2);

	if (m_open)
	{
		lua_pushboolean(L, false);
		return 1;
	}

	bool res = Connect(host);
	lua_pushboolean(L, res);
	return 1;
}

// Lua binding to send a line of data to the socket
int TCPSocket::lSendLine(struct lua_State* L)
{
	String data = luaL_checkstring(L, 2);

	SendLine(data);

	return 0;
}

// Send a line of data to the socket
void TCPSocket::SendLine(String data)
{
	// For now send a 0 to say it is line deliniated
	char zeroByte = (char)TCPPacketMode::JSON_LINE;
	send(m_socket, (char*)& zeroByte, 1, 0);

	send(m_socket, data.c_str(), data.length(), 0);

	char new_line = '\n';
	send(m_socket, (char*)& new_line, 1, 0);
}

// Send a JSON packet to the server
void TCPSocket::SendJSON(nlohmann::json packet)
{
	SendLine(packet.dump());
}

// Lua function to close the socket
int TCPSocket::lClose(struct lua_State* L)
{
	if (!m_open)
		return 0;
	Close();
	return 0;
}

// Check if the socket is ready to read using select
bool TCPSocket::m_readyToRead()
{
	if (!m_open)
		return false;

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(m_socket, &fds);

	// Non blocking time
	timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	return select(m_socket + 1, &fds, 0, 0, &tv) == 1;
}

// Close the socket if it is open
void TCPSocket::Close()
{
	if (!m_open)
		return;

	int status = 0;
#ifdef _WIN32
	status = shutdown(m_socket, SD_BOTH);
	if (status == 0) { status = closesocket(m_socket); }
#else
	status = shutdown(m_socket, SHUT_RDWR);
	if (status == 0) { status = close(m_socket); }
#endif

	m_open = false;

	// Reset buffering
	if (m_dataBuff)
	{
		free(m_dataBuff);
		m_dataBuff = nullptr;
	}
	m_dataLength = 0;
	m_amountRead = 0;
	m_readingMode = TCPPacketMode::NOT_READING;

	Log("[Socket] Socket closed", Logger::Info);

	if (m_closeCallback != nullptr)
	{
		m_closeCallback->Call();
	}
}


void TCPSocket::m_processPacket(char* ptr, size_t length, TCPPacketMode mode)
{
	String packetData = String(ptr, length);
	if (mode != TCPPacketMode::JSON_LINE)
	{
		Logf("[Socket] Could not handle packet with mode %u", Logger::Error, mode);
		return;
	}

	// TODO(itszn) Line handler callback?

	auto jsonPacket = nlohmann::json::parse(packetData);

	// TODO(itszn) Raw json handler callback?

	if (!jsonPacket.is_object())
	{
		Logf("[Socket] Not an object %s", Logger::Error, jsonPacket.dump());
		return;
	}

	String topic = jsonPacket.value("topic", "");
	if (topic == "")
	{
		Logf("[Socket] No topic in %s", Logger::Error, jsonPacket.dump());
		return;
	}

	// Call any C++ topic handlers
	if (m_topicHandlers.find(topic) != m_topicHandlers.end())
	{
		IFunctionBinding<bool, nlohmann::json&>* f = m_topicHandlers[topic];
		if (!f->Call(jsonPacket))
			return;
	}

	// Call any lua topic handlers
	if (m_luaTopicHandlers.find(topic) != m_luaTopicHandlers.end())
	{
		LuaTCPHandler* handler = m_luaTopicHandlers[topic];
		if (m_boundStates.Contains(handler->L))
		{
			lua_rawgeti(handler->L, LUA_REGISTRYINDEX, handler->callback);
			m_pushJsonObject(handler->L, jsonPacket);
			if (lua_pcall(handler->L, 1, 0, 0) != 0)
			{
				Logf("[Socket] Lua error on calling TCP handler: %s", Logger::Error, lua_tostring(handler->L, -1));
			}
			lua_settop(handler->L, 0);
		}
	}
}

// Push a single json value as a lua table
void TCPSocket::m_pushJsonValue(lua_State* L, const nlohmann::json& val)
{
	if (val.is_object())
	{
		m_pushJsonObject(L, val);
	}
	else if (val.is_array())
	{
		lua_newtable(L);
		for (int index = 0; index < val.size(); index++)
		{
			lua_pushinteger(L, index + 1);
			m_pushJsonValue(L, val[index]);
			lua_settable(L, -3);
		}
	}
	else if (val.is_boolean())
	{
		lua_pushboolean(L, val);
	}
	else if (val.is_null())
	{
		lua_pushnil(L);
	}
	else if (val.is_string())
	{
    String valueString;
    val.get_to(valueString);
		lua_pushstring(L, *valueString);
	}
	else if (val.is_number_integer())
	{
		lua_pushinteger(L, val);
	}
	else if (val.is_number())
	{
		lua_pushnumber(L, val);
	}
	else
	{
		lua_pushnil(L);
	}
}

// Push a json object onto the lua stack as a table
void TCPSocket::m_pushJsonObject(lua_State* L, const nlohmann::json& packet)
{
	assert(packet.is_object());

	lua_newtable(L);

	for (auto& el : packet.items())
	{
		const String& key = el.key();
		auto& val = el.value();
		
		lua_pushstring(L, key.c_str());
		m_pushJsonValue(L, val);
		lua_settable(L, -3);
	}
}

// Buffer data from the socket and try to read packets out of it
void TCPSocket::ProcessSocket()
{
	if (!m_open)
		return;

	// This is the case where we don't have any data in the buffer
	if (m_readingMode == TCPPacketMode::NOT_READING)
	{
		// If we for some reason are still not reading yet we have stuff in the buffer
		// then our state is bad (server sent a packet we couldn't understand)
		if (m_amountRead != 0)
		{
			Close();
			return;
		}

		if (!m_readyToRead())
			return;
		
		// Get a byte to indicate the packet type
		unsigned char mode;
		if (recv(m_socket, (char*)&mode, 1, 0) <= 0)
		{
			Close();
			return;
		}

		m_readingMode = static_cast<TCPPacketMode>(
			std::min(mode, (unsigned char)TCPPacketMode::UNKNOWN));

		// Create the buffer if it doesn't exist
		if (m_dataBuff == nullptr)
		{
			m_dataLength = 4096;
			m_amountRead = 0;
			m_dataBuff = (char*)malloc(m_dataLength + 1); // Plus one for potential null byte
		}

		if (!m_readyToRead())
			return;
	}

	// We don't know the kind of packet this is so bail on the stream
	if (m_readingMode == TCPPacketMode::UNKNOWN)
	{
		Close();
		return;
	}

	// We will start reading into the buffer and try to parse at least one packet
	// We may parse more packets if they happen to have been buffered in
	while (m_readyToRead() && m_readingMode != TCPPacketMode::NOT_READING)
	{
		// Fill up the buffer as much as we can
		int newRead = recv(m_socket, m_dataBuff + m_amountRead,
			std::min(m_dataLength - m_amountRead, 4096u), 0);

		if (newRead <= 0)
		{
			Close();
			return;
		}

		// First packet scan only looks at the new data
		uint32 newDataStart = m_amountRead;

		// Update total amount read
		m_amountRead += newRead;
		
		// Parse out any packets in the buffer currently
		while (m_readingMode != TCPPacketMode::NOT_READING)
		{
			if (m_readingMode == TCPPacketMode::JSON_LINE)
			{
				// Scan the new bytes for line break
				int tokenIndex;
				for (tokenIndex = newDataStart;
					tokenIndex < m_amountRead && m_dataBuff[tokenIndex] != '\n'; tokenIndex++);

				if (tokenIndex < m_amountRead)
				{
					// We have a valid packet in the buffer

					// Replace newline with null terminator
					m_dataBuff[tokenIndex] = '\0';

					m_processPacket(m_dataBuff, tokenIndex, m_readingMode);

					// Check if we can have an extra byte in the buffer to start next packet
					if (tokenIndex + 1 < m_amountRead)
					{
						unsigned char mode = m_dataBuff[++tokenIndex];
						m_readingMode = static_cast<TCPPacketMode>(
							std::min(mode, (unsigned char)TCPPacketMode::UNKNOWN));
					}
					else
					{
						// In this case we used up the whole buffer, so wait for next recv
						m_readingMode = TCPPacketMode::NOT_READING;
					}

					// Clear out used data from buffer
					m_eraseBuffer(tokenIndex + 1);

					// Next packet scan will begin at the start
					newDataStart = 0;
					continue;
				}

				// There is no complete packet in the buffer currently, so stop scanning
				break;
			}

			// If we hit a bad packet, bail
			else if (m_readingMode == TCPPacketMode::UNKNOWN)
			{
				Close();
				return;
			}
			
		}

		// Check if we need to expand the buffer
		if (m_amountRead >= m_dataLength)
		{
			m_dataLength *= 2;

			// Some max limit incase we have bad data coming to us
			if (m_dataLength > 0x1000000)
			{
				Close();
				return;
			}

			// Resize the buffer
			m_dataBuff = (char*)realloc(m_dataBuff, m_dataLength + 1);
		}
	}
}

// This erases the data in the buffer up to start
void TCPSocket::m_eraseBuffer(size_t end)
{
	// Copy data to start of buffer
	m_amountRead -= end;
	memmove(m_dataBuff, m_dataBuff + end, m_amountRead);

	// Find the new smallest buffer size to contain the data
	// Should always be less than 4096 but check to be safe
	for (m_dataLength = 4096; m_dataLength < m_amountRead; m_dataLength *= 2);

	// Resize buffer to new buffer size
	char* new_dataBuff = static_cast<char*>(realloc(m_dataBuff, m_dataLength + 1));
	if (!new_dataBuff)
		free(m_dataBuff);
	m_dataBuff = new_dataBuff;
}

void TCPSocket::PushFunctions(lua_State* L)
{
	auto bindable = new LuaBindable(L, "Tcp");
	bindable->AddFunction("Connect", this, &TCPSocket::lConnect);
	bindable->AddFunction("SetTopicHandler", this, &TCPSocket::lSetTopicHandler);
	bindable->AddFunction("SendLine", this, &TCPSocket::lSendLine);
	bindable->AddFunction("Close", this, &TCPSocket::lClose);
	bindable->Push();
	lua_settop(L, 0);
	m_boundStates.Add(L, bindable);

	// Trigger the lua tcp init
	lua_getglobal(L, "init_tcp");
	if (lua_isfunction(L, -1))
	{
		if (lua_pcall(L, 0, 0, 0) != 0)
		{
			Logf("[Socket] Lua error on init_tcp: %s", Logger::Error, lua_tostring(L, -1));
			g_gameWindow->ShowMessageBox("Lua Error on init_tcp", lua_tostring(L, -1), 0);
		}
	}
	lua_settop(L, 0);
}

void TCPSocket::ClearState(lua_State* L)
{
	if (!m_boundStates.Contains(L))
		return;

	// TODO(itszn) remove any lua callbacks from this state

	delete m_boundStates.at(L);
	m_boundStates.erase(L);
}


// Add a new handler to the stack
int TCPSocket::lSetTopicHandler(struct lua_State* L)
{
	String topic = luaL_checkstring(L, 2);
	int callback = luaL_ref(L, LUA_REGISTRYINDEX);

	LuaTCPHandler* h = new LuaTCPHandler();
	h->callback = callback;
	h->L = L;

	m_luaTopicHandlers[topic] = h;
	return 0;
}
