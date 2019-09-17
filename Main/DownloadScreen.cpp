#include "stdafx.h"
#include "DownloadScreen.hpp"
#include "Application.hpp"
#include "lua.hpp"
#include "archive.h"
#include "archive_entry.h"
#include "SkinHttp.hpp"
#include "GameConfig.hpp"
#include "cpr/util.h"

DownloadScreen::DownloadScreen()
{
}

DownloadScreen::~DownloadScreen()
{
	g_input.OnButtonPressed.RemoveAll(this);
	g_input.OnButtonReleased.RemoveAll(this);
	g_gameWindow->OnMouseScroll.RemoveAll(this);
	m_running = false;
	if(m_archiveThread.joinable())
		m_archiveThread.join();
	if(m_lua)
		g_application->DisposeLua(m_lua);
}

bool DownloadScreen::Init()
{
	m_lua = g_application->LoadScript("downloadscreen");
	if (m_lua == nullptr)
		return false;
	g_input.OnButtonPressed.Add(this, &DownloadScreen::m_OnButtonPressed);
	g_input.OnButtonReleased.Add(this, &DownloadScreen::m_OnButtonReleased);
	g_gameWindow->OnMouseScroll.Add(this, &DownloadScreen::m_OnMouseScroll);
	m_bindable = new LuaBindable(m_lua, "dlScreen");
	m_bindable->AddFunction("Exit", this, &DownloadScreen::m_Exit);
	m_bindable->AddFunction("DownloadArchive", this, &DownloadScreen::m_DownloadArchive);
	m_bindable->AddFunction("GetSongsPath", this, &DownloadScreen::m_GetSongsPath);
	m_bindable->Push();
	lua_settop(m_lua, 0);

	m_archiveThread = Thread(&DownloadScreen::m_ArchiveLoop, this);

	return true;
}

void DownloadScreen::Tick(float deltaTime)
{
	m_advanceSong += g_input.GetInputLaserDir(1);
	int advanceSongActual = (int)Math::Floor(m_advanceSong * Math::Sign(m_advanceSong)) * Math::Sign(m_advanceSong);
	if (advanceSongActual != 0)
	{
		lua_getglobal(m_lua, "advance_selection");
		if (lua_isfunction(m_lua, -1))
		{
			lua_pushnumber(m_lua, advanceSongActual);
			if (lua_pcall(m_lua, 1, 0, 0) != 0)
			{
				Logf("Lua error on advance_selection: %s", Logger::Error, lua_tostring(m_lua, -1));
				g_gameWindow->ShowMessageBox("Lua Error on advance_selection", lua_tostring(m_lua, -1), 0);
			}
		}
		lua_settop(m_lua, 0);
	}
	m_advanceSong -= advanceSongActual;
	m_ProcessArchiveResponses();
}

void DownloadScreen::Render(float deltaTime)
{
	if (IsSuspended())
		return;

	lua_getglobal(m_lua, "render");
	lua_pushnumber(m_lua, deltaTime);
	if (lua_pcall(m_lua, 1, 0, 0) != 0)
	{
		Logf("Lua error: %s", Logger::Error, lua_tostring(m_lua, -1));
		g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
		g_application->RemoveTickable(this);
	}
}

void DownloadScreen::OnKeyPressed(int32 key)
{
	lua_getglobal(m_lua, "key_pressed");
	if (lua_isfunction(m_lua, -1))
	{	
		lua_pushnumber(m_lua, key);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on key_pressed: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on key_pressed", lua_tostring(m_lua, -1), 0);
		}
	}
	lua_settop(m_lua, 0);
}

void DownloadScreen::OnKeyReleased(int32 key)
{
	lua_getglobal(m_lua, "key_released");
	if (lua_isfunction(m_lua, -1))
	{
		lua_pushnumber(m_lua, key);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on key_released: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on key_released", lua_tostring(m_lua, -1), 0);
		}
	}
	lua_settop(m_lua, 0);
}

void DownloadScreen::m_ArchiveLoop()
{
	while (m_running)
	{
		m_archiveLock.lock();
		if (m_archiveReqs.size() > 0)
		{
			//get request
			ArchiveRequest& r = m_archiveReqs.front();
			m_archiveLock.unlock();
			auto response = r.response.get();
			if (response.error.code == cpr::ErrorCode::OK && response.status_code < 300)
			{
				ArchiveResponse ar;
				archive* a;
				a = archive_read_new();
				ar.a = a;
				ar.callback = r.callback;
				ar.id = r.id;
				ar.data.resize(response.text.length());
				memcpy(ar.data.data(), response.text.c_str(), ar.data.size());

				archive_read_support_filter_all(a);
				archive_read_support_format_all(a);
				int res = archive_read_open_memory(a, ar.data.data(), ar.data.size());
				if (res == ARCHIVE_OK)
				{
				
					m_archiveLock.lock();
					m_archiveResps.push(std::move(ar));
					m_archiveLock.unlock();
				}
				else
				{
					res = archive_read_free(a);
				}
			}
			m_archiveLock.lock();
			m_archiveReqs.pop();
			m_archiveLock.unlock();
		}
		else
		{
			m_archiveLock.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
	}
}

void DownloadScreen::m_OnButtonPressed(Input::Button buttonCode)
{
	lua_getglobal(m_lua, "button_pressed");
	if (lua_isfunction(m_lua, -1))
	{
		lua_pushnumber(m_lua, (int32)buttonCode);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on button_pressed: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on button_pressed", lua_tostring(m_lua, -1), 0);
		}
	}
	lua_settop(m_lua, 0);
}

void DownloadScreen::m_OnButtonReleased(Input::Button buttonCode)
{
	lua_getglobal(m_lua, "button_released");
	if (lua_isfunction(m_lua, -1))
	{
		lua_pushnumber(m_lua, (int32)buttonCode);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on button_released: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on button_released", lua_tostring(m_lua, -1), 0);
		}
	}
	lua_settop(m_lua, 0);
}

void DownloadScreen::m_OnMouseScroll(int32 steps)
{
	lua_getglobal(m_lua, "advance_selection");
	if (lua_isfunction(m_lua, -1))
	{
		lua_pushnumber(m_lua, steps);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on advance_selection: %s", Logger::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on advance_selection", lua_tostring(m_lua, -1), 0);
		}
	}
	lua_settop(m_lua, 0);
}

void DownloadScreen::m_ProcessArchiveResponses()
{
	m_archiveLock.lock();
	if (m_archiveResps.size() > 0)
	{
		//get response
		ArchiveResponse& ar = m_archiveResps.front();
		m_archiveLock.unlock();

		//process response
		lua_rawgeti(m_lua, LUA_REGISTRYINDEX, ar.callback);
		struct archive_entry *entry;
		int numEntries = 1;
		lua_newtable(m_lua);
		while (archive_read_next_header(ar.a, &entry) == ARCHIVE_OK) {
			//Logf("%s", Logger::Info, archive_entry_pathname(entry));
			lua_pushinteger(m_lua, numEntries++);
			lua_pushstring(m_lua, archive_entry_pathname(entry));
			lua_settable(m_lua, -3);
			archive_read_data_skip(ar.a);
		}
		archive_read_free(ar.a);
		lua_pushstring(m_lua, ar.id.c_str());
		
		if (lua_pcall(m_lua, 2, 1, 0) != 0)
		{
			Logf("Lua error on calling archive callback: %s", Logger::Error, lua_tostring(m_lua, -1));
		}
		else //process returned table and extract files
		{
			auto entryPathMap = m_mapFromLuaTable(1);

			if (entryPathMap.Contains(".folders"))
			{
				auto folders = entryPathMap.at(".folders").Explode("|");
				Set<String> created;
				for (String& f : folders)
				{
					if (created.Contains(f))
						continue;
					Path::CreateDir(f);
					created.Add(f);
				}
			}

			entry = nullptr;
			ar.a = archive_read_new();
			archive_read_support_filter_all(ar.a);
			archive_read_support_format_all(ar.a);
			archive_read_open_memory(ar.a, ar.data.data(), ar.data.size());
			while (archive_read_next_header(ar.a, &entry) == ARCHIVE_OK) {
				String entryName = archive_entry_pathname(entry);
				if (entryPathMap.Contains(entryName))
				{
					if (!m_extractFile(ar.a, entryPathMap.at(entryName)))
					{
						Logf("Failed to extract file: \"%s\"", Logger::Warning, entryName);
						Logf("Archive error: %s", Logger::Warning, archive_error_string(ar.a));
					}
				}
			}
		}
		lua_settop(m_lua, 0);
		luaL_unref(m_lua, LUA_REGISTRYINDEX, ar.callback);
		
		//pop response
		m_archiveLock.lock();
		m_archiveResps.pop();
	}
	m_archiveLock.unlock();
}

int DownloadScreen::m_DownloadArchive(lua_State* L)
{
	String url = luaL_checkstring(L, 2);
	auto header = SkinHttp::HeaderFromLuaTable(L, 3);
	String id = luaL_checkstring(L, 4);
	int callback = luaL_ref(L, LUA_REGISTRYINDEX);


	m_archiveLock.lock();
	m_archiveReqs.push(ArchiveRequest{ cpr::GetAsync(cpr::Url{ url }, header), id, callback });
	m_archiveLock.unlock();

	return 0;
}

int DownloadScreen::m_GetSongsPath(lua_State * L)
{
	lua_pushstring(L, *Path::Normalize(Path::Absolute(g_gameConfig.GetString(GameConfigKeys::SongFolder))));
	return 1;
}

bool DownloadScreen::m_extractFile(archive * a, String path)
{
	int r;
	const void *buff;
	size_t size;
	la_int64_t offset;
	File f;
	if (path.back() == '/') //folder
	{
		Path::CreateDir(path);
		return true;
	}
	
	if (!f.OpenWrite(Path::Normalize(path)))
	{
		return false;
	}

	while(true) {
		r = archive_read_data_block(a, &buff, &size, &offset);
		if (r == ARCHIVE_EOF)
			return true;
		if (r < ARCHIVE_OK)
			return false;
		f.Write(buff, size);
	}
	f.Close();
}

//https://stackoverflow.com/a/6142700
Map<String, String> DownloadScreen::m_mapFromLuaTable(int index)
{
	Map<String, String> ret;
	if (!lua_istable(m_lua, index))
	{
		return ret;
	}
	lua_pushvalue(m_lua, index);
	lua_pushnil(m_lua);
	while (lua_next(m_lua, -2))
	{
		lua_pushvalue(m_lua, -2);
		const char *key = lua_tostring(m_lua, -1);
		const char *value = lua_tostring(m_lua, -2);
		ret[key] = value;
		lua_pop(m_lua, 2);
	}
	lua_pop(m_lua, 1);
	return ret;
}

int DownloadScreen::m_Exit(lua_State * L)
{
	g_application->RemoveTickable(this);
	return 0;
}
