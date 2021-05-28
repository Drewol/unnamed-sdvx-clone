#include "stdafx.h"
#include "MultiplayerScreen.hpp"
#include "Application.hpp"
#include "SkinHttp.hpp"
#include "GameConfig.hpp"
#include "cpr/util.h"
#include "SongSelect.hpp"
#include "SettingsScreen.hpp"
#include "SkinConfig.hpp"
#include "ChatOverlay.hpp"
#include <Audio/Audio.hpp>

#include <TransitionScreen.hpp>
#include <Game.hpp>
#include "Gauge.hpp"

#define MULTIPLAYER_VERSION "v0.19"

// XXX probably should be moved with the songselect one to its own class file?
class TextInputMultiplayer
{
public:
	String input;
	String composition;
	uint32 backspaceCount;
	bool active = false;
	Delegate<const String&> OnTextChanged;
	bool start_taking_input = false;

	~TextInputMultiplayer()
	{
		g_gameWindow->OnTextInput.RemoveAll(this);
		g_gameWindow->OnTextComposition.RemoveAll(this);
		g_gameWindow->OnKeyRepeat.RemoveAll(this);
		g_gameWindow->OnKeyPressed.RemoveAll(this);
	}

	void OnTextInput(const String& wstr)
	{
		if (!start_taking_input)
			return;
		input += wstr;
		OnTextChanged.Call(input);
	}
	void OnTextComposition(const Graphics::TextComposition& comp)
	{
		composition = comp.composition;
	}
	void OnKeyRepeat(SDL_Scancode key)
	{
		if (key == SDL_SCANCODE_BACKSPACE)
		{
			if (input.empty())
				backspaceCount++; // Send backspace
			else
			{
				auto it = input.end(); // Modify input string instead
				--it;
				while ((*it & 0b11000000) == 0b10000000)
				{
					input.erase(it);
					--it;
				}
				input.erase(it);
				OnTextChanged.Call(input);
			}
		}
	}
	void OnKeyPressed(SDL_Scancode code)
	{
		SDL_Keycode key = SDL_GetKeyFromScancode(code);
		if (key == SDLK_v)
		{
			if (g_gameWindow->GetModifierKeys() == ModifierKeys::Ctrl)
			{
				if (g_gameWindow->GetTextComposition().composition.empty())
				{
					// Paste clipboard text into input buffer
					input += g_gameWindow->GetClipboard();
				}
			}
		}
	}
	void SetActive(bool state)
	{
		active = state;
		if (state)
		{
			start_taking_input = false;

			SDL_StartTextInput();
			g_gameWindow->OnTextInput.Add(this, &TextInputMultiplayer::OnTextInput);
			g_gameWindow->OnTextComposition.Add(this, &TextInputMultiplayer::OnTextComposition);
			g_gameWindow->OnKeyRepeat.Add(this, &TextInputMultiplayer::OnKeyRepeat);
			g_gameWindow->OnKeyPressed.Add(this, &TextInputMultiplayer::OnKeyPressed);
		}
		else
		{
			// XXX This seems to break nuklear so I commented it out
			//SDL_StopTextInput();
			g_gameWindow->OnTextInput.RemoveAll(this);
			g_gameWindow->OnTextComposition.RemoveAll(this);
			g_gameWindow->OnKeyRepeat.RemoveAll(this);
			g_gameWindow->OnKeyPressed.RemoveAll(this);
		}
	}
	void Reset()
	{
		backspaceCount = 0;
		input.clear();
	}
	void Tick()
	{
		// Wait until we release the start button
		if (active && !start_taking_input && !g_input.GetButton(Input::Button::BT_S))
		{
			start_taking_input = true;
		}
	}
};

MultiplayerScreen::~MultiplayerScreen()
{
	g_input.OnButtonPressed.RemoveAll(this);
	g_input.OnButtonReleased.RemoveAll(this);
	g_gameWindow->OnMouseScroll.RemoveAll(this);
	g_gameWindow->OnMousePressed.RemoveAll(this);
	m_mapDatabase->OnFoldersCleared.Clear();
	if (m_lua)
	{
		g_application->DisposeLua(m_lua);
		m_tcp.ClearState(m_lua);
	}

	delete m_chatOverlay;
	delete m_bindable;
}

bool MultiplayerScreen::Init()
{
	if (!m_settDiag.Init())
		return false;

	return true;
}

void MultiplayerScreen::JoinRoomWithToken(String token)
{
	m_joinToken = token;
	// Already authed
	if (m_userId != "")
		return m_joinRoomWithToken();
}

void MultiplayerScreen::m_joinRoomWithToken()
{
	nlohmann::json joinReq;
	joinReq["topic"] = "server.room.join";
	joinReq["token"] = m_joinToken;
	m_tcp.SendJSON(joinReq);
}

void MultiplayerScreen::m_authenticate()
{
	if (m_textInput->active)
		m_textInput->SetActive(false);

	String password = g_gameConfig.GetString(GameConfigKeys::MultiplayerPassword);

	if (m_userName == "")
		m_userName = "Guest";

	nlohmann::json packet;
	packet["topic"] = "user.auth";
	packet["password"] = password;
	packet["name"] = m_userName;
	packet["version"] = MULTIPLAYER_VERSION;
	m_tcp.SendJSON(packet);
}

void MultiplayerScreen::m_handleSocketClose()
{
	// Don't exit if we are in game or selection
	if (m_suspended)
		return;

	g_application->RemoveTickable(this);
}

void MultiplayerScreen::m_render(float deltaTime)
{
	m_statusLock.lock();
	lua_pushstring(m_lua, *m_lastStatus);
	lua_setglobal(m_lua, "searchStatus");
	m_statusLock.unlock();

	lua_getglobal(m_lua, "render");
	lua_pushnumber(m_lua, deltaTime);


	if (lua_pcall(m_lua, 1, 0, 0) != 0)
	{
		Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
		g_gameWindow->ShowMessageBox("Lua Error in render", lua_tostring(m_lua, -1), 0);

		g_application->RemoveTickable(this);
	}
}


bool MultiplayerScreen::m_handleBadPassword(nlohmann::json& packet)
{
	lua_pushboolean(m_lua, true);
	lua_setglobal(m_lua, "passwordError");

	lua_pushnumber(m_lua, 1.0f);
	lua_setglobal(m_lua, "passwordErrorOffset");
	return true;
}

bool MultiplayerScreen::m_handleRoomList(nlohmann::json& packet)
{
	g_application->DiscordPresenceMenu("Browsing multiplayer rooms");

	if (m_screenState != MultiplayerScreenState::ROOM_LIST) {
		m_screenState = MultiplayerScreenState::ROOM_LIST;
		lua_pushstring(m_lua, "roomList");
		lua_setglobal(m_lua, "screenState");

		m_roomId = "";
		m_hasSelectedMap = false;
	}

	return true;
}

bool MultiplayerScreen::m_handleJoinRoom(nlohmann::json& packet)
{
	if (m_textInput->active)
		m_textInput->SetActive(false);


	m_screenState = MultiplayerScreenState::IN_ROOM;
	m_chatOverlay->EnableOpeningChat();

	lua_pushstring(m_lua, "inRoom");
	lua_setglobal(m_lua, "screenState");
	packet["room"]["id"].get_to(m_roomId);
	packet["room"]["join_token"].get_to(m_joinToken);
	g_application->DiscordPresenceMulti(m_joinToken, 1, 8, "test");

	String roomname;
	packet["room"]["name"].get_to(roomname);
	m_chatOverlay->AddMessage("You joined "+roomname, 207, 178, 41);

	return true;
}

bool MultiplayerScreen::m_handleError(nlohmann::json& packet)
{
	g_gameWindow->ShowMessageBox("Multiplayer server closed", packet.value("error", ""), 0);
	
	// Fatal error, so leave this view
	m_suspended = true;
	g_application->RemoveTickable(this);
	return true;
}

// Save the unique user id the server assigns us
bool MultiplayerScreen::m_handleAuthResponse(nlohmann::json& packet)
{
	double server_version = atof(static_cast<String>(packet.value("version", "0.0")).c_str()+1);
	if (server_version < 0.16)
	{
		g_gameWindow->ShowMessageBox("Multiplayer server closed", "This version of multiplayer (" MULTIPLAYER_VERSION ") does not support this server", 0);
		// Fatal error, so leave this view
		m_suspended = true;
		g_application->RemoveTickable(this);
		return false;
	}

	g_application->DiscordPresenceMenu("Browsing multiplayer rooms");
	packet["userid"].get_to(m_userId);
	m_scoreInterval = packet.value("refresh_rate",1000);

	// If we are waiting to join a room, join now
	if (m_joinToken != "")
		m_joinRoomWithToken();
	return true;
}

bool MultiplayerScreen::m_handleRoomUpdate(nlohmann::json& packet)
{
	int userCount = packet.at("users").size();
	m_joinToken = packet.value("join_token", "");
	g_application->DiscordPresenceMulti(m_joinToken, userCount, 8, "test");
	m_handleSongChange(packet);

	return true;
}
bool MultiplayerScreen::m_handleSongChange(nlohmann::json& packet)
{
	// Case for no song selected yet
	if (packet["song"].is_null())
		return true;

	const String& chart_hash = packet.value("chart_hash", "");
	const String& audio_hash = packet.value("audio_hash", "");
	const String& song = packet.value("song", "");

	// Fallback case for no hash provided from the server
	if (m_hasSelectedMap && chart_hash.length() == 0 && song == m_selectedMapShortPath)
		return true;

	// Case for same song as before
	if (m_hasSelectedMap && chart_hash == m_selectedMapHash)
		return true;

	// Clear jacket variable to force image reload
	lua_pushinteger(m_lua, 0);
	lua_setglobal(m_lua, "jacket");


	// Grab new song
	uint32 diff_ind = packet["diff"];
	ChartIndex* newChart = m_getChartByHash(chart_hash, song, &diff_ind, packet["level"]);

	if (newChart == nullptr)
	{
		// If we don't find it, then update lua state
		m_hasSelectedMap = false;
		m_clearLuaMap();

		// Tell the server we don't have the map
		// TODO(itszn) send less frequently?
		nlohmann::json packet;
		packet["topic"] = "user.nomap";
		m_tcp.SendJSON(packet);

		return true;
	}

	m_selfPicked = false;
	m_updateSelectedMap(newChart->folderId, diff_ind, false);

	m_selectedMapShortPath = song;
	m_selectedMapHash = chart_hash;

	return true;
}

bool MultiplayerScreen::m_handleFinalStats(nlohmann::json& packet)
{
	m_addFinalStat(packet);
	return true;
}

// Start the game
bool MultiplayerScreen::m_handleStartPacket(nlohmann::json& packet)
{
	if (!this->m_hasSelectedMap)
		return false;

	m_inGame = true;
	m_failed = false;
	m_syncState = SyncState::LOADING;
	m_finalStats.clear();

	// Grab the map from the database
	FolderIndex* folder = m_mapDatabase->GetFolder(m_selectedMapId);
	ChartIndex* chart = folder->charts[m_selectedDiffIndex];

	// Reset score time before playing
	m_lastScoreSent = 0;

	Logf("[Multiplayer] Starting game: diff_id=%d mapId=%d path=%s", Logger::Severity::Info, chart->id, chart->folderId, chart->path.c_str());

	// The server tells us if we are playing excessive or not
	bool is_hard = packet["hard"];
	bool is_mirror = packet.value("mirror", false);

	PlaybackOptions opts;
	if (is_hard)
		opts.gaugeType = GaugeType::Hard;
	opts.mirror = is_mirror;

	// Create the game using the Create that takes the MultiplayerScreen class
	Game* game = Game::Create(this, chart, opts);
	if (!game)
	{
		Log("Failed to start game", Logger::Severity::Error);
		return 0;
	}

	m_suspended = true;

	// Switch to the new tickable
	g_transition->TransitionTo(game);
	return false;
}


bool MultiplayerScreen::m_handleSyncStartPacket(nlohmann::json& packet)
{
	m_syncState = SyncState::SYNCED;
	return true;
}

// Get a song for given audio hash and level
// If we find a match find a matching hash but not level, just go with any level
ChartIndex* MultiplayerScreen::m_getChartByHash(const String& hash, const String& path, uint32* diffIndex, int32 level)
{
	// Fallback on an empty hash
	if (hash.length() == 0)
		return m_getChartByShortPath(path, diffIndex, level, true);
	
	Logf("[Multiplayer] looking up song hash '%s' level %u", Logger::Severity::Info, *hash, level);
	for (auto folder : m_mapDatabase->FindFoldersByHash(hash))
	{
		ChartIndex* newChart = NULL;

		for (size_t ind = 0; ind < folder.second->charts.size(); ind++)
		{
			ChartIndex* chart = folder.second->charts[ind];
			if (chart->level == level)
			{
				// We found a matching level for this hash, good to go
				newChart = chart;
				*diffIndex = ind;
			}
		}

		// We didn't find the exact level, but we should still use this map anyway
		if (newChart == NULL)
		{
			assert(folder.second->charts.size() > 0);
			*diffIndex = 0;
			newChart = folder.second->charts[0];
		}

		if (newChart != NULL)
		{
			Logf("[Multiplayer] Found: diff_id=%d mapid=%d index=%u path=%s", Logger::Severity::Info, newChart->id, newChart->folderId, *diffIndex, newChart->path.c_str());
			return newChart;
		}
	}
	Log("[Multiplayer] Could not find song by hash, falling back to foldername", Logger::Severity::Warning);
	return m_getChartByShortPath(path, diffIndex, level, true);
}

// Get a map from a given "short" path, and level
// The selected index will be written to diffIndex
// diffIndex can be used as a hint to which song to pick
ChartIndex* MultiplayerScreen::m_getChartByShortPath(const String& path, uint32* diffIndex, int32 level, bool useHint)
{
	Logf("[Multiplayer] looking up song '%s' level %u difficulty index hint %u", Logger::Severity::Info, path.c_str(), level, *diffIndex);

	for (auto folder : m_mapDatabase->FindFoldersByPath(path))
	{
		ChartIndex* newChart = NULL;

		for (size_t ind = 0; ind < folder.second->charts.size(); ind++)
		{
			ChartIndex* chart = folder.second->charts[ind];

			if (chart->level == level)
			{
				// First we try to get exact song (kinda) by matching index hint to level
				if (useHint && *diffIndex != ind)
					break;

				// If we already searched the songs just take any level that matches
				newChart = chart;
				*diffIndex = ind;
				break;
			}
		}

		// If we didn't find any matches and we are on our last try, just pick anything
		if (newChart == NULL && !useHint)
		{
			assert(folder.second->charts.size() > 0);
			*diffIndex = 0;
			newChart = folder.second->charts[0];
		}


		if (newChart != NULL)
		{
			Logf("[Multiplayer] Found: diff_id=%d mapid=%d index=%u path=%s", Logger::Severity::Info, newChart->id, newChart->folderId, *diffIndex, newChart->path.c_str());
			return newChart;
		}
	}

	// Search one more time, but ignore the hint this time
	if (useHint)
	{
		*diffIndex = 0;
		return m_getChartByShortPath(path, diffIndex, level, false);
	}

	Log("[Multiplayer] Could not find song", Logger::Severity::Warning);
	return nullptr;
}

// Set the selected map to a given map and difficulty (used by SongSelect)
void MultiplayerScreen::SetSelectedMap(FolderIndex* folder, ChartIndex* chart)
{
	const SongSelectIndex song(folder, chart);

	// Get the "short" path (basically the last part of the path)
	const size_t last_slash_idx = song.GetFolder()->path.find_last_of("\\/");
	std::string short_path = song.GetFolder()->path.substr(last_slash_idx + 1);

	// Get the difficulty index into the selected map
	uint32 diff_index = (song.id % 10) - 1;

	// Get the actual map id
	const ChartIndex* newChart = m_getChartByHash(chart->hash, short_path, &diff_index, chart->level);

	if (newChart == nullptr)
	{
		// Somehow we failed the round trip, so we can't use this map
		m_hasSelectedMap = false;
		return;
	}
	

	m_selfPicked = true;
	m_updateSelectedMap(newChart->folderId, diff_index, true);

	m_selectedMapShortPath = short_path;
	m_selectedMapHash = chart->hash;
}

void MultiplayerScreen::m_changeSelectedRoom(int offset)
{
	lua_getglobal(m_lua, "change_selected_room");
	if (lua_isfunction(m_lua, -1))
	{
		lua_pushinteger(m_lua, offset);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on set_diff: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on set_diff", lua_tostring(m_lua, -1), 0);
			assert(false);
		}
	}
	lua_settop(m_lua, 0);
	
}

void MultiplayerScreen::m_changeDifficulty(int offset)
{
	FolderIndex* folder = m_mapDatabase->GetFolder(this->m_selectedMapId);
	int oldDiff = this->m_selectedDiffIndex;
	int newInd = this->m_selectedDiffIndex + offset;
	if (newInd < 0 || newInd >= (int)folder->charts.size())
	{
		return;
	}

	m_updateSelectedMap(this->m_selectedMapId, newInd, false);

	lua_getglobal(m_lua, "set_diff");
	if (lua_isfunction(m_lua, -1))
	{
		lua_pushinteger(m_lua, oldDiff + 1);
		lua_pushinteger(m_lua, newInd + 1);
		if (lua_pcall(m_lua, 2, 0, 0) != 0)
		{
			Logf("Lua error on set_diff: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on set_diff", lua_tostring(m_lua, -1), 0);
			assert(false);
		}
	}
	lua_settop(m_lua, 0);
	

}

void  MultiplayerScreen::GetMapBPMForSpeed(String path, struct MultiplayerBPMInfo& info)
{
	path = Path::Normalize(path);
	if (!Path::FileExists(path))
	{
		Logf("Couldn't find map at %s", Logger::Severity::Error, path);

		info = { 0, 0, 0, 0 };
		return;
	}

	// Load map
	Beatmap* newMap = new Beatmap();
	File mapFile;
	if (!mapFile.OpenRead(path))
	{
		Logf("Could not read path for beatmap: %s", Logger::Severity::Error, path);
		delete newMap;
		info = { 0, 0, 0, 0 };
		return;
	}
	FileReader reader(mapFile);
	if (!newMap->Load(reader))
	{
		delete newMap;
		info = { 0, 0, 0, 0 };
		return;
	}

	// Most of this code is copied from Game.cpp to match its calculations

	double useBPM = -1;

	const BeatmapSettings& mapSettings = newMap->GetMapSettings();

	info.start = newMap->GetLinearTimingPoints().front()->GetBPM();

	ObjectState* const* lastObj = &newMap->GetLinearObjects().back();
	while ((*lastObj)->type == ObjectType::Event && lastObj != &newMap->GetLinearObjects().front())
	{
		lastObj--;
	}

	MapTime lastObjectTime = (*lastObj)->time;
	if ((*lastObj)->type == ObjectType::Hold)
	{
		HoldObjectState* lastHold = (HoldObjectState*)(*lastObj);
		lastObjectTime += lastHold->duration;
	}
	else if ((*lastObj)->type == ObjectType::Laser)
	{
		LaserObjectState* lastHold = (LaserObjectState*)(*lastObj);
		lastObjectTime += lastHold->duration;
	}

	{
		Map<double, MapTime> bpmDurations;
		const Vector<TimingPoint*>& timingPoints = newMap->GetLinearTimingPoints();
		MapTime lastMT = mapSettings.offset;
		MapTime largestMT = -1;
		double lastBPM = -1;

		info.min = -1;
		info.max = -1;

		for (TimingPoint* tp : timingPoints)
		{
			double thisBPM = tp->GetBPM();

			if (info.max == -1 || thisBPM > info.max)
				info.max = thisBPM;

			if (info.min == -1 || thisBPM < info.min)
				info.min = thisBPM;

			if (!bpmDurations.count(lastBPM))
			{
				bpmDurations[lastBPM] = 0;
			}
			MapTime timeSinceLastTP = tp->time - lastMT;
			bpmDurations[lastBPM] += timeSinceLastTP;
			if (bpmDurations[lastBPM] > largestMT)
			{
				useBPM = lastBPM;
				largestMT = bpmDurations[lastBPM];
			}
			lastMT = tp->time;
			lastBPM = thisBPM;
		}
		bpmDurations[lastBPM] += lastObjectTime - lastMT;

		if (bpmDurations[lastBPM] > largestMT)
		{
			useBPM = lastBPM;
		}
		info.mode = useBPM;
	}

	delete newMap;
}

ChartIndex* MultiplayerScreen::GetCurrentSelectedChart() const
{
	if (!m_hasSelectedMap)
		return nullptr;

	FolderIndex* folder = m_mapDatabase->GetFolder(this->m_selectedMapId);
	ChartIndex* chart = folder->charts[this->m_selectedDiffIndex];

	return chart;
}

void MultiplayerScreen::m_updateSelectedMap(int32 mapid, int32 diff_ind, bool isNew)
{
	this->m_selectedMapId = mapid;
	this->m_selectedDiffIndex = diff_ind;

	// Get the current map from the database
	FolderIndex* folder = m_mapDatabase->GetFolder(mapid);
	ChartIndex* chart = folder->charts[diff_ind];

	// Find "short" path for the selected map
	const size_t lastSlashIdx = folder->path.find_last_of("\\/");
	String shortPath = folder->path.substr(lastSlashIdx + 1);

	m_hispeed = g_gameConfig.GetFloat(GameConfigKeys::HiSpeed);
	m_speedMod = g_gameConfig.GetEnum<Enum_SpeedMods>(GameConfigKeys::SpeedMod);
	m_modSpeed = g_gameConfig.GetFloat(GameConfigKeys::ModSpeed);

	GetMapBPMForSpeed(chart->path, m_bpm);


	m_speedBPM = m_bpm.start;
	if (m_speedMod == SpeedMods::MMod)
	{
		m_speedBPM = m_bpm.mode;
		m_hispeed = m_modSpeed / m_speedBPM;
	}
	else if (m_speedMod == SpeedMods::CMod)
	{
		m_hispeed = m_modSpeed / m_speedBPM;
	}


	// Push a table of info to lua
	lua_newtable(m_lua);
	m_PushStringToTable("title", chart->title.c_str());
	m_PushStringToTable("artist", chart->artist.c_str());
	m_PushStringToTable("bpm", chart->bpm.c_str());
	m_PushIntToTable("id", folder->id);
	m_PushStringToTable("path", folder->path.c_str());
	m_PushStringToTable("short_path", *shortPath);

	m_PushStringToTable("jacketPath", Path::Normalize(folder->path + "/" + chart->jacket_path).c_str());
	m_PushIntToTable("level", chart->level);
	m_PushIntToTable("difficulty", chart->diff_index);
	m_PushIntToTable("diff_index", diff_ind);
	lua_pushstring(m_lua, "self_picked");
	lua_pushboolean(m_lua, m_selfPicked);
	lua_settable(m_lua, -3);
	m_PushStringToTable("effector", chart->effector.c_str());
	m_PushStringToTable("illustrator", chart->illustrator.c_str());

	int diffIndex = 0;
	lua_pushstring(m_lua, "all_difficulties");
	lua_newtable(m_lua);
	for (auto& chart : folder->charts)
	{
		lua_pushinteger(m_lua, ++diffIndex);
		lua_newtable(m_lua);
		m_PushIntToTable("level", chart->level);
		m_PushIntToTable("id", chart->id);
		m_PushIntToTable("diff_index", diffIndex-1);
		m_PushIntToTable("difficulty", chart->diff_index);
		lua_settable(m_lua, -3);
	}
	lua_settable(m_lua, -3);

	lua_pushstring(m_lua, "hispeed");
	lua_pushnumber(m_lua, m_hispeed);
	lua_settable(m_lua, -3);

	lua_pushstring(m_lua, "min_bpm");
	lua_pushnumber(m_lua, m_bpm.min);
	lua_settable(m_lua, -3);

	lua_pushstring(m_lua, "max_bpm");
	lua_pushnumber(m_lua, m_bpm.max);
	lua_settable(m_lua, -3);

	lua_pushstring(m_lua, "start_bpm");
	lua_pushnumber(m_lua, m_bpm.start);
	lua_settable(m_lua, -3);

	lua_pushstring(m_lua, "speed_bpm");
	lua_pushnumber(m_lua, m_speedBPM);
	lua_settable(m_lua, -3);

	lua_setglobal(m_lua, "selected_song");

	m_hasSelectedMap = true;
	m_updatePreview(chart, true);

	// If we selected this song ourselves, we have to tell the server about it
	if (isNew)
	{
		
		nlohmann::json packet;
		packet["topic"] = "room.setsong";
		packet["song"] = shortPath;
		packet["diff"] = diff_ind;
		packet["level"] = chart->level;
		packet["chart_hash"] = chart->hash;
		m_tcp.SendJSON(packet);
	}
	else
	{
		nlohmann::json packet;
		packet["topic"] = "user.song.level";
		packet["level"] = chart->level;
		m_tcp.SendJSON(packet);
	}

}

void MultiplayerScreen::m_clearLuaMap()
{
	lua_pushnil(m_lua);
	lua_setglobal(m_lua, "selected_song");
}

void MultiplayerScreen::MousePressed(MouseButton button)
{
	if (IsSuspended() || m_chatOverlay->IsOpen())
		return;

	lua_getglobal(m_lua, "mouse_pressed");
	if (lua_isfunction(m_lua, -1))
	{
		lua_pushnumber(m_lua, (int32)button);
		if (lua_pcall(m_lua, 1, 1, 0) != 0)
		{
			Logf("Lua error on mouse_pressed: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on mouse_pressed", lua_tostring(m_lua, -1), 0);
			assert(false);
		}
	}
	lua_settop(m_lua, 0);
}

bool MultiplayerScreen::m_returnToMainList()
{
	if (m_screenState == MultiplayerScreenState::ROOM_LIST)
		return false;

	// Exiting from name setup
	if (m_userName == "")
	{
		m_suspended = true;
		g_application->RemoveTickable(this);
		return true;
	}

	if (m_screenState == MultiplayerScreenState::IN_ROOM)
	{
		nlohmann::json packet;
		packet["topic"] = "room.leave";
		m_tcp.SendJSON(packet);
	}
	else
	{
		nlohmann::json packet;
		packet["topic"] = "server.rooms";
		m_tcp.SendJSON(packet);
	}

	if (m_textInput->active)
		m_textInput->SetActive(false);

	if (m_screenState == MultiplayerScreenState::IN_ROOM) {
		m_chatOverlay->AddMessage("You left the lobby", 207, 178, 41);
	}

	m_chatOverlay->EnableOpeningChat();

	m_screenState = MultiplayerScreenState::ROOM_LIST;
	m_stopPreview();
	lua_pushstring(m_lua, "roomList");
	lua_setglobal(m_lua, "screenState");
	g_application->DiscordPresenceMulti("", 0, 0, "");
	m_roomId = "";
	m_hasSelectedMap = false;
	return true;
}

void MultiplayerScreen::Tick(float deltaTime)
{
	// Tick the tcp socket even if we are suspended
	m_tcp.ProcessSocket();

	if (m_dbUpdateTimer.Milliseconds() > 500)
	{
		m_mapDatabase->Update();
		m_dbUpdateTimer.Restart();
	}


	m_textInput->Tick();

	lua_newtable(m_lua);
	m_PushStringToTable("text", m_textInput->input.c_str());
	lua_setglobal(m_lua, "textInput");


	if (IsSuspended())
		return;

	for (size_t i = 0; i < (size_t)Input::Button::Length; i++)
	{
		m_timeSinceButtonPressed[(Input::Button)i] += deltaTime;
		m_timeSinceButtonReleased[(Input::Button)i] += deltaTime;
	}

	m_settDiag.Tick(deltaTime);

	m_chatOverlay->Tick(deltaTime);
	m_previewPlayer.Update(deltaTime);

	// Lock mouse to screen when active
	if (m_screenState == MultiplayerScreenState::ROOM_LIST && 
		g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice) == InputDevice::Mouse &&
        g_gameWindow->IsActive() &&
        !m_chatOverlay->IsOpen())
	{
		if (!m_lockMouse)
			m_lockMouse = g_input.LockMouse();
	}
	else
	{
		if (m_lockMouse)
			m_lockMouse.reset();
		g_gameWindow->SetCursorVisible(true);
	}

	// Change difficulty
	if (m_hasSelectedMap && !m_chatOverlay->IsOpen())
	{
		if (g_input.GetButton(Input::Button::BT_0)) {
			for (int i = 0; i < 2; i++) {
				float change = g_input.GetInputLaserDir(i) / 3.0f;
				m_hispeed += change;
				m_hispeed = Math::Clamp(m_hispeed, 0.1f, 16.f);
				if ((m_speedMod != SpeedMods::XMod) && change != 0.0f)
				{
					g_gameConfig.Set(GameConfigKeys::ModSpeed, m_hispeed * m_speedBPM);
					m_modSpeed = m_hispeed * m_speedBPM;

					lua_getglobal(m_lua, "selected_song");

					lua_pushstring(m_lua, "hispeed");
					lua_pushnumber(m_lua, m_hispeed);
					lua_settable(m_lua, -3);

					lua_setglobal(m_lua, "selected_song");
				}
			}
		}
		else {
			float diff_input = g_input.GetInputLaserDir(0);
			m_advanceDiff += diff_input;
			int advanceDiffActual = (int)Math::Floor(m_advanceDiff * Math::Sign(m_advanceDiff)) * Math::Sign(m_advanceDiff);
			if (advanceDiffActual != 0)
				m_changeDifficulty(advanceDiffActual);
			m_advanceDiff -= advanceDiffActual;
		}
	}

	// Room selection
	if (m_screenState == MultiplayerScreenState::ROOM_LIST)
	{
		float room_input = g_input.GetInputLaserDir(1);
		m_advanceRoom += room_input;
		int advanceRoomActual = (int)Math::Floor(m_advanceRoom * Math::Sign(m_advanceRoom)) * Math::Sign(m_advanceRoom);
		if (advanceRoomActual != 0)
			m_changeSelectedRoom(advanceRoomActual);
		m_advanceRoom -= advanceRoomActual;
	}
}

void MultiplayerScreen::PerformScoreTick(Scoring& scoring, MapTime time)
{
	if (m_failed)
		return;

	int scoreUpdateIndex = time / m_scoreInterval;

	if (scoreUpdateIndex <= m_lastScoreSent)
		return;

	m_lastScoreSent = scoreUpdateIndex;

	uint32 score = scoring.CalculateCurrentScore();

	nlohmann::json packet;
	packet["topic"] = "room.score.update";
	packet["time"] = Math::Max(0, scoreUpdateIndex) * m_scoreInterval;
	packet["score"] = score;

	m_tcp.SendJSON(packet);
}

void MultiplayerScreen::m_addFinalStat(nlohmann::json& data)
{
	m_finalStats.push_back(data);
	// Sort all scores
	sort(m_finalStats.begin(), m_finalStats.end(),
		[](const nlohmann::json& a, const nlohmann::json& b) -> bool
		{
			return (a["score"].get<int>() + (a["clear"].get<int>() > 1 ? 10000000 : 0)) >
				(b["score"].get<int>() +( b["clear"].get<int>() > 1 ? 10000000 : 0));
		});
}

void MultiplayerScreen::SendFinalScore(class Game* game, ClearMark clearState)
{
	Scoring& scoring = game->GetScoring();

	clearState = HasFailed() ? ClearMark::Played : clearState;

	PlaybackOptions opts = game->GetPlaybackOptions();
	Gauge* gauge = scoring.GetTopGauge();

	nlohmann::json packet;
	packet["topic"] = "room.score.final";
	packet["score"] = scoring.CalculateCurrentScore();
	packet["combo"] = scoring.maxComboCounter;
	packet["clear"] = static_cast<int>(clearState);
	packet["gauge"] = gauge->GetValue();
	packet["early"] = scoring.timedHits[0];
	packet["late"] = scoring.timedHits[1];
	packet["miss"] = scoring.categorizedHits[0];
	packet["near"] = scoring.categorizedHits[1];
	packet["crit"] = scoring.categorizedHits[2];

	packet["gauge_type"] = (uint32)gauge->GetType();
	packet["gauge_option"] = gauge->GetOpts();
	packet["mirror"] = opts.mirror;
	packet["random"] = opts.random;
	packet["auto_flags"] = (uint32)opts.autoFlags;

	//flags for backwards compatibility
	opts.gaugeType = gauge->GetType();
	opts.gaugeOption = gauge->GetOpts();
	packet["flags"] = PlaybackOptions::ToLegacyFlags(opts);

	packet["mean_delta"] = scoring.GetMeanHitDelta();
	packet["median_delta"] = scoring.GetMedianHitDelta();

	packet["graph"] = game->GetGaugeSamples();

	m_tcp.SendJSON(packet);

	packet["name"] = m_userName;
	packet["uid"] = m_userId;

	m_addFinalStat(packet);

	// In case we exit early
	m_syncState = SyncState::SYNCED;
}

void MultiplayerScreen::Render(float deltaTime)
{
	if (!IsSuspended()) {
		m_render(deltaTime);
		m_chatOverlay->Render(deltaTime);
		m_settDiag.Render(deltaTime);
	}
}

void MultiplayerScreen::ForceRender(float deltaTime)
{
	m_render(deltaTime);
	m_chatOverlay->Render(deltaTime);
}

void MultiplayerScreen::OnSearchStatusUpdated(String status)
{
	m_statusLock.lock();
	m_lastStatus = status;
	m_statusLock.unlock();
}

void MultiplayerScreen::OnKeyPressed(SDL_Scancode code)
{
	if (IsSuspended() || m_settDiag.IsActive())
		return;

	if (m_chatOverlay->OnKeyPressedConsume(code))
		return;

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

	if (code == SDL_SCANCODE_LEFT && m_hasSelectedMap)
	{
		m_changeDifficulty(-1);
	}
	else if (code == SDL_SCANCODE_RIGHT && m_hasSelectedMap)
	{
		m_changeDifficulty(1);
	}
	else if (code == SDL_SCANCODE_UP && m_screenState == MultiplayerScreenState::ROOM_LIST)
	{
		m_changeSelectedRoom(-1);
	}
	else if (code == SDL_SCANCODE_DOWN && m_screenState == MultiplayerScreenState::ROOM_LIST)
	{
		m_changeSelectedRoom(1);
	}
	else if (code == SDL_SCANCODE_ESCAPE)
	{
		int backScancode = g_gameConfig.GetInt(GameConfigKeys::Key_Back);

		if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) != InputDevice::Keyboard
			|| backScancode != SDL_SCANCODE_ESCAPE)
		{
			switch (m_screenState)
			{
				// Escape works for text editing
			case MultiplayerScreenState::JOIN_PASSWORD:
			case MultiplayerScreenState::NEW_ROOM_NAME:
			case MultiplayerScreenState::NEW_ROOM_PASSWORD:
			case MultiplayerScreenState::SET_USERNAME:
				m_returnToMainList();
			default:
				break;
			}
		}
	}
	else if (code == SDL_SCANCODE_RETURN)
	{
		if (m_screenState == MultiplayerScreenState::JOIN_PASSWORD) 
		{
			lJoinWithPassword(NULL);
		}
		else if (m_screenState == MultiplayerScreenState::NEW_ROOM_NAME ||
			m_screenState == MultiplayerScreenState::NEW_ROOM_PASSWORD)
		{
			lNewRoomStep(NULL);
		}
		else if (m_screenState == MultiplayerScreenState::SET_USERNAME)
		{
			lSaveUsername(NULL);
		}
	}
}



void MultiplayerScreen::OnKeyReleased(SDL_Scancode code)
{
	if (IsSuspended() || m_chatOverlay->IsOpen() || m_settDiag.IsActive())
		return;

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

void MultiplayerScreen::m_OnButtonPressed(Input::Button buttonCode)
{
	if (IsSuspended() || m_settDiag.IsActive())
		return;

	if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Keyboard)
	{
		if (m_chatOverlay->IsOpen())
			return;

		// Allow Button Back while text editing
		if (m_textInput->active && buttonCode != Input::Button::Back)
			return;
	}

	m_timeSinceButtonPressed[buttonCode] = 0;

	switch (buttonCode)
	{
	case Input::Button::FX_1:
		if (g_input.GetButton(Input::Button::FX_0))
		{
			m_buttonPressed[Input::Button::FX_0] = false;
			m_settDiag.Open();
			return;
		}
		break;
	case Input::Button::FX_0:
		if (g_input.GetButton(Input::Button::FX_1))
		{
			m_buttonPressed[Input::Button::FX_1] = false;
			m_settDiag.Open();
			return;
		}
		break;
	case Input::Button::Back:
		switch (m_screenState)
		{
		case MultiplayerScreenState::JOIN_PASSWORD:
		case MultiplayerScreenState::NEW_ROOM_NAME:
		case MultiplayerScreenState::NEW_ROOM_PASSWORD:
		case MultiplayerScreenState::SET_USERNAME:
			if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Keyboard)
			{
				// In this case we want them to hit escape so we don't exit on text inputs
				break;
			}
			// Otherwise fall though
            [[fallthrough]];
		default:
			if (m_returnToMainList())
				return;
			break;
		}
		break;
	default:
		break;
	}

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

	m_buttonPressed[buttonCode] = true;
}

void MultiplayerScreen::m_OnButtonReleased(Input::Button buttonCode)
{
	if (IsSuspended() || m_chatOverlay->IsOpen() || m_settDiag.IsActive())
		return;

	if (g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice) == InputDevice::Keyboard && m_textInput->active)
		return;

	// Check if press was consumed by something else
	if (!m_buttonPressed[buttonCode])
		return;

	m_timeSinceButtonReleased[buttonCode] = 0;


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

	m_buttonPressed[buttonCode] = false;
}

void MultiplayerScreen::m_OnMouseScroll(int32 steps)
{
	if (IsSuspended() || m_chatOverlay->IsOpen() || m_settDiag.IsActive())
		return;

	lua_getglobal(m_lua, "mouse_scroll");
	if (lua_isfunction(m_lua, -1))
	{
		lua_pushnumber(m_lua, steps);
		if (lua_pcall(m_lua, 1, 0, 0) != 0)
		{
			Logf("Lua error on advance_selection: %s", Logger::Severity::Error, lua_tostring(m_lua, -1));
			g_gameWindow->ShowMessageBox("Lua Error on mouse_scroll", lua_tostring(m_lua, -1), 0);
		}
	}
	lua_settop(m_lua, 0);
}

int MultiplayerScreen::lExit(lua_State * L)
{
	m_suspended = true;
	g_application->RemoveTickable(this);
	return 0;
}

int MultiplayerScreen::lSongSelect(lua_State* L)
{
	m_suspended = true;
	g_transition->TransitionTo(SongSelect::Create(this));
	return 0;
}

int MultiplayerScreen::lSettings(lua_State* L)
{
	if (m_settDiag.IsActive())
		return 0;
	m_settDiag.Open();
	return 0;
}

void MultiplayerScreen::OnRestore()
{
	m_suspended = false;
	m_previewPlayer.Restore();

    // Restart nuklear if we turned it off during suspend
    m_chatOverlay->InitNuklearIfNeeded();

	// If we disconnected while playing or selecting wait until we get back before exiting
	/*if (!m_tcp.IsOpen())
	{
		m_suspended = true;
		g_application->RemoveTickable(this);
		return;
	}*/

	m_mapDatabase->StartSearching();

	// Retrive the lobby info now that we are out of the game
	if (m_inGame)
	{
		m_inGame = false;
		m_selfPicked = false;
		nlohmann::json packet;
		packet["topic"] = "room.update.get";
		m_tcp.SendJSON(packet);
	}
}

bool MultiplayerScreen::AsyncLoad()
{
	// Add a handler for some socket events
	m_tcp.SetTopicHandler("game.started", this, &MultiplayerScreen::m_handleStartPacket);
	m_tcp.SetTopicHandler("game.sync.start", this, &MultiplayerScreen::m_handleSyncStartPacket);
	m_tcp.SetTopicHandler("server.info", this, &MultiplayerScreen::m_handleAuthResponse);
	m_tcp.SetTopicHandler("server.rooms", this, &MultiplayerScreen::m_handleRoomList);
	m_tcp.SetTopicHandler("room.update", this, &MultiplayerScreen::m_handleRoomUpdate);
	m_tcp.SetTopicHandler("server.room.joined", this, &MultiplayerScreen::m_handleJoinRoom);
	m_tcp.SetTopicHandler("server.error", this, &MultiplayerScreen::m_handleError);
	m_tcp.SetTopicHandler("server.room.badpassword", this, &MultiplayerScreen::m_handleBadPassword);
	m_tcp.SetTopicHandler("game.finalstats", this, &MultiplayerScreen::m_handleFinalStats);

	m_tcp.SetCloseHandler(this, &MultiplayerScreen::m_handleSocketClose);

	// TODO(itszn) better method for entering server and port
	String host = g_gameConfig.GetString(GameConfigKeys::MultiplayerHost);
	if (!m_tcp.Connect(host))
		return false;

	m_textInput = Ref<TextInputMultiplayer>(new TextInputMultiplayer());
	m_mapDatabase = new MapDatabase(true);

	// Add database update hook before triggering potential update
	m_mapDatabase->OnDatabaseUpdateStarted.Add(this, &MultiplayerScreen::m_onDatabaseUpdateStart);
	m_mapDatabase->OnDatabaseUpdateDone.Add(this, &MultiplayerScreen::m_onDatabaseUpdateDone);
	m_mapDatabase->OnDatabaseUpdateProgress.Add(this, &MultiplayerScreen::m_onDatabaseUpdateProgress);
	m_mapDatabase->SetChartUpdateBehavior(g_gameConfig.GetBool(GameConfigKeys::TransferScoresOnChartUpdate));
	m_mapDatabase->FinishInit();

	m_mapDatabase->AddSearchPath(g_gameConfig.GetString(GameConfigKeys::SongFolder));
	return true;
}

void MultiplayerScreen::m_onDatabaseUpdateStart(int max)
{
	m_dbUpdateScreen = new DBUpdateScreen(max);
	g_application->AddTickable(m_dbUpdateScreen);
}

void MultiplayerScreen::m_onDatabaseUpdateProgress(int current, int max)
{
	if (!m_dbUpdateScreen)
		return;
	m_dbUpdateScreen->SetCurrent(current, max);
}

void MultiplayerScreen::m_onDatabaseUpdateDone()
{
	if (!m_dbUpdateScreen)
		return;
	g_application->RemoveTickable(m_dbUpdateScreen);
	m_dbUpdateScreen = NULL;
}

bool MultiplayerScreen::AsyncFinalize()
{
	m_lua = g_application->LoadScript("multiplayerscreen");
	if (m_lua == nullptr)
		return false;

	m_previewParams = {"", 0, 0};

	// Install the socket functions and call the lua init
	m_tcp.PushFunctions(m_lua);

	g_input.OnButtonPressed.Add(this, &MultiplayerScreen::m_OnButtonPressed);
	g_input.OnButtonReleased.Add(this, &MultiplayerScreen::m_OnButtonReleased);
	g_gameWindow->OnMouseScroll.Add(this, &MultiplayerScreen::m_OnMouseScroll);
	g_gameWindow->OnMousePressed.Add(this, &MultiplayerScreen::MousePressed);

	m_settDiag.onSongOffsetChange.Add(this, &MultiplayerScreen::m_SetCurrentChartOffset);

	m_bindable = new LuaBindable(m_lua, "mpScreen");
	m_bindable->AddFunction("Exit", this, &MultiplayerScreen::lExit);
	m_bindable->AddFunction("SelectSong", this, &MultiplayerScreen::lSongSelect);
	m_bindable->AddFunction("OpenSettings", this, &MultiplayerScreen::lSettings);
	m_bindable->AddFunction("JoinWithPassword", this, &MultiplayerScreen::lJoinWithPassword);
	m_bindable->AddFunction("JoinWithoutPassword", this, &MultiplayerScreen::lJoinWithoutPassword);
	m_bindable->AddFunction("NewRoomStep", this, &MultiplayerScreen::lNewRoomStep);
	m_bindable->AddFunction("SaveUsername", this, &MultiplayerScreen::lSaveUsername);

	m_bindable->Push();
	lua_settop(m_lua, 0);

	lua_pushstring(m_lua, MULTIPLAYER_VERSION);
	lua_setglobal(m_lua, "MULTIPLAYER_VERSION");

	m_screenState = MultiplayerScreenState::ROOM_LIST;
	lua_pushstring(m_lua, "roomList");
	lua_setglobal(m_lua, "screenState");

	// Start the map database
	m_mapDatabase->OnSearchStatusUpdated.Add(this, &MultiplayerScreen::OnSearchStatusUpdated);
	m_mapDatabase->StartSearching();

	m_userName = g_gameConfig.GetString(GameConfigKeys::MultiplayerUsername);
	if (m_userName == "")
	{
		m_screenState = MultiplayerScreenState::SET_USERNAME;
		lua_pushstring(m_lua, "setUsername");
		lua_setglobal(m_lua, "screenState");
		m_textInput->Reset();
		m_textInput->SetActive(true);
	}
	else
	{
		m_authenticate();
	}

	m_chatOverlay = new ChatOverlay(this);
	m_chatOverlay->Init();

	return true;
}

void MultiplayerScreen::m_SetCurrentChartOffset(int newValue)
{
	if (ChartIndex* chart = GetCurrentSelectedChart())
	{
		chart->custom_offset = newValue;
		m_mapDatabase->UpdateChartOffset(chart);
	}
}

void MultiplayerScreen::OnSuspend()
{
	m_suspended = true;
	m_previewPlayer.Pause();
	m_mapDatabase->StopSearching();

	if (m_lockMouse)
		m_lockMouse.reset();
}

bool MultiplayerScreen::IsSyncing()
{
	return (m_syncState == SyncState::SYNCING);
}

void MultiplayerScreen::StartSync()
{
	m_syncState = SyncState::SYNCING;
	nlohmann::json packet;
	packet["topic"] = "room.sync.ready";
	m_tcp.SendJSON(packet);
}

int MultiplayerScreen::lJoinWithoutPassword(lua_State* L)
{
	m_roomToJoin = luaL_checkstring(L, 2);
	nlohmann::json packet;
	packet["topic"] = "server.room.join";
	packet["id"] = m_roomToJoin;
	m_tcp.SendJSON(packet);
	return 0;
}

int MultiplayerScreen::lSaveUsername(lua_State* L)
{
	if (m_userName != "" || m_textInput->input.length() == 0)
		return 0;

	// Update username
	m_userName = m_textInput->input;
	g_gameConfig.Set(GameConfigKeys::MultiplayerUsername, m_userName);
	m_textInput->SetActive(false);

	m_screenState = MultiplayerScreenState::ROOM_LIST;
	lua_pushstring(m_lua, "roomList");
	lua_setglobal(m_lua, "screenState");

	m_authenticate();
	return 0;
}

int MultiplayerScreen::lJoinWithPassword(lua_State* L)
{
	if (m_screenState == MultiplayerScreenState::ROOM_LIST)
	{
		Log("In screen", Logger::Severity::Error);
		m_roomToJoin = luaL_checkstring(L, 2);
		m_textInput->Reset();
		m_textInput->SetActive(true);
		

		m_screenState = MultiplayerScreenState::JOIN_PASSWORD;
		m_chatOverlay->DisableOpeningChat();
		lua_pushstring(m_lua, "passwordScreen");
		lua_setglobal(m_lua, "screenState");

		lua_pushboolean(m_lua, false);
		lua_setglobal(m_lua, "passwordError");
		return 0;
	}

	nlohmann::json packet;
	packet["topic"] = "server.room.join";
	packet["id"] = m_roomToJoin;
	packet["password"] = m_textInput->input;
	m_tcp.SendJSON(packet);

	return 0;
}

int MultiplayerScreen::lNewRoomStep(lua_State* L)
{
	if (m_screenState == MultiplayerScreenState::ROOM_LIST)
	{
		m_chatOverlay->DisableOpeningChat();
		m_screenState = MultiplayerScreenState::NEW_ROOM_NAME;
		lua_pushstring(m_lua, "newRoomName");
		lua_setglobal(m_lua, "screenState");

		m_textInput->Reset();
		m_textInput->input = m_userName + "'s Room";
		m_textInput->SetActive(true);
	}
	else if (m_screenState == MultiplayerScreenState::NEW_ROOM_NAME)
	{
		if (m_textInput->input.length() == 0)
		{
			return 0;
		}
		m_newRoomName = m_textInput->input;
		m_textInput->Reset();

		m_screenState = MultiplayerScreenState::NEW_ROOM_PASSWORD;
		lua_pushstring(m_lua, "newRoomPassword");
		lua_setglobal(m_lua, "screenState");

	}
	else if (m_screenState == MultiplayerScreenState::NEW_ROOM_PASSWORD)
	{
		m_chatOverlay->EnableOpeningChat();
		nlohmann::json packet;
		packet["topic"] = "server.room.new";
		packet["name"] = m_newRoomName;
		packet["password"] = m_textInput->input;
		m_tcp.SendJSON(packet);
		m_textInput->Reset();
	}
	return 0;
}

// This is basically copied from song-select
void MultiplayerScreen::m_updatePreview(ChartIndex* diff, bool mapChanged)
{
	String mapRootPath = diff->path.substr(0, diff->path.find_last_of(Path::sep));

	// Set current preview audio
	String audioPath = mapRootPath + Path::sep + diff->preview_file;

	PreviewParams params = {audioPath, static_cast<uint32>(diff->preview_offset), static_cast<uint32>(diff->preview_length)};

	/* A lot of pre-effected charts use different audio files for each difficulty; these
	 * files differ only in their effects, so the preview offset and duration remain the
	 * same. So, if the audio file is different but offset and duration equal the previously
	 * playing preview, we know that it was just a change to a different difficulty of the
	 * same chart. To avoid restarting the preview when changing difficulty, we say that
	 * charts with this setup all have the same preview.
	 *
	 * Note that if the chart is using the `previewfile` field, then all this is ignored. */
	bool newPreview = mapChanged ? m_previewParams != params : (m_previewParams.duration != params.duration || m_previewParams.offset != params.offset);

	if (newPreview)
	{
		Ref<AudioStream> previewAudio = g_audio->CreateStream(audioPath);
		if (previewAudio)
		{
			previewAudio->SetPosition(diff->preview_offset);

			m_previewPlayer.FadeTo(previewAudio, diff->preview_offset);

			m_previewParams = params;
		}
		else
		{
			params = {"", 0, 0};

			Logf("Failed to load preview audio from [%s]", Logger::Severity::Warning, audioPath);
			if (m_previewParams != params)
				m_previewPlayer.FadeTo(Ref<AudioStream>());
		}

		m_previewParams = params;
	}
}

void MultiplayerScreen::m_stopPreview()
{
	PreviewParams params = {"", 0, 0};
	if (m_previewParams != params)
		m_previewPlayer.FadeTo(Ref<AudioStream>());

}
