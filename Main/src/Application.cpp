#include "stdafx.h"
#include "Application.hpp"
#include <Beatmap/Beatmap.hpp>
#include "Game.hpp"
#include "Test.hpp"
#include "SongSelect.hpp"
#include "TitleScreen.hpp"
#include <Audio/Audio.hpp>
#include <Graphics/ResourceManagers.hpp>
#include <Shared/Profiling.hpp>
#include "GameConfig.hpp"
#include "Input.hpp"
#include "TransitionScreen.hpp"
#include "SkinConfig.hpp"
#include "ShadedMesh.hpp"
#include "IR.hpp"

#ifdef EMBEDDED
#define NANOVG_GLES2_IMPLEMENTATION
#else
#define NANOVG_GL3_IMPLEMENTATION
#endif
#include "nanovg_gl.h"
#include "GUI/nanovg_lua.h"
#ifdef _WIN32
#ifdef CRASHDUMP
#include "exception_handler.h"
#include "client_info.h"
#endif
#endif
#include "archive.h"
#include "archive_entry.h"

GameConfig g_gameConfig;
SkinConfig *g_skinConfig;
OpenGL *g_gl = nullptr;
Graphics::Window *g_gameWindow = nullptr;
Application *g_application = nullptr;
JobSheduler *g_jobSheduler = nullptr;
TransitionScreen *g_transition = nullptr;
Input g_input;

// Tickable queue
static Vector<IApplicationTickable *> g_tickables;

struct TickableChange
{
	enum Mode
	{
		Added,
		Removed,
		RemovedNoDelete,
	};
	Mode mode;
	IApplicationTickable *tickable;
	IApplicationTickable *insertBefore;
};
// List of changes applied to the collection of tickables
// Applied at the end of each main loop
static Vector<TickableChange> g_tickableChanges;

// Used to set the initial screen size
static float g_screenHeight = 1000.0f;

// Current screen size
float g_aspectRatio = (16.0f / 9.0f);
Vector2i g_resolution;

static float g_avgRenderDelta = 0.0f;

Application::Application()
{
	// Enforce single instance
	assert(!g_application);
	g_application = this;
}
Application::~Application()
{
	m_Cleanup();
	assert(g_application == this);
	g_application = nullptr;
}
void Application::SetCommandLine(int32 argc, char **argv)
{
	m_commandLine.clear();

	// Split up command line parameters
	for (int32 i = 0; i < argc; i++)
	{
		m_commandLine.Add(argv[i]);
	}
}
void Application::SetCommandLine(const char *cmdLine)
{
	m_commandLine.clear();

	// Split up command line parameters
	m_commandLine = Path::SplitCommandLine(cmdLine);
}
void Application::ApplySettings()
{
	String newskin = g_gameConfig.GetString(GameConfigKeys::Skin);
	if (m_skin != newskin)
	{
		m_needSkinReload = true;
	}
	Logger::Get().SetLogLevel(g_gameConfig.GetEnum<Logger::Enum_Severity>(GameConfigKeys::LogLevel));
	g_gameWindow->SetVSync(g_gameConfig.GetBool(GameConfigKeys::VSync) ? 1 : 0);
	m_showFps = g_gameConfig.GetBool(GameConfigKeys::ShowFps);

	m_UpdateWindowPosAndShape();
	m_OnWindowResized(g_gameWindow->GetWindowSize());
	m_SaveConfig();
}
int32 Application::Run()
{
	if (!m_Init())
		return 1;

	if (m_commandLine.Contains("-test"))
	{
		// Create test scene
		AddTickable(Test::Create());
	}
	else
	{
		bool mapLaunched = false;
		// Play the map specified in the command line
		if (m_commandLine.size() > 1 && m_commandLine[1].front() != '-')
		{
			Game *game = LaunchMap(m_commandLine[1]);
			if (!game)
			{
				Logf("LaunchMap(%s) failed", Logger::Severity::Error, m_commandLine[1]);
			}
			else
			{
				auto &cmdLine = g_application->GetAppCommandLine();
				if (cmdLine.Contains("-autoplay") || cmdLine.Contains("-auto"))
				{
					game->GetScoring().autoplayInfo.autoplay = true;
				}
				mapLaunched = true;
			}
		}

		if (!mapLaunched)
		{
			if (m_commandLine.Contains("-notitle"))
			{
				SongSelect *ss = SongSelect::Create();
				ss->AsyncLoad();
				ss->AsyncFinalize();
				AddTickable(ss);
			}
			else // Start regular game, goto title screen
				AddTickable(TitleScreen::Create());
		}
	}

	m_MainLoop();

	return 0;
}

void Application::SetUpdateAvailable(const String &version, const String &url, const String &download)
{
	m_updateVersion = version;
	m_updateUrl = url;
	m_updateDownload = download;
	m_hasUpdate = true;
}


void Application::RunUpdater()
{
#ifdef _WIN32

	//HANDLE handle = GetCurrentProcess();
	//HANDLE handledup;

	//DuplicateHandle(GetCurrentProcess(),
	//	handle,
	//	GetCurrentProcess(),
	//	&handledup,
	//	SYNCHRONIZE,
	//	FALSE,
	//	0);

	/// TODO: use process handle instead of pid to wait
	
	String arguments = Utility::Sprintf("%lld %s", GetCurrentProcessId(), *m_updateDownload);
	Path::Run(Path::Absolute("updater.exe"), *arguments);
	Shutdown();
#endif
}

void Application::ForceRender()
{
	nvgEndFrame(g_guiState.vg);
	g_application->GetRenderQueueBase()->Process();
	nvgBeginFrame(g_guiState.vg, g_resolution.x, g_resolution.y, 1);
}

NVGcontext *Application::GetVGContext()
{
	return g_guiState.vg;
}

Vector<String> Application::GetUpdateAvailable()
{
	if (m_hasUpdate)
	{
		return Vector<String>{m_updateUrl, m_updateVersion};
	}

	return Vector<String>();
}

int copyArchiveData(archive *ar, archive *aw)
{
	int r;
	const void *buff;
	size_t size;
	la_int64_t offset;

	for (;;)
	{
		r = archive_read_data_block(ar, &buff, &size, &offset);
		if (r == ARCHIVE_EOF)
			return (ARCHIVE_OK);
		if (r < ARCHIVE_OK)
			return (r);
		r = archive_write_data_block(aw, buff, size, offset);
		if (r < ARCHIVE_OK)
		{
			return (r);
		}
	}
}

// Extract .usc-skin files in the skin directory
void Application::m_unpackSkins()
{
	bool interrupt = false;
	Vector<FileInfo> files = Files::ScanFiles(
		Path::Absolute("skins/"), "usc-skin", &interrupt);
	if (interrupt)
		return;

	for (FileInfo &fi : files)
	{
		Logf("[Archive] Extracting skin '%s'", Logger::Severity::Info, fi.fullPath);

		// Init archive structs
		archive *a = archive_read_new();
		archive *ext = archive_write_disk_new();
		archive_entry *entry;

		archive_read_support_filter_all(a);
		archive_read_support_format_all(a);

		// Setup archive to write to disk
		archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);
		archive_write_disk_set_standard_lookup(ext);

		// Read the file twice so we can determin if there is a single file
		// in the top level or multiple files

		int res = archive_read_open_filename(a, fi.fullPath.c_str(), 10240);
		if (res != ARCHIVE_OK)
		{
			Logf("[Archive] Error reading skin archive '%s'", Logger::Severity::Error,
				 archive_error_string(a));
			archive_read_close(a);
			archive_read_free(a);
			archive_write_free(ext);
			continue;
		}

		bool singleDir = false;
		bool otherFiles = false;

		for (;;)
		{
			res = archive_read_next_header(a, &entry);
			if (res == ARCHIVE_EOF)
				break;

			String currentFile = archive_entry_pathname(entry);
			size_t slashIndex = currentFile.find("/"); // Seems to be / on both unix and windows

			if (slashIndex == String::npos)
			{
				// This is a non directory in the root
				otherFiles = true;
				if (singleDir)
					singleDir = false;
			}
			// Check if first slash is in last position
			else if (slashIndex == currentFile.length() - 1)
			{
				if (singleDir) // This is the second dir we have seen
					singleDir = false;
				else if (!otherFiles) // First file we have seen
					singleDir = true;

				otherFiles = true;
			}

			archive_read_data_skip(a);
		}

		// Reset the archive
		archive_read_close(a);
		archive_read_free(a);
		a = archive_read_new();

		archive_read_support_filter_all(a);
		archive_read_support_format_all(a);

		// This time we will actually extract
		res = archive_read_open_filename(a, fi.fullPath.c_str(), 10240);
		if (res != ARCHIVE_OK)
		{
			Logf("[Archive] Error reading skin archive '%s'", Logger::Severity::Error,
				 archive_error_string(a));
			archive_read_close(a);
			archive_read_free(a);
			archive_write_free(ext);
			continue;
		}

		// Use the zip name as the directory if there is no single dir
		String dest = Path::Absolute("skins/");
		if (!singleDir)
			dest = fi.fullPath.substr(0, fi.fullPath.length() - 9) + Path::sep;

		bool extractOk = true;

		for (;;)
		{
			// Read the header
			res = archive_read_next_header(a, &entry);
			if (res == ARCHIVE_EOF)
				break;
			if (res < ARCHIVE_OK)
				Logf("[Archive] Error reading skin archive '%s'", Logger::Severity::Error,
					 archive_error_string(a));
			if (res < ARCHIVE_WARN)
			{
				extractOk = false;
				break;
			}

			// Update the path to our dest
			const char *currentFile = archive_entry_pathname(entry);
			const std::string fullOutputPath = dest + currentFile;

			const String dot_dot_win = "..\\";
			const String dot_dot_unix = "../";

			// Check for zipslip
			if (fullOutputPath.find(dot_dot_win) != String::npos)
			{
				Logf("[Archive] Error reading skin archive: '%s' can't appear in file name '%s'", Logger::Severity::Error, dot_dot_win.c_str(), fullOutputPath.c_str());
				extractOk = false;
				break;
			}
			if (fullOutputPath.find(dot_dot_unix) != String::npos)
			{
				Logf("[Archive] Error reading skin archive: '%s' can't appear in file name '%s'", Logger::Severity::Error, dot_dot_unix.c_str(), fullOutputPath.c_str());
				extractOk = false;
				break;
			}

			archive_entry_set_pathname(entry, fullOutputPath.c_str());

			// Write the new header
			res = archive_write_header(ext, entry);
			if (res < ARCHIVE_OK)
				Logf("[Archive] Error writing skin archive '%s'", Logger::Severity::Error,
					 archive_error_string(ext));
			else if (archive_entry_size(entry) > 0)
			{
				// Copy the data so it will be extracted
				res = copyArchiveData(a, ext);
				if (res < ARCHIVE_OK)
					Logf("[Archive] Error writing skin archive '%s'", Logger::Severity::Error,
						 archive_error_string(ext));
				if (res < ARCHIVE_WARN)
				{
					extractOk = false;
					break;
				}
			}
			res = archive_write_finish_entry(ext);
			if (res < ARCHIVE_OK)
				Logf("[Archive] Error writing skin archive '%s'", Logger::Severity::Error,
					 archive_error_string(ext));
			if (res < ARCHIVE_WARN)
			{
				extractOk = false;
				break;
			}
		}

		// Free and close everything
		archive_read_close(a);
		archive_read_free(a);
		archive_write_close(ext);
		archive_write_free(ext);

		// If we extracted everything alright, we can remove the file
		if (extractOk)
		{
			Path::Delete(fi.fullPath);
		}
	}
}

bool Application::ReloadConfig(const String& profile)
{
	return m_LoadConfig(profile);
}

bool Application::m_LoadConfig(String profileName /* must be by value */)
{

	bool successful = false;

	String configPath = "Main.cfg";
	File mainConfigFile;
	if (mainConfigFile.OpenRead(Path::Absolute(configPath)))
	{
		FileReader reader(mainConfigFile);
		successful = g_gameConfig.Load(reader);
		mainConfigFile.Close();
	}
	else
	{
        // Clear here to apply defaults
        g_gameConfig.Clear();
		g_gameConfig.Set(GameConfigKeys::ConfigVersion, GameConfig::VERSION);
	}

	if (profileName == "")
		profileName = g_gameConfig.GetString(GameConfigKeys::CurrentProfileName);

	// First load main config over
	if (profileName == "Main") {
		// If only loading main, then we are done
		g_gameConfig.Set(GameConfigKeys::CurrentProfileName, profileName);
		return successful;
	}

	// Otherwise we are going to load the profile information over top
	configPath = Path::Normalize("profiles/" + profileName + ".cfg");

	File profileConfigFile;
	if (profileConfigFile.OpenRead(Path::Absolute(configPath)))
	{
		FileReader reader(profileConfigFile);
		successful |= g_gameConfig.Load(reader, false); // Do not reset

		profileConfigFile.Close();
	}
    else
    {
		// We couldn't load this, but we are not going to do anything about it
		successful = false;
    }

	g_gameConfig.Set(GameConfigKeys::CurrentProfileName, profileName);
	return successful;
}

void Application::m_UpdateConfigVersion()
{
	g_gameConfig.UpdateVersion();
}

void Application::m_SaveConfig()
{
	if (!g_gameConfig.IsDirty())
		return;

	String profile = g_gameConfig.GetString(GameConfigKeys::CurrentProfileName);
	String configPath = "Main.cfg";
	if (profile == "Main")
	{
		//Save everything into main.cfg
		File configFile;
		if (configFile.OpenWrite(Path::Absolute(configPath)))
		{
			FileWriter writer(configFile);
			g_gameConfig.Save(writer);
			configFile.Close();
		}
		return;
	}
	// We are going to save the config excluding profile settings
	{
		GameConfig tmp_gc;
		{
			// First load the current Main.cfg
			File configFile;
			if (configFile.OpenRead(Path::Absolute(configPath)))
			{
				FileReader reader(configFile);
				tmp_gc.Load(reader);
				configFile.Close();
			}
			else
			{
				tmp_gc.Clear();
			}
		}

		// Now merge our new settings (ignoring profile settings)
		tmp_gc.Update(g_gameConfig, &GameConfigProfileSettings);

		// Finally save the updated version to file
		File configFile;
		if (configFile.OpenWrite(Path::Absolute(configPath)))
		{
			FileWriter writer(configFile);
			tmp_gc.Save(writer);
			configFile.Close();
		}
	}

	// Now save the profile only settings
	configPath = Path::Normalize("profiles/" + profile + ".cfg");

	GameConfig tmp_gc;
	{
		// First load the current profile (including extra settings)
		File configFile;
		if (configFile.OpenRead(Path::Absolute(configPath)))
		{
			FileReader reader(configFile);
			tmp_gc.Load(reader);
			configFile.Close();
		}
		else
		{
			tmp_gc.Clear();
		}
	}

	// Now merge our new settings (only profile settings)
	tmp_gc.Update(g_gameConfig, nullptr, &GameConfigProfileSettings);

	// If there are any extra keys in the profile config, add them
	ConfigBase::KeyList toSave(GameConfigProfileSettings);
	for (uint32 k : tmp_gc.GetKeysInFile())
	{
		toSave.insert(k);
	}

	File configFile;
	if (configFile.OpenWrite(Path::Absolute(configPath)))
	{
		FileWriter writer(configFile);
		tmp_gc.Save(writer, nullptr, &toSave);
		configFile.Close();
	}
}

void __discordError(int errorCode, const char *message)
{
	g_application->DiscordError(errorCode, message);
}

void __discordReady(const DiscordUser *user)
{
	Logf("[Discord] Logged in as \"%s\"", Logger::Severity::Info, user->username);
}

void __discordJoinGame(const char *joins)
{
	g_application->JoinMultiFromInvite(joins);
}

void __discordSpecGame(const char *specs)
{
}

void __discordJoinReq(const DiscordUser *duser)
{
}

void __discordDisconnected(int errcode, const char *msg)
{
	g_application->DiscordError(errcode, msg);
}

void __updateChecker()
{
	// Handle default config or old config
	if (g_gameConfig.GetBool(GameConfigKeys::OnlyRelease))
	{
		g_gameConfig.Set(GameConfigKeys::UpdateChannel, "release");
		g_gameConfig.Set(GameConfigKeys::OnlyRelease, false);
	}

	String channel = g_gameConfig.GetString(GameConfigKeys::UpdateChannel);

    // For some reason the github actions have the branch as HEAD?
    if (channel == "HEAD")
    {
		g_gameConfig.Set(GameConfigKeys::UpdateChannel, "master");
    }

	ProfilerScope $1("Check for updates");
	if (channel == "release")
	{
		auto r = cpr::Get(cpr::Url{"https://api.github.com/repos/drewol/unnamed-sdvx-clone/releases/latest"});

		Logf("Update check status code: %d", Logger::Severity::Normal, r.status_code);
		if (r.status_code != 200)
		{
			Logf("Failed to get update information: %s", Logger::Severity::Error, r.error.message.c_str());
		}
		else
		{
			nlohmann::json latestInfo;
			///TODO: Don't use exceptions
			try
			{
				latestInfo = nlohmann::json::parse(r.text);
			}
			catch (const std::exception &e)
			{
				Logf("Failed to parse version json: \"%s\"", Logger::Severity::Error, e.what());
				return;
			}

			//tag_name should always be "vX.Y.Z" so we remove the 'v'
			String tagname;
			latestInfo.at("tag_name").get_to(tagname);
			tagname = tagname.substr(1);
			bool outdated = false;
			Vector<String> versionStrings = tagname.Explode(".");
			int major = 0, minor = 0, patch = 0;
			major = std::stoi(versionStrings[0]);
			if (versionStrings.size() > 1)
				minor = std::stoi(versionStrings[1]);
			if (versionStrings.size() > 2)
				patch = std::stoi(versionStrings[2]);

			outdated = major > VERSION_MAJOR || minor > VERSION_MINOR || patch > VERSION_PATCH;

			if (outdated)
			{
				String updateUrl;
				latestInfo.at("html_url").get_to(updateUrl);
				String updateDownload;
				latestInfo.at("assets").at(0).at("browser_download_url").get_to(updateDownload);
				g_application->SetUpdateAvailable(tagname, updateUrl, updateDownload);
			}
		}
	}
	else
	{
#ifdef GIT_COMMIT
		auto response = cpr::Get(cpr::Url{"https://api.github.com/repos/drewol/unnamed-sdvx-clone/actions/runs"});
		if (response.status_code != 200)
		{
			Logf("Failed to get update information: %s", Logger::Severity::Error, response.error.message.c_str());
			return;
		}

		auto commits = nlohmann::json::parse(response.text);
		String current_hash;
		String(GIT_COMMIT).Split("_", nullptr, &current_hash);
		String current_branch = channel;

		if (commits.contains("message"))
		{
			String errormsg;
			commits.at("message").get_to(errormsg);
			Logf("Failed to get update information: %s", Logger::Severity::Warning, *errormsg);
			return;
		}

		commits = commits.at("workflow_runs");
		for (auto &commit_kvp : commits.items())
		{
			auto commit = commit_kvp.value();

			String branch;
			commit.at("head_branch").get_to(branch);
			String status;
			commit.at("status").get_to(status);
			String conclusion;
			if (commit.at("conclusion").is_null())
			{
				//not built yet
				continue;
			}
			commit.at("conclusion").get_to(conclusion);

			if (branch == current_branch && status == "completed" && conclusion == "success")
			{
				String new_hash;
				commit.at("head_sha").get_to(new_hash);
				if (current_hash == new_hash.substr(0, current_hash.length())) //up to date
				{
					return;
				}
				else //update available
				{
					auto response = cpr::Get(cpr::Url{"https://api.github.com/repos/drewol/unnamed-sdvx-clone/commits/" + new_hash});
					String updateUrl = "https://github.com/drewol/unnamed-sdvx-clone";
					if (response.status_code != 200)
					{
						Logf("Failed to get update information: %s", Logger::Severity::Warning, response.error.message.c_str());
					}
					else
					{
						auto commit_status = nlohmann::json::parse(response.text);
						commit_status.at("html_url").get_to(updateUrl);
					}
					if (current_branch == "master")
						g_application->SetUpdateAvailable(new_hash.substr(0, 7), updateUrl, "http://drewol.me/Downloads/Game.zip");
					else
						g_application->SetUpdateAvailable(new_hash.substr(0, 7), updateUrl, "https://builds.drewol.me/" + current_branch + "/Game");
					return;
				}
			}
		}
#endif
	}
}

void Application::CheckForUpdate()
{
	m_hasUpdate = false;
	if (g_gameConfig.GetBool(GameConfigKeys::CheckForUpdates))
	{
		if (m_updateThread.joinable())
			m_updateThread.join();
		m_updateThread = Thread(__updateChecker);
	}
}

void Application::m_InitDiscord()
{
	ProfilerScope $("Discord RPC Init");
	DiscordEventHandlers dhe;
	memset(&dhe, 0, sizeof(dhe));
	dhe.errored = __discordError;
	dhe.ready = __discordReady;
	dhe.joinRequest = __discordJoinReq;
	dhe.spectateGame = __discordSpecGame;
	dhe.joinGame = __discordJoinGame;
	dhe.disconnected = __discordDisconnected;
	Discord_Initialize(DISCORD_APPLICATION_ID, &dhe, 1, nullptr);
}

bool Application::m_Init()
{
	ProfilerScope $("Application Setup");

	String version = Utility::Sprintf("%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
	Logf("Version: %s", Logger::Severity::Info, version.c_str());

#ifdef EMBEDDED
	Log("Embeedded version.");
#endif

#ifdef GIT_COMMIT
	Logf("Git commit: %s", Logger::Severity::Info, GIT_COMMIT);
#endif // GIT_COMMIT

#ifdef _WIN32
#ifdef CRASHDUMP
	google_breakpad::CustomInfoEntry kCustomInfoEntries[]{
		google_breakpad::CustomInfoEntry(L"version", std::wstring(version.begin(), version.end()).c_str()),
#ifdef GIT_COMMIT
		google_breakpad::CustomInfoEntry(L"git", L"" GIT_COMMIT),
#else
		CustomInfoEntry("git", ""),
#endif
	};
	google_breakpad::CustomClientInfo custom_info = {kCustomInfoEntries, 2};
	//CustomClientInfo custom_info
	auto handler = new google_breakpad::ExceptionHandler(
		L".\\crash_dumps",
		NULL,
		NULL,
		NULL,
		google_breakpad::ExceptionHandler::HANDLER_ALL,
		MiniDumpNormal,
		(const wchar_t *)nullptr,
		&custom_info);
#endif
#endif

	// Must have command line
	assert(m_commandLine.size() >= 1);

	// Flags read _before_ config load
	for (auto &cl : m_commandLine)
	{
		String k, v;
		if (cl.Split("=", &k, &v))
		{
			if (k == "-gamedir")
			{
				Path::gameDir = v;
			}
		}
	}

	// Set the locale so that functions such as `fopen` use UTF-8.
	{
		String prevLocale = setlocale(LC_CTYPE, nullptr);
		setlocale(LC_CTYPE, ".UTF-8");

		Logf("The locale was changed from %s to %s", Logger::Severity::Info, prevLocale.c_str(), setlocale(LC_CTYPE, nullptr));
	}

	// Load config
	if (!m_LoadConfig())
		Log("Failed to load config file", Logger::Severity::Warning);

	// Job sheduler
	g_jobSheduler = new JobSheduler();

	m_allowMapConversion = false;
	bool debugMute = false;
	bool startFullscreen = false;
	int32 fullscreenMonitor = -1;

	// Fullscreen settings from config
	if (g_gameConfig.GetBool(GameConfigKeys::Fullscreen))
		startFullscreen = true;

	fullscreenMonitor = g_gameConfig.GetInt(GameConfigKeys::FullscreenMonitorIndex);

	// Flags read _after_ config load
	for (auto &cl : m_commandLine)
	{
		String k, v;
		if (cl.Split("=", &k, &v))
		{
			if (k == "-monitor")
			{
				fullscreenMonitor = atol(*v);
			}
		}
		else
		{
			if (cl == "-convertmaps")
			{
				m_allowMapConversion = true;
			}
			else if (cl == "-mute")
			{
				debugMute = true;
			}
			else if (cl == "-fullscreen")
			{
				startFullscreen = true;
			}
		}
	}

	// Init font library
	if (!Graphics::FontRes::InitLibrary())
		return false;

	// Create the game window
	g_resolution = Vector2i(
		g_gameConfig.GetInt(GameConfigKeys::ScreenWidth),
		g_gameConfig.GetInt(GameConfigKeys::ScreenHeight));
	g_aspectRatio = (float)g_resolution.x / (float)g_resolution.y;

	int samplecount = g_gameConfig.GetInt(GameConfigKeys::AntiAliasing);
	if (samplecount > 0)
		samplecount = 1 << samplecount;

	g_gameWindow = new Graphics::Window(g_resolution, samplecount);

	// Versioning up config uses some SDL util functions, so it must be called after SDL is initialized.
	// SDL is initialized in the constructor of Graphics::Window.
	// The awkward placement of this call may be avoided by initializing SDL earlier, but to avoid unwanted side-effects I'll put this here for now.
	this->m_UpdateConfigVersion();

	g_gameWindow->Show();

	g_gameWindow->OnKeyPressed.Add(this, &Application::m_OnKeyPressed);
	g_gameWindow->OnKeyReleased.Add(this, &Application::m_OnKeyReleased);
	g_gameWindow->OnResized.Add(this, &Application::m_OnWindowResized);
	g_gameWindow->OnMoved.Add(this, &Application::m_OnWindowMoved);
	g_gameWindow->OnFocusChanged.Add(this, &Application::m_OnFocusChanged);

	// Initialize Input
	g_input.Init(*g_gameWindow);

	m_unpackSkins();

	// Set skin variable
	m_skin = g_gameConfig.GetString(GameConfigKeys::Skin);

	// Fallback to default if not found
	if (!Path::FileExists(Path::Absolute("skins/" + m_skin)))
	{
		m_skin = "Default";
		g_gameConfig.Set(GameConfigKeys::Skin, m_skin);
	}

	g_skinConfig = new SkinConfig(m_skin);

	// Window cursor
	Image cursorImg = ImageRes::Create(Path::Absolute("skins/" + m_skin + "/textures/cursor.png"));
	g_gameWindow->SetCursor(cursorImg, Vector2i(5, 5));

	m_UpdateWindowPosAndShape(fullscreenMonitor, startFullscreen, g_gameConfig.GetBool(GameConfigKeys::AdjustWindowPositionOnStartup));

	// Set render state variables
	m_renderStateBase.aspectRatio = g_aspectRatio;
	m_renderStateBase.viewportSize = g_resolution;
	m_renderStateBase.time = 0.0f;

	{
		ProfilerScope $1("Audio Init");

		// Init audio
		new Audio();
		bool exclusive = g_gameConfig.GetBool(GameConfigKeys::WASAPI_Exclusive);
		if (!g_audio->Init(exclusive))
		{
			if (exclusive)
			{
				Log("Failed to open in WASAPI Exclusive mode, attempting shared mode.", Logger::Severity::Warning);
				g_gameWindow->ShowMessageBox("WASAPI Exclusive mode error.", "Failed to open in WASAPI Exclusive mode, attempting shared mode.", 1);
				if (!g_audio->Init(false))
				{
					Log("Audio initialization failed", Logger::Severity::Error);
					delete g_audio;
					return false;
				}
			}
			else
			{
				Log("Audio initialization failed", Logger::Severity::Error);
				delete g_audio;
				return false;
			}
		}

		// Debug Mute?
		// Test tracks may get annoying when continously debugging ;)
		if (debugMute)
		{
			g_audio->SetGlobalVolume(0.0f);
		}
	}

	{
		ProfilerScope $1("GL Init");

		// Create graphics context
		g_gl = new OpenGL();
		if (!g_gl->Init(*g_gameWindow, g_gameConfig.GetInt(GameConfigKeys::AntiAliasing)))
		{
			Log("Failed to create OpenGL context", Logger::Severity::Error);
			return false;
		}
#ifdef EMBEDDED
#ifdef _DEBUG
		g_guiState.vg = nvgCreateGLES2(NVG_DEBUG);
#else
		g_guiState.vg = nvgCreateGLES2(0);
#endif
#else
#ifdef _DEBUG
		g_guiState.vg = nvgCreateGL3(NVG_DEBUG);
#else
		g_guiState.vg = nvgCreateGL3(0);
#endif
#endif
		nvgCreateFont(g_guiState.vg, "fallback", *Path::Absolute("fonts/NotoSansCJKjp-Regular.otf"));
	}

	CheckForUpdate();


	m_InitDiscord();

	CheckedLoad(m_fontMaterial = LoadMaterial("font"));
	m_fontMaterial->opaque = false;
	CheckedLoad(m_fillMaterial = LoadMaterial("guiColor"));
	m_fillMaterial->opaque = false;
	CheckedLoad(m_guiTex = LoadMaterial("guiTex"));
	m_guiTex->opaque = false;


	//m_skinHtpp = new SkinHttp();
	// call the initial OnWindowResized now that we have intialized OpenGL
	m_OnWindowResized(g_resolution);

	m_showFps = g_gameConfig.GetBool(GameConfigKeys::ShowFps);
	g_gameWindow->SetVSync(g_gameConfig.GetBool(GameConfigKeys::VSync) ? 1 : 0);

	{
		ProfilerScope $("Load Transition Screens");
		g_transition = TransitionScreen::Create();
	}

	///TODO: check if directory exists already?
	Path::CreateDir(Path::Absolute("screenshots"));
	Path::CreateDir(Path::Absolute("songs"));
	Path::CreateDir(Path::Absolute("replays"));
	Path::CreateDir(Path::Absolute("crash_dumps"));
	Logger::Get().SetLogLevel(g_gameConfig.GetEnum<Logger::Enum_Severity>(GameConfigKeys::LogLevel));
	return true;
}
void Application::m_MainLoop()
{
	Timer appTimer;
	m_deltaTime = 0.5f;
	Timer frameTimer;
	while (true)
	{
		m_appTime = appTimer.SecondsAsFloat();
		frameTimer.Restart();
		//run discord callbacks
		Discord_RunCallbacks();

		// Process changes in the list of items
		bool restoreTop = false;
		for (auto &ch : g_tickableChanges)
		{
			if (ch.mode == TickableChange::Added)
			{
				assert(ch.tickable);
				if (!ch.tickable->DoInit())
				{
					Log("Failed to add IApplicationTickable", Logger::Severity::Error);
					delete ch.tickable;
					continue;
				}

				if (!g_tickables.empty())
					g_tickables.back()->m_Suspend();

				auto insertionPoint = g_tickables.end();
				if (ch.insertBefore)
				{
					// Find insertion point
					for (insertionPoint = g_tickables.begin(); insertionPoint != g_tickables.end(); insertionPoint++)
					{
						if (*insertionPoint == ch.insertBefore)
							break;
					}
				}
				g_tickables.insert(insertionPoint, ch.tickable);

				restoreTop = true;
			}
			else if (ch.mode == TickableChange::Removed || ch.mode == TickableChange::RemovedNoDelete)
			{
				// Remove focus
				ch.tickable->m_Suspend();

				assert(!g_tickables.empty());
				if (g_tickables.back() == ch.tickable)
					restoreTop = true;
				g_tickables.Remove(ch.tickable);
				if (ch.mode == TickableChange::Removed)
					delete ch.tickable;
			}
		}
		if (restoreTop && !g_tickables.empty())
			g_tickables.back()->m_Restore();

		// Application should end, no more active screens
		if (!g_tickableChanges.empty() && g_tickables.empty())
		{
			Log("No more IApplicationTickables, shutting down", Logger::Severity::Warning);
			return;
		}
		g_tickableChanges.clear();

		// Determine target tick rates for update and render
		int32 targetFPS = 120; // Default to 120 FPS
		uint32 targetRenderTime = 0;
		for (auto tickable : g_tickables)
		{
			int32 tempTarget = 0;
			if (tickable->GetTickRate(tempTarget))
			{
				targetFPS = tempTarget;
			}
		}
		if (targetFPS > 0)
			targetRenderTime = 1000000 / targetFPS;

		// Main loop
		float currentTime = appTimer.SecondsAsFloat();

		g_avgRenderDelta = g_avgRenderDelta * 0.98f + m_deltaTime * 0.02f; // Calculate avg

		// Set time in render state
		m_renderStateBase.time = currentTime;

		// Also update window in render loop
		if (!g_gameWindow->Update())
			return;

		m_Tick();

		// Garbage collect resources
		ResourceManagers::TickAll();

		// Tick job sheduler
		// processed callbacks for finished tasks
		g_jobSheduler->Update();

		//This FPS limiter seems unstable over 500fps
		uint32 frameTime = frameTimer.Microseconds();
		if (frameTime < targetRenderTime)
		{
			uint32 timeLeft = (targetRenderTime - frameTime);
			uint32 sleepMicroSecs = (uint32)(timeLeft * m_fpsTargetSleepMult * 0.75);
			if (sleepMicroSecs > 1000)
			{
				uint32 sleepStart = frameTimer.Microseconds();
				std::this_thread::sleep_for(std::chrono::microseconds(sleepMicroSecs));
				float actualSleep = frameTimer.Microseconds() - sleepStart;

				m_fpsTargetSleepMult += ((float)timeLeft - (float)actualSleep / 0.75) / 500000.f;
				m_fpsTargetSleepMult = Math::Clamp(m_fpsTargetSleepMult, 0.0f, 1.0f);
			}

			do
			{
				std::this_thread::yield();
			} while (frameTimer.Microseconds() < targetRenderTime);
		}
		// Swap buffers
		g_gl->SwapBuffers();

		m_deltaTime = frameTimer.SecondsAsFloat();
	}
}

void Application::m_Tick()
{
	// Handle input first
	g_input.Update(m_deltaTime);

	// Process async lua http callbacks
	m_skinHttp.ProcessCallbacks();

	// likewise for IR
	m_skinIR.ProcessCallbacks();

	// Tick all items
	for (auto &tickable : g_tickables)
	{
		tickable->Tick(m_deltaTime);
	}

	// Not minimized / Valid resolution
	if (g_resolution.x > 0 && g_resolution.y > 0)
	{
		//Clear out opengl errors
		GLenum glErr = glGetError();
		while (glErr != GL_NO_ERROR)
		{
			Logf("OpenGL Error: %p", Logger::Severity::Debug, glErr);
			glErr = glGetError();
		}

		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		nvgBeginFrame(g_guiState.vg, g_resolution.x, g_resolution.y, 1);
		m_renderQueueBase = RenderQueue(g_gl, m_renderStateBase);
		g_guiState.rq = &m_renderQueueBase;
		g_guiState.t = Transform();
		g_guiState.fontMaterial = &m_fontMaterial;
		g_guiState.fillMaterial = &m_fillMaterial;
		g_guiState.resolution = g_resolution;

		if (g_gameConfig.GetBool(GameConfigKeys::ForcePortrait))
			g_guiState.scissorOffset = g_gameWindow->GetWindowSize().x / 2 - g_resolution.x / 2;
		else
			g_guiState.scissorOffset = 0;

		g_guiState.scissor = Rect(0, 0, -1, -1);
		g_guiState.imageTint = nvgRGB(255, 255, 255);
		// Render all items
		assert(!g_tickables.empty());
		for (auto &tickable : g_tickables)
		{
			tickable->Render(m_deltaTime);
		}
		m_renderStateBase.projectionTransform = GetGUIProjection();
		if (m_showFps)
		{
			nvgReset(g_guiState.vg);
			nvgBeginPath(g_guiState.vg);
			nvgFontFace(g_guiState.vg, "fallback");
			nvgFontSize(g_guiState.vg, 20);
			nvgTextAlign(g_guiState.vg, NVG_ALIGN_RIGHT);
			nvgFillColor(g_guiState.vg, nvgRGB(0, 200, 255));
			String fpsText = Utility::Sprintf("%.1fFPS", GetRenderFPS());
			nvgText(g_guiState.vg, g_resolution.x - 5, g_resolution.y - 5, fpsText.c_str(), 0);
			// Visualize m_fpsTargetSleepMult for debugging
			//nvgBeginPath(g_guiState.vg);
			//float h = m_fpsTargetSleepMult * g_resolution.y;
			//nvgRect(g_guiState.vg, g_resolution.x - 10, g_resolution.y - h, 10, h);
			//nvgFill(g_guiState.vg);
		}
		nvgEndFrame(g_guiState.vg);
		m_renderQueueBase.Process();
		glCullFace(GL_FRONT);
	}

	if (m_needSkinReload)
	{
		m_needSkinReload = false;
		ReloadSkin();
	}
}

void Application::m_Cleanup()
{
	ProfilerScope $("Application Cleanup");

	for (auto it : g_tickables)
	{
		delete it;
	}
	g_tickables.clear();

	if (g_audio)
	{
		delete g_audio;
		g_audio = nullptr;
	}

	if (g_gl)
	{
		delete g_gl;
		g_gl = nullptr;
	}

	// Cleanup input
	g_input.Cleanup();

	// Cleanup window after this
	if (g_gameWindow)
	{
		delete g_gameWindow;
		g_gameWindow = nullptr;
	}

	if (g_jobSheduler)
	{
		delete g_jobSheduler;
		g_jobSheduler = nullptr;
	}

	if (g_skinConfig)
	{
		delete g_skinConfig;
		g_skinConfig = nullptr;
	}

	if (g_transition)
	{
		delete g_transition;
		g_transition = nullptr;
	}

	//if (m_skinHtpp)
	//{
	//	delete m_skinHtpp;
	//	m_skinHtpp = nullptr;
	//}

	for (auto img : m_jacketImages)
	{
		delete img.second;
	}

	// Clear fonts before freeing library

	for (auto &f : g_guiState.fontCahce)
	{
		f.second.reset();
	}
	g_guiState.currentFont.reset();

	m_fonts.clear();

	Discord_Shutdown();

#ifdef EMBEDDED
	nvgDeleteGLES2(g_guiState.vg);
#else
	nvgDeleteGL3(g_guiState.vg);
#endif

	Graphics::FontRes::FreeLibrary();
	if (m_updateThread.joinable())
		m_updateThread.join();

	// Finally, save config
	m_SaveConfig();
}

class Game *Application::LaunchMap(const String &mapPath)
{
	PlaybackOptions opt;
	Game *game = Game::Create(mapPath, opt);
	g_transition->TransitionTo(game);
	return game;
}
void Application::Shutdown()
{
	g_gameWindow->Close();
}

void Application::AddTickable(class IApplicationTickable *tickable, class IApplicationTickable *insertBefore)
{
	Log("Adding tickable", Logger::Severity::Debug);

	TickableChange &change = g_tickableChanges.Add();
	change.mode = TickableChange::Added;
	change.tickable = tickable;
	change.insertBefore = insertBefore;
}
void Application::RemoveTickable(IApplicationTickable *tickable, bool noDelete)
{
	Logf("Removing tickable: %s", Logger::Severity::Debug, noDelete ? "NoDelete" : "Delete");

	TickableChange &change = g_tickableChanges.Add();
	if (noDelete)
	{
		change.mode = TickableChange::RemovedNoDelete;
	}
	else
	{
		change.mode = TickableChange::Removed;
	}
	change.tickable = tickable;
}

String Application::GetCurrentMapPath()
{
	return m_lastMapPath;
}

String Application::GetCurrentSkin()
{
	return m_skin;
}

const Vector<String> &Application::GetAppCommandLine() const
{
	return m_commandLine;
}
RenderState Application::GetRenderStateBase() const
{
	return m_renderStateBase;
}

RenderQueue *Application::GetRenderQueueBase()
{
	return &m_renderQueueBase;
}

Graphics::Image Application::LoadImage(const String &name)
{
	String path = String("skins/") + m_skin + String("/textures/") + name;
	return ImageRes::Create(Path::Absolute(path));
}

Graphics::Image Application::LoadImageExternal(const String &name)
{
	return ImageRes::Create(name);
}
Texture Application::LoadTexture(const String &name)
{
	Texture ret = TextureRes::Create(g_gl, LoadImage(name));
	return ret;
}

Texture Application::LoadTexture(const String &name, const bool &external)
{
	if (external)
	{
		Texture ret = TextureRes::Create(g_gl, LoadImageExternal(name));
		return ret;
	}
	else
	{
		Texture ret = TextureRes::Create(g_gl, LoadImage(name));
		return ret;
	}
}
Material Application::LoadMaterial(const String &name, const String &path)
{
	String pathV = path + name + ".vs";
	String pathF = path + name + ".fs";
	String pathG = path + name + ".gs";
	pathV = Path::Absolute(pathV);
	pathF = Path::Absolute(pathF);
	pathG = Path::Absolute(pathG);
	Material ret = MaterialRes::Create(g_gl, pathV, pathF);
	// Additionally load geometry shader
	if (Path::FileExists(pathG))
	{
		Shader gshader = ShaderRes::Create(g_gl, ShaderType::Geometry, pathG);
		assert(gshader);
		ret->AssignShader(ShaderType::Geometry, gshader);
	}
	assert(ret);
	return ret;
}
Material Application::LoadMaterial(const String &name)
{
	return LoadMaterial(name, String("skins/") + m_skin + String("/shaders/"));
}
Sample Application::LoadSample(const String &name, const bool &external)
{
	String path;
	if (external)
		path = name;
	else
		path = Path::Absolute(String("skins/") + m_skin + String("/audio/") + name);

	path = Path::Normalize(path);
	String ext = Path::GetExtension(path);
	if (ext.empty())
		path += ".wav";

	Sample ret = g_audio->CreateSample(path);
	//assert(ret);
	return ret;
}

Graphics::Font Application::LoadFont(const String &name, const bool &external)
{
	Graphics::Font *cached = m_fonts.Find(name);
	if (cached)
		return *cached;

	String path;
	if (external)
		path = name;
	else
		path = String("skins/") + m_skin + String("/fonts/") + name;

	Graphics::Font newFont = FontRes::Create(g_gl, path);
	m_fonts.Add(name, newFont);
	return newFont;
}

int Application::LoadImageJob(const String &path, Vector2i size, int placeholder, const bool &web)
{
	int ret = placeholder;
	auto it = m_jacketImages.find(path);
	if (it == m_jacketImages.end() || !it->second)
	{
		CachedJacketImage *newImage = new CachedJacketImage();
		JacketLoadingJob *job = new JacketLoadingJob();
		job->imagePath = path;
		job->target = newImage;
		job->w = size.x;
		job->h = size.y;
		job->web = web;
		newImage->loadingJob = Ref<JobBase>(job);
		newImage->lastUsage = m_jobTimer.SecondsAsFloat();
		g_jobSheduler->Queue(newImage->loadingJob);

		m_jacketImages.Add(path, newImage);
	}
	else
	{
		it->second->lastUsage = m_jobTimer.SecondsAsFloat();
		// If loaded set texture
		if (it->second->loaded)
		{
			ret = it->second->texture;
		}
	}
	return ret;
}

void Application::SetScriptPath(lua_State *s)
{
	//Set path for 'require' (https://stackoverflow.com/questions/4125971/setting-the-global-lua-path-variable-from-c-c?lq=1)
	String lua_path = Path::Normalize(
		Path::Absolute("./skins/" + m_skin + "/scripts/?.lua;") + Path::Absolute("./skins/" + m_skin + "/scripts/?"));

	lua_getglobal(s, "package");
	lua_getfield(s, -1, "path");				// get field "path" from table at top of stack (-1)
	std::string cur_path = lua_tostring(s, -1); // grab path string from top of stack
	cur_path.append(";");						// do your path magic here
	cur_path.append(lua_path.c_str());
	lua_pop(s, 1);						 // get rid of the string on the stack we just pushed on line 5
	lua_pushstring(s, cur_path.c_str()); // push the new one
	lua_setfield(s, -2, "path");		 // set the field "path" in table at -2 with value at top of stack
	lua_pop(s, 1);						 // get rid of package table from top of stack
}

std::set<String> g_luaErrorsSeen;

lua_State *Application::LoadScript(const String &name, bool noError)
{
	lua_State *s = luaL_newstate();
	luaL_openlibs(s);
	SetScriptPath(s);

	String path = "skins/" + m_skin + "/scripts/" + name + ".lua";
	String commonPath = "skins/" + m_skin + "/scripts/" + "common.lua";
	path = Path::Absolute(path);
	commonPath = Path::Absolute(commonPath);

	// If we can't find this file, copy it from the default skin
	if (!Path::FileExists(path))
	{
		String defaultPath = Path::Absolute("skins/Default/scripts/" + name + ".lua");
		if (Path::FileExists(defaultPath))
		{
			bool copyDefault = g_gameWindow->ShowYesNoMessage("Missing " + name + ".lua", "No " + name + ".lua file could be found, suggested solution:\n"
																										 "Would you like to copy \"scripts/" +
																							  name + ".lua\" from the default skin to your current skin?");
			if (copyDefault)
				Path::Copy(defaultPath, path);
		}
	}

	SetLuaBindings(s);
	if (luaL_dofile(s, commonPath.c_str()) || luaL_dofile(s, path.c_str()))
	{
		Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(s, -1));
		if (!noError)
			g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(s, -1), 0);
		lua_close(s);
		return nullptr;
	}
	else
		g_luaErrorsSeen.clear();
	return s;
}

// TODO add option for this
void Application::ShowLuaError(const String &error)
{
	if (g_luaErrorsSeen.find(error) != g_luaErrorsSeen.end())
		return;
	g_luaErrorsSeen.insert(error);

	Logf("Lua error: %s", Logger::Severity::Error, *error);
	g_gameWindow->ShowMessageBox("Lua Error", error, 0);
}

void Application::ReloadScript(const String &name, lua_State *L)
{
	SetScriptPath(L);
	String path = "skins/" + m_skin + "/scripts/" + name + ".lua";
	String commonPath = "skins/" + m_skin + "/scripts/" + "common.lua";
	DisposeGUI(L);
	m_skinHttp.ClearState(L);
	m_skinIR.ClearState(L);
	path = Path::Absolute(path);
	commonPath = Path::Absolute(commonPath);
	if (luaL_dofile(L, commonPath.c_str()) || luaL_dofile(L, path.c_str()))
	{
		Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(L, -1));
		g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(L, -1), 0);
		lua_close(L);
		assert(false);
	}
	else
		g_luaErrorsSeen.clear();
}

void Application::ReloadSkin()
{
	//remove all tickables
	for (auto *t : g_tickables)
	{
		t->m_Suspend();
		delete t;
	}
	g_tickables.clear();
	g_tickableChanges.clear();

	m_skin = g_gameConfig.GetString(GameConfigKeys::Skin);
	if (g_skinConfig)
	{
		delete g_skinConfig;
	}
	g_skinConfig = new SkinConfig(m_skin);
	g_guiState.fontCahce.clear();
	g_guiState.textCache.clear();
	g_guiState.nextTextId.clear();
	g_guiState.nextPaintId.clear();
	g_guiState.paintCache.clear();
	m_jacketImages.clear();

	for (auto &sample : m_samples)
	{
		sample.second->Stop();
	}
	m_samples.clear();

	if (g_transition)
	{
		delete g_transition;
		g_transition = TransitionScreen::Create();
	}

	//#ifdef EMBEDDED
	//	nvgDeleteGLES2(g_guiState.vg);
	//#else
	//	nvgDeleteGL3(g_guiState.vg);
	//#endif
	//
	//#ifdef EMBEDDED
	//#ifdef _DEBUG
	//	g_guiState.vg = nvgCreateGLES2(NVG_DEBUG);
	//#else
	//	g_guiState.vg = nvgCreateGLES2(0);
	//#endif
	//#else
	//#ifdef _DEBUG
	//	g_guiState.vg = nvgCreateGL3(NVG_DEBUG);
	//#else
	//	g_guiState.vg = nvgCreateGL3(0);
	//#endif
	//#endif

	//nvgCreateFont(g_guiState.vg, "fallback", *Path::Absolute("fonts/NotoSansCJKjp-Regular.otf"));

	//push new titlescreen
	m_gaugeRemovedWarn = true;
	TitleScreen *t = TitleScreen::Create();
	AddTickable(t);
}
void Application::DisposeLua(lua_State *state)
{
	DisposeGUI(state);
	m_skinHttp.ClearState(state);
	m_skinIR.ClearState(state);
	lua_close(state);
}

void Application::DiscordError(int errorCode, const char *message)
{
	Logf("[Discord] %s", Logger::Severity::Warning, message);
}

void Application::DiscordPresenceMenu(String name)
{
	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));
	discordPresence.state = "In Menus";
	discordPresence.details = name.c_str();

	discordPresence.joinSecret = *m_multiRoomSecret;
	discordPresence.partySize = m_multiRoomCount;
	discordPresence.partyMax = m_multiRoomSize;
	discordPresence.partyId = *m_multiRoomId;

	Discord_UpdatePresence(&discordPresence);
}

void Application::DiscordPresenceMulti(String secret, int partySize, int partyMax, String id)
{
	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));

	m_multiRoomCount = partySize;
	m_multiRoomSize = partyMax;
	m_multiRoomSecret = secret;
	m_multiRoomId = id;

	discordPresence.state = "In Lobby";
	discordPresence.details = "Waiting for multiplayer game to start.";

	discordPresence.joinSecret = *m_multiRoomSecret;
	discordPresence.partySize = m_multiRoomCount;
	discordPresence.partyMax = m_multiRoomSize;
	discordPresence.partyId = *m_multiRoomId;

	Discord_UpdatePresence(&discordPresence);
}

void Application::DiscordPresenceSong(const BeatmapSettings &song, int64 startTime, int64 endTime)
{
	Vector<String> diffNames = {"NOV", "ADV", "EXH", "INF"};
	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));
	char bufferState[128] = {0};
	sprintf(bufferState, "Playing [%s %d]", diffNames[song.difficulty].c_str(), song.level);
	discordPresence.state = bufferState;
	char bufferDetails[128] = {0};
	int titleLength = snprintf(bufferDetails, 128, "%s - %s", *song.title, *song.artist);
	if (titleLength >= 128 || titleLength < 0)
	{
		memset(bufferDetails, 0, 128);
		titleLength = snprintf(bufferDetails, 128, "%s", *song.title);
	}
	if (titleLength >= 128 || titleLength < 0)
	{
		memset(bufferDetails, 0, 128);
		strcpy(bufferDetails, "[title too long]");
	}
	discordPresence.details = bufferDetails;
	discordPresence.startTimestamp = startTime;
	discordPresence.endTimestamp = endTime;

	discordPresence.joinSecret = *m_multiRoomSecret;
	discordPresence.partySize = m_multiRoomCount;
	discordPresence.partyMax = m_multiRoomSize;
	discordPresence.partyId = *m_multiRoomId;

	Discord_UpdatePresence(&discordPresence);
}

void Application::JoinMultiFromInvite(String secret)
{
	MultiplayerScreen *mpScreen = new MultiplayerScreen();
	IApplicationTickable *title = (IApplicationTickable *)TitleScreen::Create();
	String *token = new String(*secret);
	auto tokenInput = [=](void *screen) {
		MultiplayerScreen *mpScreen = (MultiplayerScreen *)screen;
		mpScreen->JoinRoomWithToken(*token);
		delete token;
	};
	auto handle = g_transition->OnLoadingComplete.AddLambda(std::move(tokenInput));
	g_transition->RemoveOnComplete(handle);
	title->m_Suspend();

	//Remove all tickables and add back a titlescreen as a base
	AddTickable(title);
	g_transition->TransitionTo(mpScreen);
	for (IApplicationTickable *tickable : g_tickables)
	{
		RemoveTickable(tickable);
	}
}



void Application::WarnGauge()
{
	if (m_gaugeRemovedWarn)
	{
		g_gameWindow->ShowMessageBox("Gauge functions removed.",
			"gfx.DrawGauge and gfx.SetGaugeColor have been removed in favour of drawing the gauge with the other gfx functions.\n"
			"Please update your skin or contact the skin author.", 1);
		m_gaugeRemovedWarn = false;
	}
}

float Application::GetRenderFPS() const
{
	return 1.0f / g_avgRenderDelta;
}

Material Application::GetFontMaterial() const
{
	return m_fontMaterial;
}

Material Application::GetGuiTexMaterial() const
{
	return m_guiTex;
}

Transform Application::GetGUIProjection() const
{
	return ProjectionMatrix::CreateOrthographic(0.0f, (float)g_resolution.x, (float)g_resolution.y, 0.0f, 0.0f, 100.0f);
}
Transform Application::GetCurrentGUITransform() const
{
	return g_guiState.t;
}
Rect Application::GetCurrentGUIScissor() const
{
	return g_guiState.scissor;
}
void Application::StoreNamedSample(String name, Sample sample)
{
	m_samples.Add(name, sample);
}
void Application::PlayNamedSample(String name, bool loop)
{
	if (m_samples.Contains(name))
	{
		Sample sample = m_samples[name];
		if (sample)
		{
			sample->Play(loop);
		}
		else
		{
			Logf("Sample \"%s\" exists but is invalid.", Logger::Severity::Warning, *name);
		}
	}
	else
	{
		Logf("No sample named \"%s\" found.", Logger::Severity::Warning, *name);
	}
}
void Application::StopNamedSample(String name)
{
	if (m_samples.Contains(name))
	{
		Sample sample = m_samples[name];
		if (sample)
		{
			sample->Stop();
		}
		else
		{
			Logf("Sample \"%s\" exists but is invalid.", Logger::Severity::Warning, *name);
		}
	}
	else
	{
		Logf("No sample named \"%s\" found.", Logger::Severity::Warning, *name);
	}
}
int Application::IsNamedSamplePlaying(String name)
{
	if (m_samples.Contains(name))
	{
		Sample sample = m_samples[name];
		if (sample)
		{
			return sample->IsPlaying() ? 1 : 0;
		}
		else
		{
			Logf("Sample \"%s\" exists but is invalid.", Logger::Severity::Warning, *name);
			return -1;
		}
	}
	else
	{
		Logf("No sample named \"%s\" found.", Logger::Severity::Warning, *name);
		return -1;
	}
}
void Application::m_OnKeyPressed(SDL_Scancode code)
{
	// Fullscreen toggle
	if (code == SDL_SCANCODE_RETURN)
	{
		if ((g_gameWindow->GetModifierKeys() & ModifierKeys::Alt) == ModifierKeys::Alt)
		{
			g_gameConfig.Set(GameConfigKeys::Fullscreen, !g_gameWindow->IsFullscreen());
			m_UpdateWindowPosAndShape();

			return;
		}
	}

	// Pass key to application
	for (auto it = g_tickables.rbegin(); it != g_tickables.rend();)
	{
		(*it)->OnKeyPressed(code);
		break;
	}
}
void Application::m_OnKeyReleased(SDL_Scancode code)
{
	for (auto it = g_tickables.rbegin(); it != g_tickables.rend();)
	{
		(*it)->OnKeyReleased(code);
		break;
	}
}
void Application::m_OnWindowResized(const Vector2i &newSize)
{
	if (g_gameConfig.GetBool(GameConfigKeys::ForcePortrait))
	{
		Vector2i tempsize = newSize; //do this because on startup g_resolution is the reference newSize
		float aspect = 9.0 / 16.0;
		g_resolution = Vector2i(tempsize.y * aspect, tempsize.y);
		g_aspectRatio = aspect;

		m_renderStateBase.aspectRatio = g_aspectRatio;
		m_renderStateBase.viewportSize = g_resolution;
		float left = tempsize.x / 2 - g_resolution.x / 2;
		float top = 0;
		float right = left + g_resolution.x;
		float bottom = g_resolution.y;
		g_gl->SetViewport(Rect(left, top, right, bottom));
		glScissor(0, 0, g_resolution.x, g_resolution.y);

		// Set in config
		if (g_gameWindow->IsFullscreen())
		{
			g_gameConfig.Set(GameConfigKeys::FullscreenMonitorIndex, g_gameWindow->GetDisplayIndex());
		}
		else
		{
			g_gameConfig.Set(GameConfigKeys::ScreenWidth, tempsize.x);
			g_gameConfig.Set(GameConfigKeys::ScreenHeight, tempsize.y);
		}
	}
	else
	{
		g_resolution = newSize;
		g_aspectRatio = (float)g_resolution.x / (float)g_resolution.y;

		m_renderStateBase.aspectRatio = g_aspectRatio;
		m_renderStateBase.viewportSize = g_resolution;
		g_gl->SetViewport(newSize);
		glViewport(0, 0, newSize.x, newSize.y);
		glScissor(0, 0, newSize.x, newSize.y);

		// Set in config
		if (g_gameWindow->IsFullscreen())
		{
			g_gameConfig.Set(GameConfigKeys::FullscreenMonitorIndex, g_gameWindow->GetDisplayIndex());
		}
		else
		{
			g_gameConfig.Set(GameConfigKeys::ScreenWidth, newSize.x);
			g_gameConfig.Set(GameConfigKeys::ScreenHeight, newSize.y);
		}
	}
}

void Application::m_OnWindowMoved(const Vector2i& newPos)
{
	if (g_gameWindow->IsActive() && !g_gameWindow->IsFullscreen())
	{
		g_gameConfig.Set(GameConfigKeys::ScreenX, newPos.x);
		g_gameConfig.Set(GameConfigKeys::ScreenY, newPos.y);
	}
}

void Application::m_UpdateWindowPosAndShape()
{
	m_UpdateWindowPosAndShape(
		g_gameConfig.GetInt(GameConfigKeys::FullscreenMonitorIndex),
		g_gameConfig.GetBool(GameConfigKeys::Fullscreen),
		false
	);
}

void Application::m_UpdateWindowPosAndShape(int32 monitorId, bool fullscreen, bool ensureInBound)
{
	const Vector2i windowPos(g_gameConfig.GetInt(GameConfigKeys::ScreenX), g_gameConfig.GetInt(GameConfigKeys::ScreenY));
	const Vector2i windowSize(g_gameConfig.GetInt(GameConfigKeys::ScreenWidth), g_gameConfig.GetInt(GameConfigKeys::ScreenHeight));
	const Vector2i fullscreenSize(g_gameConfig.GetInt(GameConfigKeys::FullScreenWidth), g_gameConfig.GetInt(GameConfigKeys::FullScreenHeight));

	g_gameWindow->SetPosAndShape(Graphics::Window::PosAndShape {
		fullscreen, g_gameConfig.GetBool(GameConfigKeys::WindowedFullscreen),
		windowPos, windowSize, monitorId, fullscreenSize
	}, ensureInBound);
	
	if (ensureInBound && !fullscreen)
	{
		Vector2i windowPos = g_gameWindow->GetWindowPos();
		g_gameConfig.Set(GameConfigKeys::ScreenX, windowPos.x);
		g_gameConfig.Set(GameConfigKeys::ScreenY, windowPos.y);
	}
}

void Application::m_OnFocusChanged(bool focused)
{
	bool muteUnfocused = g_gameConfig.GetBool(GameConfigKeys::MuteUnfocused);
	if (focused && muteUnfocused)
	{
		g_audio->SetGlobalVolume(g_gameConfig.GetFloat(GameConfigKeys::MasterVolume));
	}
	else if (!focused && muteUnfocused)
	{
		g_audio->SetGlobalVolume(0.0f);
	}
}

int Application::FastText(String inputText, float x, float y, int size, int align, const Color &color /* = Color::White */)
{
	WString text = Utility::ConvertToWString(inputText);
	String fontpath = Path::Normalize(Path::Absolute("fonts/settings/NotoSans-Regular.ttf"));
	Text te = g_application->LoadFont(fontpath, true)->CreateText(text, size);
	Transform textTransform;
	textTransform *= Transform::Translation(Vector2(x, y));

	//vertical alignment
	if ((align & (int)NVGalign::NVG_ALIGN_BOTTOM) != 0)
	{
		textTransform *= Transform::Translation(Vector2(0, -te->size.y));
	}
	else if ((align & (int)NVGalign::NVG_ALIGN_MIDDLE) != 0)
	{
		textTransform *= Transform::Translation(Vector2(0, -te->size.y / 2));
	}

	//horizontal alignment
	if ((align & (int)NVGalign::NVG_ALIGN_CENTER) != 0)
	{
		textTransform *= Transform::Translation(Vector2(-te->size.x / 2, 0));
	}
	else if ((align & (int)NVGalign::NVG_ALIGN_RIGHT) != 0)
	{
		textTransform *= Transform::Translation(Vector2(-te->size.x, 0));
	}

	MaterialParameterSet params;
	params.SetParameter("color", color);
	g_application->GetRenderQueueBase()->Draw(textTransform, te, g_application->GetFontMaterial(), params);
	return 0;
}

static int lGetMousePos(lua_State *L)
{
	Vector2i pos = g_gameWindow->GetMousePos();
	float left = g_gameWindow->GetWindowSize().x / 2 - g_resolution.x / 2;
	lua_pushnumber(L, pos.x - left);
	lua_pushnumber(L, pos.y);
	return 2;
}

static int lGetResolution(lua_State *L)
{
	lua_pushnumber(L, g_resolution.x);
	lua_pushnumber(L, g_resolution.y);
	return 2;
}

static int lGetLaserColor(lua_State *L /*int laser*/)
{
	int laser = luaL_checkinteger(L, 1);
	float laserHues[2] = {0.f};
	laserHues[0] = g_gameConfig.GetFloat(GameConfigKeys::Laser0Color);
	laserHues[1] = g_gameConfig.GetFloat(GameConfigKeys::Laser1Color);
	Colori c = Color::FromHSV(laserHues[laser], 1.0, 1.0).ToRGBA8();
	lua_pushnumber(L, c.x);
	lua_pushnumber(L, c.y);
	lua_pushnumber(L, c.z);
	return 3;
}

static int lLog(lua_State *L)
{
	String msg = luaL_checkstring(L, 1);
	int severity = luaL_checkinteger(L, 2);
	Log(msg, (Logger::Severity)severity);
	return 0;
}

static int lGetButton(lua_State *L /* int button */)
{
    int button = luaL_checkinteger(L, 1);
    if (g_application->autoplayInfo
        && (g_application->autoplayInfo->IsAutoplayButtons()) && button < 6)
        lua_pushboolean(L, g_application->autoplayInfo->buttonAnimationTimer[button] > 0);
    else
        lua_pushboolean(L, g_input.GetButton((Input::Button)button));
    return 1;
}

static int lGetKnob(lua_State *L /* int knob */)
{
	int knob = luaL_checkinteger(L, 1);
	lua_pushnumber(L, g_input.GetAbsoluteLaser(knob));
	return 1;
}

static int lGetUpdateAvailable(lua_State *L)
{
	Vector<String> info = g_application->GetUpdateAvailable();
	if (info.empty())
	{
		return 0;
	}

	lua_pushstring(L, *info[0]);
	lua_pushstring(L, *info[1]);
	return 2;
}

static int lCreateSkinImage(lua_State *L /*const char* filename, int imageflags */)
{
	const char *filename = luaL_checkstring(L, 1);
	int imageflags = luaL_checkinteger(L, 2);
	String path = "skins/" + g_application->GetCurrentSkin() + "/textures/" + filename;
	path = Path::Absolute(path);
	int handle = nvgCreateImage(g_guiState.vg, path.c_str(), imageflags);
	if (handle != 0)
	{
		g_guiState.vgImages[L].Add(handle);
		lua_pushnumber(L, handle);
		return 1;
	}
	return 0;
}

static int lLoadSkinAnimation(lua_State *L)
{
	const char *p;
	float frametime;
	int loopcount = 0;
	bool compressed = false;

	p = luaL_checkstring(L, 1);
	frametime = luaL_checknumber(L, 2);
	if (lua_gettop(L) == 3)
	{
		loopcount = luaL_checkinteger(L, 3);
	}
	else if (lua_gettop(L) == 4)
	{
		loopcount = luaL_checkinteger(L, 3);
		compressed = lua_toboolean(L, 4) == 1;
	}

	String path = "skins/" + g_application->GetCurrentSkin() + "/textures/" + p;
	path = Path::Absolute(path);
	int result = LoadAnimation(L, *path, frametime, loopcount, compressed);
	if (result == -1)
		return 0;

	lua_pushnumber(L, result);
	return 1;
}

static int lLoadSkinFont(lua_State *L /*const char* name */)
{
	const char *name = luaL_checkstring(L, 1);
	String path = "skins/" + g_application->GetCurrentSkin() + "/fonts/" + name;
	path = Path::Absolute(path);
	return LoadFont(name, path.c_str(), L);
}

static int lLoadSkinSample(lua_State *L /*char* name */)
{
	const char *name = luaL_checkstring(L, 1);
	Sample newSample = g_application->LoadSample(name);
	if (!newSample)
	{
		lua_Debug ar;
		lua_getstack(L, 1, &ar);
		lua_getinfo(L, "Snl", &ar);
		String luaFilename;
		Path::RemoveLast(ar.source, &luaFilename);
		lua_pushstring(L, *Utility::Sprintf("Failed to load sample \"%s\" at line %d in \"%s\"", name, ar.currentline, luaFilename));
		return lua_error(L);
	}
	g_application->StoreNamedSample(name, newSample);
	return 0;
}

static int lPlaySample(lua_State *L /*char* name, bool loop */)
{
	const char *name = luaL_checkstring(L, 1);
	bool loop = false;
	if (lua_gettop(L) == 2)
	{
		loop = lua_toboolean(L, 2) == 1;
	}

	g_application->PlayNamedSample(name, loop);
	return 0;
}

static int lIsSamplePlaying(lua_State *L /* char* name */)
{
	const char *name = luaL_checkstring(L, 1);
	int res = g_application->IsNamedSamplePlaying(name);
	if (res == -1)
		return 0;

	lua_pushboolean(L, res);
	return 1;
}

static int lStopSample(lua_State *L /* char* name */)
{
	const char *name = luaL_checkstring(L, 1);
	g_application->StopNamedSample(name);
	return 0;
}

static int lPathAbsolute(lua_State *L /* string path */)
{
	const char *path = luaL_checkstring(L, 1);
	lua_pushstring(L, *Path::Absolute(path));
	return 1;
}

static int lForceRender(lua_State *L)
{
	g_application->ForceRender();
	return 0;
}

static int lLoadImageJob(lua_State *L /* char* path, int placeholder, int w = 0, int h = 0 */)
{
	const char *path = luaL_checkstring(L, 1);
	int fallback = luaL_checkinteger(L, 2);
	int w = 0, h = 0;
	if (lua_gettop(L) == 4)
	{
		w = luaL_checkinteger(L, 3);
		h = luaL_checkinteger(L, 4);
	}
	lua_pushinteger(L, g_application->LoadImageJob(path, {w, h}, fallback));
	return 1;
}

static int lLoadWebImageJob(lua_State *L /* char* url, int placeholder, int w = 0, int h = 0 */)
{
	const char *url = luaL_checkstring(L, 1);
	int fallback = luaL_checkinteger(L, 2);
	int w = 0, h = 0;
	if (lua_gettop(L) == 4)
	{
		w = luaL_checkinteger(L, 3);
		h = luaL_checkinteger(L, 4);
	}
	lua_pushinteger(L, g_application->LoadImageJob(url, {w, h}, fallback, true));
	return 1;
}

static int lWarnGauge(lua_State *L)
{
	g_application->WarnGauge();
	return 0;
}

static int lGetSkin(lua_State *L)
{
	lua_pushstring(L, *g_application->GetCurrentSkin());
	return 1;
}

static int lSetSkinSetting(lua_State *L /*String key, Any value*/)
{
	String key = luaL_checkstring(L, 1);
	IConfigEntry *entry = g_skinConfig->GetEntry(key);
	if (!entry) //just set depending on value type
	{
		if (lua_isboolean(L, 2))
		{
			bool value = luaL_checknumber(L, 2) == 1;
			g_skinConfig->Set(key, value);
		}
		else if (lua_isnumber(L, 2)) //no good way to know if int or not
		{
			float value = luaL_checknumber(L, 2);
			g_skinConfig->Set(key, value);
		}
		else if (lua_isstring(L, 2))
		{
			String value = luaL_checkstring(L, 2);
			g_skinConfig->Set(key, value);
		}
	}
	else
	{
		if (entry->GetType() == IConfigEntry::EntryType::Boolean)
		{
			bool value = luaL_checknumber(L, 2) == 1;
			g_skinConfig->Set(key, value);
		}
		else if (entry->GetType() == IConfigEntry::EntryType::Float)
		{
			float value = luaL_checknumber(L, 2);
			g_skinConfig->Set(key, value);
		}
		else if (entry->GetType() == IConfigEntry::EntryType::Integer)
		{
			int value = luaL_checkinteger(L, 2);
			g_skinConfig->Set(key, value);
		}
		else if (entry->GetType() == IConfigEntry::EntryType::String)
		{
			String value = luaL_checkstring(L, 2);
			g_skinConfig->Set(key, value);
		}
	}
	return 0;
}

static int lGetSkinSetting(lua_State *L /*String key*/)
{
	String key = luaL_checkstring(L, 1);
	IConfigEntry *entry = g_skinConfig->GetEntry(key);
	if (!entry)
	{
		return 0;
	}

	if (entry->GetType() == IConfigEntry::EntryType::Boolean)
	{
		lua_pushboolean(L, entry->As<BoolConfigEntry>()->data);
		return 1;
	}
	else if (entry->GetType() == IConfigEntry::EntryType::Float)
	{
		lua_pushnumber(L, entry->As<FloatConfigEntry>()->data);
		return 1;
	}
	else if (entry->GetType() == IConfigEntry::EntryType::Integer)
	{
		lua_pushnumber(L, entry->As<IntConfigEntry>()->data);
		return 1;
	}
	else if (entry->GetType() == IConfigEntry::EntryType::String)
	{
		lua_pushstring(L, entry->As<StringConfigEntry>()->data.c_str());
		return 1;
	}
	else if (entry->GetType() == IConfigEntry::EntryType::Color)
	{
		Colori data = entry->As<ColorConfigEntry>()->data.ToRGBA8();
		lua_pushnumber(L, data.x);
		lua_pushnumber(L, data.y);
		lua_pushnumber(L, data.z);
		lua_pushnumber(L, data.w);
		return 4;
	}
	else
	{
		return 0;
	}
}

void Application::SetLuaBindings(lua_State *state)
{
	auto pushFuncToTable = [&](const char *name, int (*func)(lua_State *)) {
		lua_pushstring(state, name);
		lua_pushcfunction(state, func);
		lua_settable(state, -3);
	};

	auto pushIntToTable = [&](const char *name, int data) {
		lua_pushstring(state, name);
		lua_pushinteger(state, data);
		lua_settable(state, -3);
	};

	//gfx
	{
		lua_newtable(state);
		pushFuncToTable("BeginPath", lBeginPath);
		pushFuncToTable("Rect", lRect);
		pushFuncToTable("FastRect", lFastRect);
		pushFuncToTable("Fill", lFill);
		pushFuncToTable("FillColor", lFillColor);
		pushFuncToTable("CreateImage", lCreateImage);
		pushFuncToTable("CreateSkinImage", lCreateSkinImage);
		pushFuncToTable("ImagePatternFill", lImagePatternFill);
		pushFuncToTable("ImageRect", lImageRect);
		pushFuncToTable("Text", lText);
		pushFuncToTable("TextAlign", lTextAlign);
		pushFuncToTable("FontFace", lFontFace);
		pushFuncToTable("FontSize", lFontSize);
		pushFuncToTable("Translate", lTranslate);
		pushFuncToTable("Scale", lScale);
		pushFuncToTable("Rotate", lRotate);
		pushFuncToTable("ResetTransform", lResetTransform);
		pushFuncToTable("LoadFont", lLoadFont);
		pushFuncToTable("LoadSkinFont", lLoadSkinFont);
		pushFuncToTable("FastText", lFastText);
		pushFuncToTable("CreateLabel", lCreateLabel);
		pushFuncToTable("DrawLabel", lDrawLabel);
		pushFuncToTable("MoveTo", lMoveTo);
		pushFuncToTable("LineTo", lLineTo);
		pushFuncToTable("BezierTo", lBezierTo);
		pushFuncToTable("QuadTo", lQuadTo);
		pushFuncToTable("ArcTo", lArcTo);
		pushFuncToTable("ClosePath", lClosePath);
		pushFuncToTable("MiterLimit", lMiterLimit);
		pushFuncToTable("StrokeWidth", lStrokeWidth);
		pushFuncToTable("LineCap", lLineCap);
		pushFuncToTable("LineJoin", lLineJoin);
		pushFuncToTable("Stroke", lStroke);
		pushFuncToTable("StrokeColor", lStrokeColor);
		pushFuncToTable("UpdateLabel", lUpdateLabel);
		pushFuncToTable("DrawGauge", lWarnGauge);
		pushFuncToTable("SetGaugeColor", lWarnGauge);
		pushFuncToTable("RoundedRect", lRoundedRect);
		pushFuncToTable("RoundedRectVarying", lRoundedRectVarying);
		pushFuncToTable("Ellipse", lEllipse);
		pushFuncToTable("Circle", lCircle);
		pushFuncToTable("SkewX", lSkewX);
		pushFuncToTable("SkewY", lSkewY);
		pushFuncToTable("LinearGradient", lLinearGradient);
		pushFuncToTable("BoxGradient", lBoxGradient);
		pushFuncToTable("RadialGradient", lRadialGradient);
		pushFuncToTable("ImagePattern", lImagePattern);
		pushFuncToTable("UpdateImagePattern", lUpdateImagePattern);
		pushFuncToTable("GradientColors", lGradientColors);
		pushFuncToTable("FillPaint", lFillPaint);
		pushFuncToTable("StrokePaint", lStrokePaint);
		pushFuncToTable("Save", lSave);
		pushFuncToTable("Restore", lRestore);
		pushFuncToTable("Reset", lReset);
		pushFuncToTable("PathWinding", lPathWinding);
		pushFuncToTable("ForceRender", lForceRender);
		pushFuncToTable("LoadImageJob", lLoadImageJob);
		pushFuncToTable("LoadWebImageJob", lLoadWebImageJob);
		pushFuncToTable("Scissor", lScissor);
		pushFuncToTable("IntersectScissor", lIntersectScissor);
		pushFuncToTable("ResetScissor", lResetScissor);
		pushFuncToTable("TextBounds", lTextBounds);
		pushFuncToTable("LabelSize", lLabelSize);
		pushFuncToTable("FastTextSize", lFastTextSize);
		pushFuncToTable("ImageSize", lImageSize);
		pushFuncToTable("Arc", lArc);
		pushFuncToTable("SetImageTint", lSetImageTint);
		pushFuncToTable("LoadAnimation", lLoadAnimation);
		pushFuncToTable("LoadSkinAnimation", lLoadSkinAnimation);
		pushFuncToTable("TickAnimation", lTickAnimation);
		pushFuncToTable("ResetAnimation", lResetAnimation);
		pushFuncToTable("GlobalCompositeOperation", lGlobalCompositeOperation);
		pushFuncToTable("GlobalCompositeBlendFunc", lGlobalCompositeBlendFunc);
		pushFuncToTable("GlobalCompositeBlendFuncSeparate", lGlobalCompositeBlendFuncSeparate);
		pushFuncToTable("GlobalAlpha", lGlobalAlpha);
		pushFuncToTable("CreateShadedMesh", ShadedMesh::lNew);
		//constants
		//Text align
		pushIntToTable("TEXT_ALIGN_BASELINE", NVGalign::NVG_ALIGN_BASELINE);
		pushIntToTable("TEXT_ALIGN_BOTTOM", NVGalign::NVG_ALIGN_BOTTOM);
		pushIntToTable("TEXT_ALIGN_CENTER", NVGalign::NVG_ALIGN_CENTER);
		pushIntToTable("TEXT_ALIGN_LEFT", NVGalign::NVG_ALIGN_LEFT);
		pushIntToTable("TEXT_ALIGN_MIDDLE", NVGalign::NVG_ALIGN_MIDDLE);
		pushIntToTable("TEXT_ALIGN_RIGHT", NVGalign::NVG_ALIGN_RIGHT);
		pushIntToTable("TEXT_ALIGN_TOP", NVGalign::NVG_ALIGN_TOP);
		//Line caps and joins
		pushIntToTable("LINE_BEVEL", NVGlineCap::NVG_BEVEL);
		pushIntToTable("LINE_BUTT", NVGlineCap::NVG_BUTT);
		pushIntToTable("LINE_MITER", NVGlineCap::NVG_MITER);
		pushIntToTable("LINE_ROUND", NVGlineCap::NVG_ROUND);
		pushIntToTable("LINE_SQUARE", NVGlineCap::NVG_SQUARE);
		//Image flags
		pushIntToTable("IMAGE_GENERATE_MIPMAPS", NVGimageFlags::NVG_IMAGE_GENERATE_MIPMAPS);
		pushIntToTable("IMAGE_REPEATX", NVGimageFlags::NVG_IMAGE_REPEATX);
		pushIntToTable("IMAGE_REPEATY", NVGimageFlags::NVG_IMAGE_REPEATY);
		pushIntToTable("IMAGE_FLIPY", NVGimageFlags::NVG_IMAGE_FLIPY);
		pushIntToTable("IMAGE_PREMULTIPLIED", NVGimageFlags::NVG_IMAGE_PREMULTIPLIED);
		pushIntToTable("IMAGE_NEAREST", NVGimageFlags::NVG_IMAGE_NEAREST);
		//Blend flags
		pushIntToTable("BLEND_ZERO,", NVGblendFactor::NVG_ZERO);
		pushIntToTable("BLEND_ONE,", NVGblendFactor::NVG_ONE);
		pushIntToTable("BLEND_SRC_COLOR", NVGblendFactor::NVG_SRC_COLOR);
		pushIntToTable("BLEND_ONE_MINUS_SRC_COLOR", NVGblendFactor::NVG_ONE_MINUS_SRC_COLOR);
		pushIntToTable("BLEND_DST_COLOR", NVGblendFactor::NVG_DST_COLOR);
		pushIntToTable("BLEND_ONE_MINUS_DST_COLOR", NVGblendFactor::NVG_ONE_MINUS_DST_COLOR);
		pushIntToTable("BLEND_SRC_ALPHA", NVGblendFactor::NVG_SRC_ALPHA);
		pushIntToTable("BLEND_ONE_MINUS_SRC_ALPHA", NVGblendFactor::NVG_ONE_MINUS_SRC_ALPHA);
		pushIntToTable("BLEND_DST_ALPHA", NVGblendFactor::NVG_DST_ALPHA);
		pushIntToTable("BLEND_ONE_MINUS_DST_ALPHA", NVGblendFactor::NVG_ONE_MINUS_DST_ALPHA);
		pushIntToTable("BLEND_SRC_ALPHA_SATURATE", NVGblendFactor::NVG_SRC_ALPHA_SATURATE);
		//Blend operations
		pushIntToTable("BLEND_OP_SOURCE_OVER", NVGcompositeOperation::NVG_SOURCE_OVER); //<<<<< default
		pushIntToTable("BLEND_OP_SOURCE_IN", NVGcompositeOperation::NVG_SOURCE_IN);
		pushIntToTable("BLEND_OP_SOURCE_OUT", NVGcompositeOperation::NVG_SOURCE_OUT);
		pushIntToTable("BLEND_OP_ATOP", NVGcompositeOperation::NVG_ATOP);
		pushIntToTable("BLEND_OP_DESTINATION_OVER", NVGcompositeOperation::NVG_DESTINATION_OVER);
		pushIntToTable("BLEND_OP_DESTINATION_IN", NVGcompositeOperation::NVG_DESTINATION_IN);
		pushIntToTable("BLEND_OP_DESTINATION_OUT", NVGcompositeOperation::NVG_DESTINATION_OUT);
		pushIntToTable("BLEND_OP_DESTINATION_ATOP", NVGcompositeOperation::NVG_DESTINATION_ATOP);
		pushIntToTable("BLEND_OP_LIGHTER", NVGcompositeOperation::NVG_LIGHTER);
		pushIntToTable("BLEND_OP_COPY", NVGcompositeOperation::NVG_COPY);
		pushIntToTable("BLEND_OP_XOR", NVGcompositeOperation::NVG_XOR);

		lua_setglobal(state, "gfx");
	}

	//game
	{
		lua_newtable(state);
		pushFuncToTable("GetMousePos", lGetMousePos);
		pushFuncToTable("GetResolution", lGetResolution);
		pushFuncToTable("Log", lLog);
		pushFuncToTable("LoadSkinSample", lLoadSkinSample);
		pushFuncToTable("PlaySample", lPlaySample);
		pushFuncToTable("StopSample", lStopSample);
		pushFuncToTable("IsSamplePlaying", lIsSamplePlaying);
		pushFuncToTable("GetLaserColor", lGetLaserColor);
		pushFuncToTable("GetButton", lGetButton);
		pushFuncToTable("GetKnob", lGetKnob);
		pushFuncToTable("UpdateAvailable", lGetUpdateAvailable);
		pushFuncToTable("GetSkin", lGetSkin);
		pushFuncToTable("GetSkin", lGetSkin);
		pushFuncToTable("GetSkinSetting", lGetSkinSetting);
		pushFuncToTable("SetSkinSetting", lSetSkinSetting);

		//constants
		pushIntToTable("LOGGER_INFO", (int)Logger::Severity::Info);
		pushIntToTable("LOGGER_NORMAL", (int)Logger::Severity::Normal);
		pushIntToTable("LOGGER_WARNING", (int)Logger::Severity::Warning);
		pushIntToTable("LOGGER_ERROR", (int)Logger::Severity::Error);
		pushIntToTable("BUTTON_BTA", (int)Input::Button::BT_0);
		pushIntToTable("BUTTON_BTB", (int)Input::Button::BT_1);
		pushIntToTable("BUTTON_BTC", (int)Input::Button::BT_2);
		pushIntToTable("BUTTON_BTD", (int)Input::Button::BT_3);
		pushIntToTable("BUTTON_FXL", (int)Input::Button::FX_0);
		pushIntToTable("BUTTON_FXR", (int)Input::Button::FX_1);
		pushIntToTable("BUTTON_STA", (int)Input::Button::BT_S);
		pushIntToTable("BUTTON_BCK", (int)Input::Button::Back);

		lua_setglobal(state, "game");
	}

	//path
	{
		lua_newtable(state);
		pushFuncToTable("Absolute", lPathAbsolute);
		lua_setglobal(state, "path");
	}

	//ir
	{
		lua_newtable(state);

		lua_pushstring(state, "States");
		lua_newtable(state);

		for(const auto& el : IR::ResponseState::Values)
			pushIntToTable(el.first, el.second);

		lua_settable(state, -3);

		lua_pushstring(state, "Active");
		lua_pushboolean(state, g_gameConfig.GetString(GameConfigKeys::IRBaseURL) != "");
		lua_settable(state, -3);

		lua_setglobal(state, "IRData");

		m_skinIR.PushFunctions(state);
	}

	//http
	m_skinHttp.PushFunctions(state);
}

bool JacketLoadingJob::Run()
{
	// Create loading task
	if (web)
	{
		auto response = cpr::Get(imagePath);
		if (response.error.code != cpr::ErrorCode::OK || response.status_code >= 300)
		{
			return false;
		}
		Buffer b;
		b.resize(response.text.length());
		memcpy(b.data(), response.text.c_str(), b.size());
		loadedImage = ImageRes::Create(b);
		if (loadedImage)
		{
			if (loadedImage->GetSize().x > w || loadedImage->GetSize().y > h)
			{
				loadedImage->ReSize({w, h});
			}
		}
		return loadedImage.get() != nullptr;
	}
	else
	{
		loadedImage = ImageRes::Create(imagePath);
		if (loadedImage)
		{
			if (loadedImage->GetSize().x > w || loadedImage->GetSize().y > h)
			{
				loadedImage->ReSize({w, h});
			}
		}
		return loadedImage.get() != nullptr;
	}
}
void JacketLoadingJob::Finalize()
{
	if (IsSuccessfull())
	{
		///TODO: Maybe do the nvgCreateImage in Run() instead
		target->texture = nvgCreateImageRGBA(g_guiState.vg, loadedImage->GetSize().x, loadedImage->GetSize().y, 0, (unsigned char *)loadedImage->GetBits());
		target->loaded = true;
	}
}
