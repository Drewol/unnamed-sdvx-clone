#include "stdafx.h"
#include "DownloadScreen.hpp"

#include "Application.hpp"
#include "lua.hpp"
#include "Shared/Time.hpp"
#include "archive.h"
#include "archive_entry.h"
#include "SkinHttp.hpp"
#include "GameConfig.hpp"
#include "cpr/util.h"
#include <Audio/Audio.hpp>

#include "Shared/StringEncodingDetector.hpp"
#include "Shared/StringEncodingConverter.hpp"
#include <include/Search.hpp>

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
	m_bindable->AddFunction("PlayPreview", this, &DownloadScreen::m_PlayPreview);
	m_bindable->AddFunction("StopPreview", this, &DownloadScreen::m_StopPreview);
	m_bindable->AddFunction("GetSongsPath", this, &DownloadScreen::m_GetSongsPath);
	m_bindable->Push();
	lua_settop(m_lua, 0);

	m_archiveThread = Thread(&DownloadScreen::m_ArchiveLoop, this);

	m_searchInput = Ref<TextInput>(new TextInput());
	m_searchInput->OnTextChanged.Add(this, &DownloadScreen::OnSearchTermChanged);

	return true;
}

void DownloadScreen::OnSearchTermChanged(const String &search)
{
	lua_getglobal(m_lua, "update_search_text");
	if (lua_isfunction(m_lua, -1))
	{
		lua_pushboolean(m_lua, m_searchInput->active);
		lua_pushstring(m_lua, *search);
		if (lua_pcall(m_lua, 2, 0, 0) != 0)
		{
			Logf("Lua error on update_search_text: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on update_search_text", lua_tostring(m_lua, -1), 0);
		}
	}
	lua_settop(m_lua, 0);
	if (m_lastSearch != search)
	{
		m_lastSearch = search;
		m_timeSinceSearchChange = 0;
	}
}

void DownloadScreen::Tick(float deltaTime)
{
	m_previewPlayer.Update(deltaTime);
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
				Logf("Lua error on advance_selection: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
				g_gameWindow->ShowMessageBox("Lua Error on advance_selection", lua_tostring(m_lua, -1), 0);
			}
		}
		lua_settop(m_lua, 0);
	}
	m_advanceSong -= advanceSongActual;
	m_ProcessArchiveResponses();

	m_searchInput->Tick();

	if (m_timeSinceSearchChange >= 0)
	{
		m_timeSinceSearchChange += deltaTime;
		if (m_timeSinceSearchChange > 1.5)
		{
			lua_getglobal(m_lua, "update_search_filters");
			if (lua_isfunction(m_lua, -1))
			{
				lua_newtable(m_lua);

				String raw = m_searchInput->input;

				lua_pushstring(m_lua, "raw");
				lua_pushstring(m_lua, *raw);
				lua_settable(m_lua, -3);

				String effector;
				String uploader;
				String level;

				const String query = SearchParser::Parse(raw, {
					{ "effector", &effector },
					{ "uploader", &uploader },
					{ "level",    &level    },
				});

				lua_pushstring(m_lua, "query");
				if (query.empty())
					lua_pushnil(m_lua);
				else
					lua_pushstring(m_lua, *query);
				lua_settable(m_lua, -3);

				lua_pushstring(m_lua, "effector");
				if (effector.empty())
					lua_pushnil(m_lua);
				else
					lua_pushstring(m_lua, *effector);
				lua_settable(m_lua, -3);

				lua_pushstring(m_lua, "uploader");
				if (uploader.empty())
					lua_pushnil(m_lua);
				else
					lua_pushstring(m_lua, *uploader);
				lua_settable(m_lua, -3);

				lua_pushstring(m_lua, "levels");
				if (level.empty())
					lua_pushnil(m_lua);
				else
				{
					lua_newtable(m_lua);

					for (int num = 1 ; !level.empty(); num++)
					{
						String unit;
						String rest;
						if (!level.Split(",", &unit, &rest))
						{
							unit = level;
							rest = "";
						}

						lua_pushnumber(m_lua, num);
						lua_pushstring(m_lua, *unit);
						lua_settable(m_lua, -3);

						level = rest;
					}
				}
				lua_settable(m_lua, -3);

				if (lua_pcall(m_lua, 1, 0, 0) != 0)
				{
					Logf("Lua error on update_search_filter: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
					g_gameWindow->ShowMessageBox("Lua Error on update_search_filter", lua_tostring(m_lua, -1), 0);
				}
			}
			lua_settop(m_lua, 0);
			m_timeSinceSearchChange = -1;
		}
	}
}

void DownloadScreen::Render(float deltaTime)
{
	if (IsSuspended())
		return;

	lua_getglobal(m_lua, "render");
	lua_pushnumber(m_lua, deltaTime);
	if (lua_pcall(m_lua, 1, 0, 0) != 0)
	{
		Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
		g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(m_lua, -1), 0);
		g_application->RemoveTickable(this);
	}
}

void DownloadScreen::OnKeyPressed(SDL_Scancode code, int32 delta)
{
	if (!m_searchInput->active)
	{
		lua_getglobal(m_lua, "key_pressed");
		if (lua_isfunction(m_lua, -1))
		{
			lua_pushnumber(m_lua, static_cast<lua_Number>(SDL_GetKeyFromScancode(code)));
			if (lua_pcall(m_lua, 1, 0, 0) != 0)
			{
				Logf("Lua error on key_pressed: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
				g_gameWindow->ShowMessageBox("Lua Error on key_pressed", lua_tostring(m_lua, -1), 0);
			}
		}
		lua_settop(m_lua, 0);
	}
}

void DownloadScreen::OnKeyReleased(SDL_Scancode code, int32 delta)
{
	lua_getglobal(m_lua, "key_released");
	if (lua_isfunction(m_lua, -1))
	{
		lua_pushnumber(m_lua, static_cast<lua_Number>(SDL_GetKeyFromScancode(code)));
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on key_released: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on key_released", lua_tostring(m_lua, -1), 0);
		}
	}
	lua_settop(m_lua, 0);
}

void DownloadScreen::m_ArchiveLoop()
{
	{
		String preview_path = Path::Normalize(Path::Absolute("preview/"));
		Vector<String> exts = { "mp3", "oog", "wav" };
		Map<String, Vector<FileInfo>> previews = Files::ScanFilesRecursive(preview_path, exts, nullptr);
		uint64 now = Shared::Time::Now().Data();
		Log("Checking for old preview files", Logger::Severity::Info);
		int removed = 0;
		for (const auto& ext : exts)
		{
			for (const FileInfo& fi : previews[ext])
			{
				uint64 writeTime = File::FileTimeToUnixTimestamp(fi.lastWriteTime);

				if (now - writeTime < 24 * 60 * 60)
					continue;
				Path::Delete(fi.fullPath);
				removed++;
			}
		}
		if (removed > 0)
			Logf("Removed %u preview files", Logger::Severity::Info, removed);
	}
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
					Log("Error opening downloaded chart archive for directory traversal", Logger::Severity::Error);
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

void DownloadScreen::m_OnButtonPressed(Input::Button buttonCode, int32 delta)
{
	if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Keyboard && m_searchInput->active)
		if (buttonCode != Input::Button::SongSelect_OpenSearch && buttonCode != Input::Button::SongSelect_CloseSearch)
			return;

	lua_getglobal(m_lua, "button_pressed");
	if (lua_isfunction(m_lua, -1))
	{
		lua_pushnumber(m_lua, (int32)buttonCode);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on button_pressed: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on button_pressed", lua_tostring(m_lua, -1), 0);
		}
	}
	lua_settop(m_lua, 0);

	int dir;
	switch (buttonCode) {
		case Input::Button::SongSelect_Up:
		case Input::Button::SongSelect_Down:
			dir = (buttonCode == Input::Button::SongSelect_Up) ? -1 : 1;
			lua_getglobal(m_lua, "advance_selection");
			if (lua_isfunction(m_lua, -1))
			{
				lua_pushnumber(m_lua, dir);
				if (lua_pcall(m_lua, 1, 0, 0) != 0)
				{
					Logf("Lua error on advance_selection: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
					g_gameWindow->ShowMessageBox("Lua Error on advance_selection", lua_tostring(m_lua, -1), 0);
				}
			}
			lua_settop(m_lua, 0);
			break;
		case Input::Button::SongSelect_OpenSearch:
			lua_getglobal(m_lua, "update_search_text");
			if (lua_isfunction(m_lua, -1))
			{
				m_searchInput->SetActive(!m_searchInput->active);
				OnSearchTermChanged(m_searchInput->input);
			}
			lua_settop(m_lua, 0);
			break;
		case Input::Button::SongSelect_CloseSearch:
			if (m_searchInput->active) {
				m_searchInput->SetActive(false);
				OnSearchTermChanged(m_searchInput->input);
			}
			break;
	}
}

void DownloadScreen::m_OnButtonReleased(Input::Button buttonCode, int32 delta)
{
	lua_getglobal(m_lua, "button_released");
	if (lua_isfunction(m_lua, -1))
	{
		lua_pushnumber(m_lua, (int32)buttonCode);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on button_released: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
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
			Logf("Lua error on advance_selection: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
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
		// When CP_UTF8 is used, libarchive will read the file name as the system codepage and convert it to UTF8 (why???)
		// Temporarily change the locale to C while extracting the archive to avoid the problem.
		String defaultLocale = setlocale(LC_CTYPE, nullptr);
		setlocale(LC_CTYPE, "C");

		// Get response
		ArchiveResponse& ar = m_archiveResps.front();
		m_archiveLock.unlock();

		StringEncoding archiveEncoding = StringEncodingDetector::DetectArchive(ar.data);
		if (archiveEncoding != StringEncoding::Unknown)
			Logf("Archive encoding is assumed to be %s", Logger::Severity::Info, GetDisplayString(archiveEncoding));
		else
			Log("Archive encoding couldn't be assumed. (Assuming UTF-8)", Logger::Severity::Warning);

		// Process response
		lua_rawgeti(m_lua, LUA_REGISTRYINDEX, ar.callback);
		struct archive_entry *entry;
		int numEntries = 1;
		bool readError = false;
		lua_newtable(m_lua);
		while (archive_read_next_header(ar.a, &entry) == ARCHIVE_OK) {
			const String pathname = StringEncodingConverter::PathnameToUTF8(archiveEncoding, entry);
			if (pathname.empty())
			{
				readError = true;
				break;
			}

			lua_pushinteger(m_lua, numEntries++);
			lua_pushstring(m_lua, pathname.c_str());
			lua_settable(m_lua, -3);
			archive_read_data_skip(ar.a);
		}

		if (archive_read_free(ar.a) != ARCHIVE_OK)
		{
			Log("Error closing handle for downloaded chart archive", Logger::Severity::Error);
		}
		lua_pushstring(m_lua, ar.id.c_str());

		if (readError)
		{
			Log("Error reading downloaded chart archive", Logger::Severity::Error);
		}
		else if (lua_pcall(m_lua, 2, 1, 0) != 0)
		{
			Logf("Lua error on calling archive callback: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
		}
		else // Process returned table and extract files
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

			if (archive_read_open_memory(ar.a, ar.data.data(), ar.data.size()) != ARCHIVE_OK)
			{
				Log("Error opening downloaded chart archive for extraction", Logger::Severity::Error);
			}
			else while (archive_read_next_header(ar.a, &entry) == ARCHIVE_OK) {
				const String entryName = StringEncodingConverter::PathnameToUTF8(archiveEncoding, entry);
				if (entryPathMap.Contains(entryName))
				{
					if (!m_extractFile(ar.a, entryPathMap.at(entryName)))
					{
						Logf("Failed to extract file: \"%s\"", Logger::Severity::Warning, entryName);
						Logf("Archive error: %s", Logger::Severity::Warning, archive_error_string(ar.a));
					}
				}
			}

			if (archive_read_free(ar.a) != ARCHIVE_OK)
			{
				Log("Error closing archive handle after extracting", Logger::Severity::Error);
			}
		}

		lua_settop(m_lua, 0);
		luaL_unref(m_lua, LUA_REGISTRYINDEX, ar.callback);

		// Pop response
		m_archiveLock.lock();
		m_archiveResps.pop();

		setlocale(LC_CTYPE, defaultLocale.c_str());
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

	const String dot_dot_win = "..\\";
	const String dot_dot_unix = "../";

	if (path.find(dot_dot_win) != String::npos) {
		Logf("[Archive] Error reading chart archive: '%s' can't appear in file name '%s'", Logger::Severity::Error, dot_dot_win.c_str(), path.c_str());
		return false;
	}
	if (path.find(dot_dot_unix) != String::npos) {
		Logf("[Archive] Error reading chart archive: '%s' can't appear in file name '%s'", Logger::Severity::Error, dot_dot_unix.c_str(), path.c_str());
		return false;
	}

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
	return true;
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

int DownloadScreen::m_PlayPreview(lua_State* L)
{
	String url = luaL_checkstring(L, 2);
	auto header = SkinHttp::HeaderFromLuaTable(L, 3);
	String song_id = luaL_checkstring(L, 4);

	Logf("Requesting Preview for song %s", Logger::Severity::Info, song_id);

	String ext = Path::GetExtension(url);

	String preview_path = Path::Normalize(Path::Absolute("preview/" + song_id + "." + ext ));
	// Create dir if it doesn't exist
	Path::CreateDir(Path::Absolute("preview"));

	bool hasFile = Path::FileExists(preview_path);

	// Download file if we have not before
	if (!hasFile)
	{
		Logf("Requesting Preview URL %s", Logger::Severity::Info, url);
		// TODO Move out of main thread?
		cpr::Response preview_data = cpr::Get(cpr::Url{ url }, header);
		auto response = preview_data;
		if (response.error.code == cpr::ErrorCode::OK && response.status_code < 300)
		{
			File replayFile;
			if (replayFile.OpenWrite(preview_path))
			{
				replayFile.Write(response.text.c_str(), response.text.length());
				hasFile = true;
			}
		}
	}

	if (!hasFile)
	{
		Logf("Could not save preview file at \"%s\"", Logger::Severity::Warning, preview_path);
		return 0;
	}

	Logf("Playing preview %s", Logger::Severity::Info, preview_path);

	// Try to play the preview
	Ref<AudioStream> previewAudio = g_audio->CreateStream(preview_path);
	if (previewAudio && previewAudio.get())
	{
		m_previewPlayer.FadeTo(previewAudio, 0);
	}
	else
	{
		Logf("Failed to load preview audio from [%s]", Logger::Severity::Warning, preview_path);
		m_previewPlayer.FadeTo(Ref<AudioStream>());
	}

	return 0;
}

int DownloadScreen::m_StopPreview(lua_State* L)
{
	m_previewPlayer.FadeTo(Ref<AudioStream>());
	return 0;
}
