#pragma once
#include <Audio/Sample.hpp>
#include <Shared/Jobs.hpp>
#include <Shared/Thread.hpp>
#include "SkinHttp.hpp"

#define DISCORD_APPLICATION_ID "514489760568573952"

extern class OpenGL* g_gl;
extern class GUIState g_guiState;
extern class Graphics::Window* g_gameWindow;
extern float g_aspectRatio;
extern Vector2i g_resolution;
extern class Application* g_application;
extern class JobSheduler* g_jobSheduler;
extern class Input g_input;
extern class SkinConfig* g_skinConfig;

class Application
{
public:
	Application();
	~Application();

	struct CachedJacketImage
	{
		float lastUsage;
		int texture;
		bool loaded = false;
		Job loadingJob;
	};
	void ApplySettings();
	// Runs the application
	int32 Run();
	
	void SetCommandLine(int32 argc, char** argv);
	void SetCommandLine(const char* cmdLine);

	class Game* LaunchMap(const String& mapPath);
	void Shutdown();

	void AddTickable(class IApplicationTickable* tickable, class IApplicationTickable* insertBefore = nullptr);
	void RemoveTickable(class IApplicationTickable* tickable);

	// Current running map path (full file path)
	String GetCurrentMapPath();

	// Current loaded skin;
	String GetCurrentSkin();

	// Retrieves application command line parameters
	const Vector<String>& GetAppCommandLine() const;

	// Gets a basic template for a render state, with all the application variables initialized
	RenderState GetRenderStateBase() const;
	RenderQueue* GetRenderQueueBase();

#ifdef LoadImage
#undef LoadImage
#endif
	Image LoadImage(const String& name);
	Graphics::Image LoadImageExternal(const String & name);
	Texture LoadTexture(const String& name);
	Texture LoadTexture(const String & name, const bool& external);
	Material LoadMaterial(const String& name);
	Sample LoadSample(const String& name, const bool& external = false);
	Graphics::Font LoadFont(const String& name, const bool& external = false);
	int LoadImageJob(const String& path, Vector2i size, int placeholder, const bool& web = false);
	void SetScriptPath(lua_State* L);
	lua_State* LoadScript(const String& name, bool noError = false);
	void ReloadScript(const String& name, lua_State* L);
	void LoadGauge(bool hard);
	void DrawGauge(float rate, float x, float y, float w, float h, float deltaTime);
	int FastText(String text, float x, float y, int size, int align);
	float GetAppTime() const { return m_lastRenderTime; }
	float GetRenderFPS() const;
	Material GetFontMaterial() const;
	Material GetGuiTexMaterial() const;
	Transform GetGUIProjection() const;
	void StoreNamedSample(String name, Sample sample);
	void PlayNamedSample(String name, bool loop);
	void StopNamedSample(String name);
	void ReloadSkin();
	void DisposeLua(lua_State* state);
	void SetGaugeColor(int i, Color c);
	void DiscordError(int errorCode, const char* message);
	void DiscordPresenceMenu(String name);
	void DiscordPresenceMulti(String secret, int partySize, int partyMax, String id);
	void DiscordPresenceSong(const struct BeatmapSettings& song, int64 startTime, int64 endTime);
	void JoinMultiFromInvite(String secret);
	void SetUpdateAvailable(const String& version, const String& url, const String& download);
	void RunUpdater();
	void ForceRender();
	void SetLuaBindings(struct lua_State* state);
	struct NVGcontext* GetVGContext();

	//if empty: no update avaiable
	//else: index 0 = url, index 1 = version
	Vector<String> GetUpdateAvailable();

private:
	bool m_LoadConfig();
	void m_SaveConfig();
	void m_InitDiscord();
	bool m_Init();
	void m_MainLoop();
	void m_Tick();
	void m_Cleanup();
	void m_OnKeyPressed(int32 key);
	void m_OnKeyReleased(int32 key);
	void m_OnWindowResized(const Vector2i& newSize);

	RenderState m_renderStateBase;
	RenderQueue m_renderQueueBase;
	Vector<String> m_commandLine;
	Map<String, Graphics::Font> m_fonts;
	Map<String, Sample> m_samples;
	Material m_fontMaterial;
	Material m_fillMaterial;
	Material m_guiTex;
	class HealthGauge* m_gauge;
	Map<String, CachedJacketImage*> m_jacketImages;
	String m_lastMapPath;
	Thread m_updateThread;
	class Beatmap* m_currentMap = nullptr;
	SkinHttp m_skinHttp;

	float m_lastRenderTime;
	float m_deltaTime;
	bool m_allowMapConversion;
	bool m_hasUpdate = false;
	bool m_showFps = false;
	String m_updateUrl;
	String m_updateDownload;
	String m_updateVersion;
	String m_currentVersion;
	String m_skin;
	Timer m_jobTimer;
	//gauge colors, 0 = normal fail, 1 = normal clear, 2 = hard lower, 3 = hard upper
	Color m_gaugeColors[4] = { Colori(0, 204, 255), Colori(255, 102, 255), Colori(200, 50, 0), Colori(255, 100, 0) };

	String m_multiRoomSecret;
	String m_multiRoomId;
	int m_multiRoomSize = 0;
	int m_multiRoomCount = 0;
};

class JacketLoadingJob : public JobBase
{
public:
	virtual bool Run();
	virtual void Finalize();

	Image loadedImage;
	String imagePath;
	int w = 0, h = 0;
	bool web = false;
	Application::CachedJacketImage* target;
};

void __discordJoinGame(const char* joins);
