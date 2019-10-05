#include "stdafx.h"
#include "Application.hpp"
#include <Beatmap/Beatmap.hpp>
#include "Game.hpp"
#include "Test.hpp"
#include "SongSelect.hpp"
#include "TitleScreen.hpp"
#include <Audio/Audio.hpp>
#include <Graphics/Window.hpp>
#include <Graphics/ResourceManagers.hpp>
#include "Shared/Jobs.hpp"
#include <Shared/Profiling.hpp>
#include "Scoring.hpp"
#include "GameConfig.hpp"
#include "Input.hpp"
#include "TransitionScreen.hpp"
#include "GUI/HealthGauge.hpp"
#include "lua.hpp"
#include "nanovg.h"
#include "discord_rpc.h"
#include "cpr/cpr.h"
#include "json.hpp"
#include "SkinConfig.hpp"
#include "SkinHttp.hpp"
#include "SDL2/SDL_keycode.h"
#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg_gl.h"
#include "GUI/nanovg_lua.h"
#ifdef _WIN32
#include <Windows.h>
#endif

GameConfig g_gameConfig;
SkinConfig* g_skinConfig;
OpenGL* g_gl = nullptr;
Graphics::Window* g_gameWindow = nullptr;
Application* g_application = nullptr;
JobSheduler* g_jobSheduler = nullptr;
Input g_input;

// Tickable queue
static Vector<IApplicationTickable*> g_tickables;

struct TickableChange
{
	enum Mode
	{
		Added,
		Removed,
	};
	Mode mode;
	IApplicationTickable* tickable;
	IApplicationTickable* insertBefore;
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
void Application::SetCommandLine(int32 argc, char** argv)
{
	m_commandLine.clear();
	
	// Split up command line parameters
	for(int32 i = 0 ; i < argc; i++)
	{
		m_commandLine.Add(argv[i]);
	}
}
void Application::SetCommandLine(const char* cmdLine)
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
		m_skin = newskin;
		ReloadSkin();
	}
	g_gameWindow->SetVSync(g_gameConfig.GetBool(GameConfigKeys::VSync) ? 1 : 0);
	m_showFps = g_gameConfig.GetBool(GameConfigKeys::ShowFps);
	m_OnWindowResized(g_gameWindow->GetWindowSize());

}
int32 Application::Run()
{
	if(!m_Init())
		return 1;

	if(m_commandLine.Contains("-test")) 
	{
		// Create test scene
		AddTickable(Test::Create());
	}
	else
	{
		bool mapLaunched = false;
		// Play the map specified in the command line
		if(m_commandLine.size() > 1 && m_commandLine[1].front() != '-')
		{
			Game* game = LaunchMap(m_commandLine[1]);
			if(!game)
			{
				Logf("LaunchMap(%s) failed", Logger::Error, m_commandLine[1]);
			}
			else
			{
				auto& cmdLine = g_application->GetAppCommandLine();
				if(cmdLine.Contains("-autoplay") || cmdLine.Contains("-auto"))
				{
					game->GetScoring().autoplay = true;
				}
				mapLaunched = true;
			}
		}

		if(!mapLaunched)
		{
			if(m_commandLine.Contains("-notitle"))
				AddTickable(SongSelect::Create());
			else // Start regular game, goto title screen
				AddTickable(TitleScreen::Create());
		}
	}

	m_MainLoop();

	return 0;
}

void Application::SetUpdateAvailable(const String& version, const String& url, const String& download)
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

NVGcontext * Application::GetVGContext()
{
	return g_guiState.vg;
}

Vector<String> Application::GetUpdateAvailable()
{
	if (m_hasUpdate) {
		return Vector<String> {m_updateUrl, m_updateVersion};
	}

	return Vector<String>();	
}

bool Application::m_LoadConfig()
{
	File configFile;
	if(configFile.OpenRead(Path::Absolute("Main.cfg")))
	{
		FileReader reader(configFile);
		if(g_gameConfig.Load(reader))
			return true;
	}
	return false;
}
void Application::m_SaveConfig()
{
	if(!g_gameConfig.IsDirty())
		return;

	File configFile;
	if(configFile.OpenWrite(Path::Absolute("Main.cfg")))
	{
		FileWriter writer(configFile);
		g_gameConfig.Save(writer);
	}
}

void __discordError(int errorCode, const char* message)
{
	g_application->DiscordError(errorCode, message);
}

void __discordReady(const DiscordUser* user)
{
	Logf("[Discord] Logged in as \"%s\"", Logger::Info, user->username);
}

void __discordJoinGame(const char * joins)
{
	g_application->JoinMultiFromInvite(joins);
}

void __discordSpecGame(const char * specs)
{
}

void __discordJoinReq(const DiscordUser * duser)
{
}

void __discordDisconnected(int errcode, const char * msg)
{
	g_application->DiscordError(errcode, msg);
}

void __updateChecker()
{

	ProfilerScope $1("Check for updates");
	if (g_gameConfig.GetBool(GameConfigKeys::OnlyRelease))
	{
		auto r = cpr::Get(cpr::Url{ "https://api.github.com/repos/drewol/unnamed-sdvx-clone/releases/latest" });

		Logf("Update check status code: %d", Logger::Normal, r.status_code);
		if (r.error.code != cpr::ErrorCode::OK)
		{
			Logf("Failed to get update information: %s", Logger::Error, r.error.message.c_str());
		}
		else
		{
			nlohmann::json latestInfo;
			///TODO: Don't use exceptions
			try
			{
				latestInfo = nlohmann::json::parse(r.text);
			}
			catch (const std::exception& e)
			{
				Logf("Failed to parse version json: \"%s\"", Logger::Error, e.what());
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
		auto response = cpr::Get(cpr::Url{ "https://api.github.com/repos/drewol/unnamed-sdvx-clone/commits" });
		if (response.status_code != 200)
		{
			Logf("Failed to get update information: %s", Logger::Error, response.error.message.c_str());
		}

		auto commits = nlohmann::json::parse(response.text);
		String current_hash;
		String(GIT_COMMIT).Split("_", nullptr, &current_hash);
		
		if (commits.contains("message"))
		{
			//some error message was sent
			return;
		}

		int commit = 0;
		while (commit < 30)
		{
			String new_hash = commits[commit]["sha"];
			if (current_hash == new_hash.substr(0, current_hash.length()))
			{
				return;
			}
			printf("Getting build status for commit \"%s\"", new_hash.c_str());
			auto response = cpr::Get(cpr::Url{ "https://api.github.com/repos/drewol/unnamed-sdvx-clone/commits/" + new_hash + "/status" });
			if (response.status_code != 200)
			{
				Logf("Failed to get update information: %s", Logger::Error, response.error.message.c_str());
			}
			auto commit_status = nlohmann::json::parse(response.text);
			String state = commit_status["state"];
			if (state == "success")
			{
				String updateUrl;
				commits[commit].at("html_url").get_to(updateUrl);
				g_application->SetUpdateAvailable(new_hash.substr(0,7), updateUrl, "http://drewol.me/Downloads/Game.zip");
				return;
			}
			commit++;
		}
#endif
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
	Logf("Version: %d.%d.%d", Logger::Info, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

#ifdef GIT_COMMIT
	Logf("Git commit: %s", Logger::Info, GIT_COMMIT);
#endif // GIT_COMMIT




	// Must have command line
	assert(m_commandLine.size() >= 1);

	if(!m_LoadConfig())
	{
		Log("Failed to load config file", Logger::Warning);
	}

	// Job sheduler
	g_jobSheduler = new JobSheduler();

	m_allowMapConversion = false;
	bool debugMute = false;
	bool startFullscreen = false;
	uint32 fullscreenMonitor = -1;

	// Fullscreen settings from config
	if(g_gameConfig.GetBool(GameConfigKeys::Fullscreen))
		startFullscreen = true;
	fullscreenMonitor = g_gameConfig.GetInt(GameConfigKeys::FullscreenMonitorIndex);

	for(auto& cl : m_commandLine)
	{
		String k, v;
		if(cl.Split("=", &k, &v))
		{
			if(k == "-monitor")
			{
				fullscreenMonitor = atol(*v);
			}
		}
		else
		{
			if(cl == "-convertmaps")
			{
				m_allowMapConversion = true;
			}
			else if(cl == "-mute")
			{
				debugMute = true;
			}
			else if(cl == "-fullscreen")
			{
				startFullscreen = true;
			}
		}
	}

	// Create the game window
	g_resolution = Vector2i(
		g_gameConfig.GetInt(GameConfigKeys::ScreenWidth),
		g_gameConfig.GetInt(GameConfigKeys::ScreenHeight));
	g_aspectRatio = (float)g_resolution.x / (float)g_resolution.y;
	int samplecount = g_gameConfig.GetInt(GameConfigKeys::AntiAliasing);
	if (samplecount > 0)
		samplecount = 1 << samplecount;
	g_gameWindow = new Graphics::Window(g_resolution, samplecount);
	g_gameWindow->Show();

	g_gameWindow->OnKeyPressed.Add(this, &Application::m_OnKeyPressed);
	g_gameWindow->OnKeyReleased.Add(this, &Application::m_OnKeyReleased);
	g_gameWindow->OnResized.Add(this, &Application::m_OnWindowResized);

	// Initialize Input
	g_input.Init(*g_gameWindow);

	// Set skin variable
	m_skin = g_gameConfig.GetString(GameConfigKeys::Skin);

	// Fallback to default if not found
	if (!Path::FileExists(Path::Absolute("skins/" + m_skin))) {
		m_skin = "Default";
	}

	g_skinConfig = new SkinConfig(m_skin);
	// Window cursor
	Image cursorImg = ImageRes::Create(Path::Absolute("skins/" + m_skin + "/textures/cursor.png"));
	g_gameWindow->SetCursor(cursorImg, Vector2i(5, 5));

	if(startFullscreen)
		g_gameWindow->SwitchFullscreen(
			g_gameConfig.GetInt(GameConfigKeys::ScreenWidth), g_gameConfig.GetInt(GameConfigKeys::ScreenHeight),
			g_gameConfig.GetInt(GameConfigKeys::FullScreenWidth), g_gameConfig.GetInt(GameConfigKeys::FullScreenHeight),
			fullscreenMonitor, g_gameConfig.GetBool(GameConfigKeys::WindowedFullscreen)
		);

	// Set render state variables
	m_renderStateBase.aspectRatio = g_aspectRatio;
	m_renderStateBase.viewportSize = g_resolution;
	m_renderStateBase.time = 0.0f;

	{
		ProfilerScope $1("Audio Init");

		// Init audio
		new Audio();
		bool exclusive = g_gameConfig.GetBool(GameConfigKeys::WASAPI_Exclusive);
		if(!g_audio->Init(exclusive))
		{
			if (exclusive)
			{
				Log("Failed to open in WASAPI Exclusive mode, attempting shared mode.", Logger::Warning);
				g_gameWindow->ShowMessageBox("WASAPI Exclusive mode error.", "Failed to open in WASAPI Exclusive mode, attempting shared mode.", 1);
				if (!g_audio->Init(false))
				{
					Log("Audio initialization failed", Logger::Error);
					delete g_audio;
					return false;
				}
			}
			else
			{
				Log("Audio initialization failed", Logger::Error);
				delete g_audio;
				return false;
			}
		}

		// Debug Mute?
		// Test tracks may get annoying when continously debugging ;)
		if(debugMute)
		{
			g_audio->SetGlobalVolume(0.0f);
		}
	}

	{
		ProfilerScope $1("GL Init");

		// Create graphics context
		g_gl = new OpenGL();
		if(!g_gl->Init(*g_gameWindow, g_gameConfig.GetInt(GameConfigKeys::AntiAliasing)))
		{
			Log("Failed to create OpenGL context", Logger::Error);
			return false;
		}

#ifdef _DEBUG
		g_guiState.vg = nvgCreateGL3(NVG_DEBUG);
#else
		g_guiState.vg = nvgCreateGL3(0);
#endif
		nvgCreateFont(g_guiState.vg, "fallback", *Path::Absolute("fonts/NotoSansCJKjp-Regular.otf"));
	}

	if(g_gameConfig.GetBool(GameConfigKeys::CheckForUpdates))
	{
		m_updateThread = Thread(__updateChecker);
	}

	m_InitDiscord();

	CheckedLoad(m_fontMaterial = LoadMaterial("font"));
	m_fontMaterial->opaque = false;	
	CheckedLoad(m_fillMaterial = LoadMaterial("guiColor"));
	m_fillMaterial->opaque = false;
	CheckedLoad(m_guiTex = LoadMaterial("guiTex"));
	m_guiTex->opaque = false;
	m_gauge = new HealthGauge();
	LoadGauge(false);

	//m_skinHtpp = new SkinHttp();
	// call the initial OnWindowResized now that we have intialized OpenGL
	m_OnWindowResized(g_resolution);

	m_showFps = g_gameConfig.GetBool(GameConfigKeys::ShowFps);
	g_gameWindow->SetVSync(g_gameConfig.GetBool(GameConfigKeys::VSync) ? 1 : 0);

	///TODO: check if directory exists already?
	Path::CreateDir(Path::Absolute("screenshots"));
	Path::CreateDir(Path::Absolute("songs"));

	return true;
}
void Application::m_MainLoop()
{
	Timer appTimer;
	m_lastRenderTime = 0.0f;
	while(true)
	{
		//run discord callbacks
		Discord_RunCallbacks();

		// Process changes in the list of items
		bool restoreTop = false;
		for(auto& ch : g_tickableChanges)
		{
			if(ch.mode == TickableChange::Added)
			{
				assert(ch.tickable);
				if(!ch.tickable->DoInit())
				{
					Log("Failed to add IApplicationTickable", Logger::Error);
					delete ch.tickable;
					continue;
				}

				if(!g_tickables.empty())
					g_tickables.back()->m_Suspend();

				auto insertionPoint = g_tickables.end();
				if(ch.insertBefore)
				{
					// Find insertion point
					for(insertionPoint = g_tickables.begin(); insertionPoint != g_tickables.end(); insertionPoint++)
					{
						if(*insertionPoint == ch.insertBefore)
							break;
					}
				}
				g_tickables.insert(insertionPoint, ch.tickable);
				
				restoreTop = true;
			}
			else if(ch.mode == TickableChange::Removed)
			{
				// Remove focus
				ch.tickable->m_Suspend();

				assert(!g_tickables.empty());
				if(g_tickables.back() == ch.tickable)
					restoreTop = true;
				g_tickables.Remove(ch.tickable);
				delete ch.tickable;
			}
		}
		if(restoreTop && !g_tickables.empty())
			g_tickables.back()->m_Restore();

		// Application should end, no more active screens
		if(!g_tickableChanges.empty() && g_tickables.empty())
		{
			Log("No more IApplicationTickables, shutting down", Logger::Warning);
			return;
		}
		g_tickableChanges.clear();

		// Determine target tick rates for update and render
		int32 targetFPS = 120; // Default to 120 FPS
		float targetRenderTime = 0.0f;
		for(auto tickable : g_tickables)
		{
			int32 tempTarget = 0;
			if(tickable->GetTickRate(tempTarget))
			{
				targetFPS = tempTarget;
			}
		}
		if(targetFPS > 0)
			targetRenderTime = 1.0f / (float)targetFPS;
			

		// Main loop
		float currentTime = appTimer.SecondsAsFloat();
		float timeSinceRender = currentTime - m_lastRenderTime;
		if(timeSinceRender > targetRenderTime)
		{
			// Calculate actual deltatime for timing calculations
			currentTime = appTimer.SecondsAsFloat();
			float actualDeltaTime = currentTime - m_lastRenderTime;
			g_avgRenderDelta = g_avgRenderDelta * 0.98f + actualDeltaTime * 0.02f; // Calculate avg

			m_deltaTime = actualDeltaTime;
			m_lastRenderTime = currentTime;

			// Set time in render state
			m_renderStateBase.time = currentTime;

			// Also update window in render loop
			if(!g_gameWindow->Update())
				return;

			m_Tick();
			timeSinceRender = 0.0f;

			// Garbage collect resources
			ResourceManagers::TickAll();
		}

		// Tick job sheduler
		// processed callbacks for finished tasks
		g_jobSheduler->Update();

		if(timeSinceRender < targetRenderTime)
		{
			float timeLeft = (targetRenderTime - timeSinceRender);
			uint32 sleepMicroSecs = (uint32)(timeLeft*1000000.0f * 0.75f);
			std::this_thread::sleep_for(std::chrono::microseconds(sleepMicroSecs));
		}
	}
}

void Application::m_Tick()
{
	// Handle input first
	g_input.Update(m_deltaTime);

	// Process async lua http callbacks
	m_skinHttp.ProcessCallbacks();

	// Tick all items
	for(auto& tickable : g_tickables)
	{
		tickable->Tick(m_deltaTime);
	}

	// Not minimized / Valid resolution
	if(g_resolution.x > 0 && g_resolution.y > 0)
	{
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

		g_guiState.scissor = Rect(0,0,-1,-1);
		g_guiState.imageTint = nvgRGB(255, 255, 255);
		// Render all items
		assert(!g_tickables.empty());
		for(auto& tickable : g_tickables)
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
		}
		nvgEndFrame(g_guiState.vg);
		m_renderQueueBase.Process();
		glCullFace(GL_FRONT);
		// Swap buffers
		g_gl->SwapBuffers();
	}
}

void Application::m_Cleanup()
{
	ProfilerScope $("Application Cleanup");

	for(auto it : g_tickables)
	{
		delete it;
	}
	g_tickables.clear();

	if(g_audio)
	{
		delete g_audio;
		g_audio = nullptr;
	}

	if(g_gl)
	{
		delete g_gl;
		g_gl = nullptr;
	}

	// Cleanup input
	g_input.Cleanup();

	// Cleanup window after this
	if(g_gameWindow)
	{
		delete g_gameWindow;
		g_gameWindow = nullptr;
	}

	if(g_jobSheduler)
	{
		delete g_jobSheduler;
		g_jobSheduler = nullptr;
	}

	if (g_skinConfig)
	{
		delete g_skinConfig;
		g_skinConfig = nullptr;
	}
	if (m_gauge)
	{
		delete m_gauge;
		m_gauge = nullptr;
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

	Discord_Shutdown();

	nvgDeleteGL3(g_guiState.vg);

	if(m_updateThread.joinable())
		m_updateThread.join();


	// Finally, save config
	m_SaveConfig();
}

class Game* Application::LaunchMap(const String& mapPath)
{
	Game* game = Game::Create(mapPath, GameFlags::None);
	TransitionScreen* screen = TransitionScreen::Create(game);
	AddTickable(screen);
	return game;
}
void Application::Shutdown()
{
	g_gameWindow->Close();
}

void Application::AddTickable(class IApplicationTickable* tickable, class IApplicationTickable* insertBefore)
{
	TickableChange& change = g_tickableChanges.Add();
	change.mode = TickableChange::Added;
	change.tickable = tickable;
	change.insertBefore = insertBefore;
}
void Application::RemoveTickable(IApplicationTickable* tickable)
{
	TickableChange& change = g_tickableChanges.Add();
	change.mode = TickableChange::Removed;
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

const Vector<String>& Application::GetAppCommandLine() const
{
	return m_commandLine;
}
RenderState Application::GetRenderStateBase() const
{
	return m_renderStateBase;
}

RenderQueue* Application::GetRenderQueueBase()
{
	return &m_renderQueueBase;
}

Graphics::Image Application::LoadImage(const String& name)
{
	String path = String("skins/") + m_skin + String("/textures/") + name;
	return ImageRes::Create(Path::Absolute(path));
}

Graphics::Image Application::LoadImageExternal(const String& name)
{
	return ImageRes::Create(name);
}
Texture Application::LoadTexture(const String& name)
{
	Texture ret = TextureRes::Create(g_gl, LoadImage(name));
	return ret;
}

Texture Application::LoadTexture(const String& name, const bool& external)
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
Material Application::LoadMaterial(const String& name)
{
	String pathV = String("skins/") + m_skin + String("/shaders/") + name + ".vs";
	String pathF = String("skins/") + m_skin + String("/shaders/") + name + ".fs";
	String pathG = String("skins/") + m_skin + String("/shaders/") + name + ".gs";
	pathV = Path::Absolute(pathV);
	pathF = Path::Absolute(pathF);
	pathG = Path::Absolute(pathG);
	Material ret = MaterialRes::Create(g_gl, pathV, pathF);
	// Additionally load geometry shader
	if(Path::FileExists(pathG))
	{
		Shader gshader = ShaderRes::Create(g_gl, ShaderType::Geometry, pathG);
		assert(gshader);
		ret->AssignShader(ShaderType::Geometry, gshader);
	}
	assert(ret);
	return ret;
}
Sample Application::LoadSample(const String& name, const bool& external)
{
    String path;
    if(external)
	    path = name;
    else
        path = Path::Absolute(String("skins/") + m_skin + String("/audio/") + name);

	path = Path::Normalize(path);
	String ext = Path::GetExtension(path);
	if (ext.empty())
		path += ".wav";

	Sample ret = g_audio->CreateSample(path);
	assert(ret);
	return ret;
}

Graphics::Font Application::LoadFont(const String & name, const bool & external)
{
	Graphics::Font* cached = m_fonts.Find(name);
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

int Application::LoadImageJob(const String & path, Vector2i size, int placeholder, const bool& web)
{
	int ret = placeholder;
	auto it = m_jacketImages.find(path);
	if (it == m_jacketImages.end() || !it->second)
	{
		CachedJacketImage* newImage = new CachedJacketImage();
		JacketLoadingJob* job = new JacketLoadingJob();
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

void Application::SetScriptPath(lua_State * s)
{
	//Set path for 'require' (https://stackoverflow.com/questions/4125971/setting-the-global-lua-path-variable-from-c-c?lq=1)
	String lua_path = Path::Normalize(
		 Path::Absolute("./skins/" + m_skin + "/scripts/?.lua;")
		+ Path::Absolute("./skins/" + m_skin + "/scripts/?"));

	lua_getglobal(s, "package");
	lua_getfield(s, -1, "path"); // get field "path" from table at top of stack (-1)
	std::string cur_path = lua_tostring(s, -1); // grab path string from top of stack
	cur_path.append(";"); // do your path magic here
	cur_path.append(lua_path.c_str());
	lua_pop(s, 1); // get rid of the string on the stack we just pushed on line 5
	lua_pushstring(s, cur_path.c_str()); // push the new one
	lua_setfield(s, -2, "path"); // set the field "path" in table at -2 with value at top of stack
	lua_pop(s, 1); // get rid of package table from top of stack
}

lua_State* Application::LoadScript(const String & name, bool noError)
{
	lua_State* s = luaL_newstate();
	luaL_openlibs(s);
	SetScriptPath(s);

	String path = "skins/" + m_skin + "/scripts/" + name + ".lua";
	String commonPath = "skins/" + m_skin + "/scripts/" + "common.lua";
	path = Path::Absolute(path);
	commonPath = Path::Absolute(commonPath);
	m_SetNvgLuaBindings(s);
	if (luaL_dofile(s, commonPath.c_str()) || luaL_dofile(s, path.c_str()))
	{
		Logf("Lua error: %s", Logger::Error, lua_tostring(s, -1));
		if(!noError)
			g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(s, -1), 0);
		lua_close(s);
		return nullptr;
	}
	return s;
}

void Application::ReloadScript(const String& name, lua_State* L)
{
	SetScriptPath(L);
	String path = "skins/" + m_skin + "/scripts/" + name + ".lua";
	String commonPath = "skins/" + m_skin + "/scripts/" + "common.lua";
	path = Path::Absolute(path);
	commonPath = Path::Absolute(commonPath);
	if (luaL_dofile(L, commonPath.c_str()) || luaL_dofile(L, path.c_str()))
	{
		Logf("Lua error: %s", Logger::Error, lua_tostring(L, -1));
		g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(L, -1), 0);
		lua_close(L);
		assert(false);
	}
}

void Application::ReloadSkin()
{
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
	nvgDeleteGL3(g_guiState.vg);
#ifdef _DEBUG
	g_guiState.vg = nvgCreateGL3(NVG_DEBUG);
#else
	g_guiState.vg = nvgCreateGL3(0);
#endif

	nvgCreateFont(g_guiState.vg, "fallback", "fonts/NotoSansCJKjp-Regular.otf");
}
void Application::DisposeLua(lua_State* state)
{
	DisposeGUI(state);
	m_skinHttp.ClearState(state);
	lua_close(state);
}
void Application::SetGaugeColor(int i, Color c)
{
	m_gaugeColors[i] = c;
	if (m_gauge->colorBorder < 0.5f)
	{
		m_gauge->lowerColor = m_gaugeColors[2];
		m_gauge->upperColor = m_gaugeColors[3];
	}
	else
	{
		m_gauge->lowerColor = m_gaugeColors[0];
		m_gauge->upperColor = m_gaugeColors[1];
	}
}
void Application::DiscordError(int errorCode, const char * message)
{
	Logf("[Discord] %s", Logger::Warning, message);
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

void Application::DiscordPresenceSong(const BeatmapSettings& song, int64 startTime, int64 endTime)
{
	Vector<String> diffNames = { "NOV", "ADV", "EXH", "INF" };
	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));
	char bufferState[128] = { 0 };
	sprintf(bufferState, "Playing [%s %d]", diffNames[song.difficulty].c_str(), song.level);
	discordPresence.state = bufferState;
	char bufferDetails[128] = { 0 };
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
	MultiplayerScreen* mpScreen = new MultiplayerScreen();
	IApplicationTickable* title = (IApplicationTickable*)TitleScreen::Create();
	auto transition = TransitionScreen::Create(mpScreen);
	String* token = new String(*secret);
	auto tokenInput = [=](void* screen)
	{
		MultiplayerScreen* mpScreen = (MultiplayerScreen*)screen;
		mpScreen->JoinRoomWithToken(*token);
		delete token;
	};
	transition->OnLoadingComplete.AddLambda(std::move(tokenInput));
	title->m_Suspend();

	//Remove all tickables and add back a titlescreen as a base
	AddTickable(title);
	AddTickable(transition);
	for(IApplicationTickable* tickable : g_tickables)
	{
		RemoveTickable(tickable);
	}
}

void Application::LoadGauge(bool hard)
{
	String gaugePath = "gauges/normal/";
	if (hard)
	{
		gaugePath = "gauges/hard/";
		m_gauge->colorBorder = 0.3f;
		m_gauge->lowerColor = m_gaugeColors[2];
		m_gauge->upperColor = m_gaugeColors[3];
	}
	else
	{
		m_gauge->colorBorder = 0.7f;
		m_gauge->lowerColor = m_gaugeColors[0];
		m_gauge->upperColor = m_gaugeColors[1];
	}
	m_gauge->fillTexture = LoadTexture(gaugePath + "gauge_fill.png");
	m_gauge->frontTexture = LoadTexture(gaugePath + "gauge_front.png");
	m_gauge->backTexture = LoadTexture(gaugePath + "gauge_back.png");
	m_gauge->maskTexture = LoadTexture(gaugePath + "gauge_mask.png");
	m_gauge->fillTexture->SetWrap(Graphics::TextureWrap::Clamp, Graphics::TextureWrap::Clamp);
	m_gauge->frontTexture->SetWrap(Graphics::TextureWrap::Clamp, Graphics::TextureWrap::Clamp);
	m_gauge->backTexture->SetWrap(Graphics::TextureWrap::Clamp, Graphics::TextureWrap::Clamp);
	m_gauge->maskTexture->SetWrap(Graphics::TextureWrap::Clamp, Graphics::TextureWrap::Clamp);
	m_gauge->fillMaterial = LoadMaterial("gauge");
	m_gauge->fillMaterial->opaque = false;
	m_gauge->baseMaterial = LoadMaterial("guiTex");
	m_gauge->baseMaterial->opaque = false;
}

void Application::DrawGauge(float rate, float x, float y, float w, float h, float deltaTime)
{
	m_gauge->rate = rate;
	Mesh m = MeshGenerators::Quad(g_gl, Vector2(x, y), Vector2(w, h));
	m_gauge->Render(m, deltaTime);
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
void Application::StoreNamedSample(String name, Sample sample)
{
	m_samples.Add(name, sample);
}
void Application::PlayNamedSample(String name, bool loop)
{
	m_samples[name]->Play(loop);
}
void Application::StopNamedSample(String name)
{
	m_samples[name]->Stop();
}
void Application::m_OnKeyPressed(int32 key)
{
	// Fullscreen toggle
	if(key == SDLK_RETURN)
	{
		if((g_gameWindow->GetModifierKeys() & ModifierKeys::Alt) == ModifierKeys::Alt)
		{
			g_gameWindow->SwitchFullscreen(
				g_gameConfig.GetInt(GameConfigKeys::ScreenWidth), g_gameConfig.GetInt(GameConfigKeys::ScreenHeight),
				g_gameConfig.GetInt(GameConfigKeys::FullScreenWidth), g_gameConfig.GetInt(GameConfigKeys::FullScreenHeight),
				-1, g_gameConfig.GetBool(GameConfigKeys::WindowedFullscreen)
			);
			g_gameConfig.Set(GameConfigKeys::Fullscreen, g_gameWindow->IsFullscreen());
			//m_OnWindowResized(g_gameWindow->GetWindowSize());
			return;
		}
	}

	// Pass key to application
	for(auto it = g_tickables.rbegin(); it != g_tickables.rend();)
	{
		(*it)->OnKeyPressed(key);
		break;
	}
}
void Application::m_OnKeyReleased(int32 key)
{
	for(auto it = g_tickables.rbegin(); it != g_tickables.rend();)
	{
		(*it)->OnKeyReleased(key);
		break;
	}
}
void Application::m_OnWindowResized(const Vector2i& newSize)
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
		if (g_gameWindow->IsFullscreen()) {
			g_gameConfig.Set(GameConfigKeys::FullscreenMonitorIndex, g_gameWindow->GetDisplayIndex());
		}
		else {
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
		if (g_gameWindow->IsFullscreen()) {
			g_gameConfig.Set(GameConfigKeys::FullscreenMonitorIndex, g_gameWindow->GetDisplayIndex());
		}
		else {
			g_gameConfig.Set(GameConfigKeys::ScreenWidth, newSize.x);
			g_gameConfig.Set(GameConfigKeys::ScreenHeight, newSize.y);
		}
	}

}

int Application::FastText(String inputText, float x, float y, int size, int align)
{
	WString text = Utility::ConvertToWString(inputText);
	String fontpath = Path::Absolute(Path::Normalize("fonts/settings/NotoSans-Regular.ttf"));
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
	params.SetParameter("color", Vector4(1.f, 1.f, 1.f, 1.f));
	g_application->GetRenderQueueBase()->Draw(textTransform, te, g_application->GetFontMaterial(), params);
	return 0;
}

static int lGetMousePos(lua_State* L)
{
	Vector2i pos = g_gameWindow->GetMousePos();
	float left = g_gameWindow->GetWindowSize().x / 2 - g_resolution.x / 2;
	lua_pushnumber(L, pos.x - left);
	lua_pushnumber(L, pos.y);
	return 2;
}

static int lGetResolution(lua_State* L)
{
	lua_pushnumber(L, g_resolution.x);
	lua_pushnumber(L, g_resolution.y);
	return 2;
}

static int lGetLaserColor(lua_State* L /*int laser*/)
{
	int laser = luaL_checkinteger(L, 1);
	float laserHues[2] = { 0.f };
	laserHues[0] = g_gameConfig.GetFloat(GameConfigKeys::Laser0Color);
	laserHues[1] = g_gameConfig.GetFloat(GameConfigKeys::Laser1Color);
	Colori c = Color::FromHSV(laserHues[laser], 1.0, 1.0).ToRGBA8();
	lua_pushnumber(L, c.x);
	lua_pushnumber(L, c.y);
	lua_pushnumber(L, c.z);
	return 3;
}

static int lLog(lua_State* L)
{
	String msg = luaL_checkstring(L, 1);
	int severity = luaL_checkinteger(L, 2);
	Log(msg, (Logger::Severity)severity);
	return 0;
}

static int lDrawGauge(lua_State* L)
{
	float rate, x, y, w, h, deltaTime;
	rate = luaL_checknumber(L, 1);
	x = luaL_checknumber(L, 2);
	y = luaL_checknumber(L, 3);
	w = luaL_checknumber(L, 4);
	h = luaL_checknumber(L, 5);
	deltaTime = luaL_checknumber(L, 6);
	g_application->DrawGauge(rate, x, y, w, h, deltaTime);
	return 0;
}

static int lGetButton(lua_State* L /* int button */)
{
	int button = luaL_checkinteger(L, 1);
	lua_pushboolean(L, g_input.GetButton((Input::Button)button));
	return 1;
}
static int lGetKnob(lua_State* L /* int knob */)
{
	int knob = luaL_checkinteger(L, 1);
	lua_pushnumber(L, g_input.GetAbsoluteLaser(knob));
	return 1;
}
static int lGetUpdateAvailable(lua_State* L)
{
	Vector<String> info = g_application->GetUpdateAvailable();
	if(info.empty())
	{
		return 0;
	}

	lua_pushstring(L, *info[0]);
	lua_pushstring(L, *info[1]);
	return 2;
}

static int lCreateSkinImage(lua_State* L /*const char* filename, int imageflags */)
{
	const char* filename = luaL_checkstring(L, 1);
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

static int lLoadSkinAnimation(lua_State* L)
{
	const char* p;
	float frametime;
	int loopcount = 0;

	p = luaL_checkstring(L, 1);
	frametime = luaL_checknumber(L, 2);
	if (lua_gettop(L) == 3)
	{
		loopcount = luaL_checkinteger(L, 3);
	}

	String path = "skins/" + g_application->GetCurrentSkin() + "/textures/" + p;
	path = Path::Absolute(path);
	int result = LoadAnimation(L, *path, frametime, loopcount);
	if (result == -1)
		return 0;

	lua_pushnumber(L, result);
	return 1;
}

static int lLoadSkinFont(lua_State* L /*const char* name */)
{
	const char* name = luaL_checkstring(L, 1);
	String path = "skins/" + g_application->GetCurrentSkin() + "/fonts/" + name;
	path = Path::Absolute(path);
	LoadFont(name, path.c_str());
	return 0;
}

static int lLoadSkinSample(lua_State* L /*char* name */)
{
	const char* name = luaL_checkstring(L, 1);
	g_application->StoreNamedSample(name, g_application->LoadSample(name));
	return 0;
}

static int lPlaySample(lua_State* L /*char* name, bool loop */)
{
	const char* name = luaL_checkstring(L, 1);
	bool loop = false;
	if(lua_gettop(L) == 2)
	{
		loop = lua_toboolean(L,2) == 1;
	}

	g_application->PlayNamedSample(name, loop);
	return 0;
}

static int lStopSample(lua_State* L /* char* name */)
{
	const char* name = luaL_checkstring(L, 1);
	g_application->StopNamedSample(name);
	return 0;
}

static int lPathAbsolute(lua_State* L /* string path */)
{
	const char* path = luaL_checkstring(L, 1);
	lua_pushstring(L, *Path::Absolute(path));
	return 1;
}

static int lForceRender(lua_State* L)
{
	g_application->ForceRender();
	return 0;
}

static int lLoadImageJob(lua_State* L /* char* path, int placeholder, int w = 0, int h = 0 */)
{
	const char* path = luaL_checkstring(L, 1);
	int fallback = luaL_checkinteger(L, 2);
	int w = 0, h = 0;
	if (lua_gettop(L) == 4)
	{
		w = luaL_checkinteger(L, 3);
		h = luaL_checkinteger(L, 4);
	}
	lua_pushinteger(L, g_application->LoadImageJob(path, { w,h }, fallback));
	return 1;
}

static int lLoadWebImageJob(lua_State* L /* char* url, int placeholder, int w = 0, int h = 0 */)
{
	const char* url = luaL_checkstring(L, 1);
	int fallback = luaL_checkinteger(L, 2);
	int w = 0, h = 0;
	if (lua_gettop(L) == 4)
	{
		w = luaL_checkinteger(L, 3);
		h = luaL_checkinteger(L, 4);
	}
	lua_pushinteger(L, g_application->LoadImageJob(url, { w,h }, fallback, true));
	return 1;
}

static int lSetGaugeColor(lua_State* L /*int colorIndex, int r, int g, int b*/)
{
	int colorindex, r, g, b;
	colorindex = luaL_checkinteger(L, 1);
	r = luaL_checkinteger(L, 2);
	g = luaL_checkinteger(L, 3);
	b = luaL_checkinteger(L, 4);

	g_application->SetGaugeColor(colorindex, Colori(r, g, b));
	return 0;
}

static int lGetSkin(lua_State* L)
{
	lua_pushstring(L, *g_application->GetCurrentSkin());
	return 1;
}

static int lSetSkinSetting(lua_State* L /*String key, Any value*/)
{
	String key = luaL_checkstring(L, 1);
	IConfigEntry* entry = g_skinConfig->GetEntry(key);
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


static int lGetSkinSetting(lua_State* L /*String key*/)
{
	String key = luaL_checkstring(L, 1);
	IConfigEntry* entry = g_skinConfig->GetEntry(key);
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

void Application::m_SetNvgLuaBindings(lua_State * state)
{
	auto pushFuncToTable = [&](const char* name, int (*func)(lua_State*))
	{
		lua_pushstring(state, name);
		lua_pushcfunction(state, func);
		lua_settable(state, -3);
	};

	auto pushIntToTable = [&](const char* name, int data)
	{
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
		pushFuncToTable("DrawGauge", lDrawGauge);
		pushFuncToTable("SetGaugeColor", lSetGaugeColor);
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
		//constants
		//Text align
		pushIntToTable("TEXT_ALIGN_BASELINE",	NVGalign::NVG_ALIGN_BASELINE);
		pushIntToTable("TEXT_ALIGN_BOTTOM",		NVGalign::NVG_ALIGN_BOTTOM);
		pushIntToTable("TEXT_ALIGN_CENTER",		NVGalign::NVG_ALIGN_CENTER);
		pushIntToTable("TEXT_ALIGN_LEFT",		NVGalign::NVG_ALIGN_LEFT);
		pushIntToTable("TEXT_ALIGN_MIDDLE",		NVGalign::NVG_ALIGN_MIDDLE);
		pushIntToTable("TEXT_ALIGN_RIGHT",		NVGalign::NVG_ALIGN_RIGHT);
		pushIntToTable("TEXT_ALIGN_TOP",		NVGalign::NVG_ALIGN_TOP);
		//Line caps and joins
		pushIntToTable("LINE_BEVEL",	NVGlineCap::NVG_BEVEL);
		pushIntToTable("LINE_BUTT",		NVGlineCap::NVG_BUTT);
		pushIntToTable("LINE_MITER",	NVGlineCap::NVG_MITER);
		pushIntToTable("LINE_ROUND",	NVGlineCap::NVG_ROUND);
		pushIntToTable("LINE_SQUARE",	NVGlineCap::NVG_SQUARE);
		//Image flags
		pushIntToTable("IMAGE_GENERATE_MIPMAPS",	NVGimageFlags::NVG_IMAGE_GENERATE_MIPMAPS);
		pushIntToTable("IMAGE_REPEATX",				NVGimageFlags::NVG_IMAGE_REPEATX);
		pushIntToTable("IMAGE_REPEATY",				NVGimageFlags::NVG_IMAGE_REPEATY);
		pushIntToTable("IMAGE_FLIPY",				NVGimageFlags::NVG_IMAGE_FLIPY);
		pushIntToTable("IMAGE_PREMULTIPLIED",		NVGimageFlags::NVG_IMAGE_PREMULTIPLIED);
		pushIntToTable("IMAGE_NEAREST",				NVGimageFlags::NVG_IMAGE_NEAREST);
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
		pushFuncToTable("GetLaserColor", lGetLaserColor);
		pushFuncToTable("GetButton", lGetButton);
		pushFuncToTable("GetKnob", lGetKnob);
		pushFuncToTable("UpdateAvailable", lGetUpdateAvailable);
		pushFuncToTable("GetSkin", lGetSkin);
		pushFuncToTable("GetSkin", lGetSkin);
		pushFuncToTable("GetSkinSetting", lGetSkinSetting);
		pushFuncToTable("SetSkinSetting", lSetSkinSetting);

		//constants
		pushIntToTable("LOGGER_INFO", Logger::Severity::Info);
		pushIntToTable("LOGGER_NORMAL", Logger::Severity::Normal);
		pushIntToTable("LOGGER_WARNING", Logger::Severity::Warning);
		pushIntToTable("LOGGER_ERROR", Logger::Severity::Error);
		pushIntToTable("BUTTON_BTA", (int)Input::Button::BT_0);
		pushIntToTable("BUTTON_BTB", (int)Input::Button::BT_1);
		pushIntToTable("BUTTON_BTC", (int)Input::Button::BT_2);
		pushIntToTable("BUTTON_BTD", (int)Input::Button::BT_3);
		pushIntToTable("BUTTON_FXL", (int)Input::Button::FX_0);
		pushIntToTable("BUTTON_FXR", (int)Input::Button::FX_1);
		pushIntToTable("BUTTON_STA", (int)Input::Button::BT_S);

		lua_setglobal(state, "game");
	}

	//path
	{
		lua_newtable(state);
		pushFuncToTable("Absolute", lPathAbsolute);
		lua_setglobal(state, "path");
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
		if (loadedImage.IsValid()) {
			if (loadedImage->GetSize().x > w || loadedImage->GetSize().y > h) {
				loadedImage->ReSize({ w,h });
			}
		}
		return loadedImage.IsValid();
	}
	else
	{
		loadedImage = ImageRes::Create(imagePath);
		if (loadedImage.IsValid()) {
			if (loadedImage->GetSize().x > w || loadedImage->GetSize().y > h) {
				loadedImage->ReSize({ w,h });
			}
		}
		return loadedImage.IsValid();
	}
}
void JacketLoadingJob::Finalize()
{
	if (IsSuccessfull())
	{
		///TODO: Maybe do the nvgCreateImage in Run() instead
		target->texture = nvgCreateImageRGBA(g_guiState.vg, loadedImage->GetSize().x, loadedImage->GetSize().y, 0, (unsigned char*)loadedImage->GetBits());
		target->loaded = true;
	}
}
