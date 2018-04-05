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
#include <GUI/GUIRenderer.hpp>
#include "Input.hpp"
#include <GUI/Canvas.hpp>
#include <GUI/CommonGUIStyle.hpp>
#include "TransitionScreen.hpp"
#ifdef _WIN32
#include "SDL_keycode.h"
#else
#include "SDL2/SDL_keycode.h"
#endif

GameConfig g_gameConfig;
OpenGL* g_gl = nullptr;
Graphics::Window* g_gameWindow = nullptr;
Application* g_application = nullptr;
JobSheduler* g_jobSheduler = nullptr;
Input g_input;

GUIRenderer* g_guiRenderer = nullptr;
Ref<Canvas> g_rootCanvas;
Ref<class CommonGUIStyle> g_commonGUIStyle;

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

bool Application::m_LoadConfig()
{
	File configFile;
	if(configFile.OpenRead("Main.cfg"))
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
	if(configFile.OpenWrite("Main.cfg"))
	{
		FileWriter writer(configFile);
		g_gameConfig.Save(writer);
	}
}

bool Application::m_Init()
{
	ProfilerScope $("Application Setup");

	// Must have command line
	assert(m_commandLine.size() >= 1);

	if(!m_LoadConfig())
	{
		Logf("Failed to load config file", Logger::Warning);
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
	g_gameWindow = new Graphics::Window(g_resolution);

	g_gameWindow->Show();

	g_gameWindow->OnKeyPressed.Add(this, &Application::m_OnKeyPressed);
	g_gameWindow->OnKeyReleased.Add(this, &Application::m_OnKeyReleased);
	g_gameWindow->OnResized.Add(this, &Application::m_OnWindowResized);

	// Initialize Input
	g_input.Init(*g_gameWindow);

	// Set skin variable
	m_skin = g_gameConfig.GetString(GameConfigKeys::Skin);

	// Window cursor
	Image cursorImg = ImageRes::Create("skins/" + m_skin + "/textures/cursor.png");
	g_gameWindow->SetCursor(cursorImg, Vector2i(5, 5));

	if(startFullscreen)
	{
		g_gameWindow->SwitchFullscreen(
			g_gameConfig.GetInt(GameConfigKeys::ScreenWidth), g_gameConfig.GetInt(GameConfigKeys::ScreenHeight),
			g_gameConfig.GetInt(GameConfigKeys::FullScreenWidth), g_gameConfig.GetInt(GameConfigKeys::FullScreenHeight),
			fullscreenMonitor
		);
	}

	// Set render state variables
	m_renderStateBase.aspectRatio = g_aspectRatio;
	m_renderStateBase.viewportSize = g_resolution;
	m_renderStateBase.time = 0.0f;

	{
		ProfilerScope $1("Audio Init");

		// Init audio
		new Audio();
		if(!g_audio->Init())
		{
			Log("Audio initialization failed", Logger::Error);
			delete g_audio;
			return 1;
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
		if(!g_gl->Init(*g_gameWindow))
		{
			Log("Failed to create OpenGL context", Logger::Error);
			return false;
		}
	}

	// call the initial OnWindowResized now that we have intialized OpenGL
	m_OnWindowResized(g_resolution);

	g_gameWindow->SetVSync(g_gameConfig.GetInt(GameConfigKeys::VSync));

	{
		ProfilerScope $1("GUI Init");

		// GUI Rendering
		g_guiRenderer = new GUIRenderer();
		if (!g_guiRenderer->Init(g_gl, g_gameWindow, m_skin))
		{
			Logf("Failed to initialize GUI renderer", Logger::Error);
			return false;
		}
	}

	{
		ProfilerScope $1("Loading common GUI elements");
		// Load GUI style for common elements
		g_commonGUIStyle = Ref<CommonGUIStyle>(new CommonGUIStyle(g_gl, m_skin));
	}

	// Create root canvas
	g_rootCanvas = Ref<Canvas>(new Canvas());

	return true;
}
void Application::m_MainLoop()
{
	Timer appTimer;
	m_lastRenderTime = 0.0f;
	while(true)
	{
		// Process changes in the list of items
		bool restoreTop = false;
		for(auto& ch : g_tickableChanges)
		{
			if(ch.mode == TickableChange::Added)
			{
				assert(ch.tickable);
				if(!ch.tickable->DoInit())
				{
					Logf("Failed to add IApplicationTickable", Logger::Error);
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
			Logf("No more IApplicationTickables, shutting down", Logger::Warning);
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
			g_avgRenderDelta = g_avgRenderDelta * 0.75f + actualDeltaTime * 0.25f; // Calculate avg

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

	// Tick all items
	for(auto& tickable : g_tickables)
	{
		tickable->Tick(m_deltaTime);
	}

	// Not minimized / Valid resolution
	if(g_resolution.x > 0 && g_resolution.y > 0)
	{
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);

		// Render all items
		for(auto& tickable : g_tickables)
		{
			tickable->Render(m_deltaTime);
		}

		// Time to render GUI
		g_guiRenderer->Render(m_deltaTime, Rect(Vector2(0, 0), g_resolution), g_rootCanvas.As<GUIElementBase>());

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

const Vector<String>& Application::GetAppCommandLine() const
{
	return m_commandLine;
}
RenderState Application::GetRenderStateBase() const
{
	return m_renderStateBase;
}

Graphics::Image Application::LoadImage(const String& name)
{
	String path = String("skins/") + m_skin + String("/textures/") + name;
	return ImageRes::Create(path);
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
        path = String("skins/") + m_skin + String("/audio/") + name + ".wav";

	Sample ret = g_audio->CreateSample(path);
	assert(ret);
	return ret;
}

float Application::GetRenderFPS() const
{
	return 1.0f / g_avgRenderDelta;
}

Transform Application::GetGUIProjection() const
{
	return ProjectionMatrix::CreateOrthographic(0.0f, (float)g_resolution.x, (float)g_resolution.y, 0.0f, 0.0f, 100.0f);
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
				g_gameConfig.GetInt(GameConfigKeys::FullScreenWidth), g_gameConfig.GetInt(GameConfigKeys::FullScreenHeight)
			);
			g_gameConfig.Set(GameConfigKeys::Fullscreen, g_gameWindow->IsFullscreen());
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
	g_resolution = newSize;
	g_aspectRatio = (float)g_resolution.x / (float)g_resolution.y;

	m_renderStateBase.aspectRatio = g_aspectRatio;
	m_renderStateBase.viewportSize = g_resolution;

	glViewport(0, 0, newSize.x, newSize.y);
	glScissor(0, 0, newSize.x, newSize.y);

	// Set in config
	if (g_gameWindow->IsFullscreen()){
		g_gameConfig.Set(GameConfigKeys::FullscreenMonitorIndex, g_gameWindow->GetDisplayIndex());
	} else{
		g_gameConfig.Set(GameConfigKeys::ScreenWidth, newSize.x);
		g_gameConfig.Set(GameConfigKeys::ScreenHeight, newSize.y);
	}
}
