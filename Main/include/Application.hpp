#pragma once
#include "SkinHttp.hpp"
#include "SkinIR.hpp"
#include "Scoring.hpp"

#define DISCORD_APPLICATION_ID "514489760568573952"

extern class OpenGL* g_gl;
extern struct GUIState g_guiState;
extern class Graphics::Window* g_gameWindow;
extern float g_aspectRatio;
extern Vector2i g_resolution;
extern class Application* g_application;
extern class JobSheduler* g_jobSheduler;
extern class Input g_input;
extern class SkinConfig* g_skinConfig;
extern class TransitionScreen* g_transition;

class SharedTexture;

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
	void RemoveTickable(class IApplicationTickable* tickable, bool noDelete = false);

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
	Material LoadMaterial(const String& name, const String& path);
	Sample LoadSample(const String& name, const bool& external = false);
	Graphics::Font LoadFont(const String& name, const bool& external = false);
	int LoadImageJob(const String& path, Vector2i size, int placeholder, const bool& web = false);
	void SetScriptPath(lua_State* L);


	// Called when a pcall fails, returns true if the script was reloaded
	bool ScriptError(const String& name, lua_State* L);

	lua_State* LoadScript(const String& name, bool noError = false);
	bool ReloadScript(const String& name, lua_State* L);

	void WarnGauge();
	int FastText(String text, float x, float y, int size, int align, const Color& color = Color::White);
	float GetAppTime() const { return m_appTime; }
	float GetRenderFPS() const;
	Material GetFontMaterial() const;
	Material GetGuiTexMaterial() const;
	Material GetGuiFillMaterial() const;
	Transform GetGUIProjection() const;
	Transform GetCurrentGUITransform() const;
	Rect GetCurrentGUIScissor() const;
	void StoreNamedSample(String name, Sample sample);
	void PlayNamedSample(String name, bool loop);
	void StopNamedSample(String name);
	// -1 if no sample exists, 0 if stopped, 1 if playing
	int IsNamedSamplePlaying(String name);
	void ReloadSkin();
	bool ReloadConfig(const String& profile = "");
	void DisposeLua(lua_State* state);
	void DiscordError(int errorCode, const char* message);
	void DiscordPresenceMenu(String name);
	void DiscordPresenceMulti(String secret, int partySize, int partyMax, String id);
	void DiscordPresenceSong(const struct BeatmapSettings& song, int64 startTime, int64 endTime);
	void JoinMultiFromInvite(String secret);
	void SetUpdateAvailable(const String& version, const String& url, const String& download);
	void RunUpdater();
	void CheckForUpdate();
	void ForceRender();
	void SetLuaBindings(struct lua_State* state);
	struct NVGcontext* GetVGContext();

	//if empty: no update avaiable
	//else: index 0 = url, index 1 = version
	Vector<String> GetUpdateAvailable();

	AutoplayInfo* autoplayInfo = nullptr;
	Map<String, Ref<SharedTexture>> sharedTextures;

private:
	bool m_LoadConfig(String profileName = "");
	void m_UpdateConfigVersion();
	void m_SaveConfig();
	void m_InitDiscord();
	bool m_Init();
	void m_MainLoop();
	void m_Tick();
	void m_Cleanup();
	void m_OnKeyPressed(SDL_Scancode code);
	void m_OnKeyReleased(SDL_Scancode code);
	void m_UpdateWindowPosAndShape();
	void m_UpdateWindowPosAndShape(int32 monitorId, bool fullscreen, bool ensureInBound);
	void m_OnWindowResized(const Vector2i& newSize);
	void m_OnWindowMoved(const Vector2i& newPos);
	void m_OnFocusChanged(bool focused);
	void m_unpackSkins();

	RenderState m_renderStateBase;
	RenderQueue m_renderQueueBase;
	Vector<String> m_commandLine;
	Map<String, Graphics::Font> m_fonts;
	Map<String, Sample> m_samples;
	Material m_fontMaterial;
	Material m_fillMaterial;
	Material m_guiTex;
	Map<String, CachedJacketImage*> m_jacketImages;
	String m_lastMapPath;
	Thread m_updateThread;
	Thread m_fontBakeThread;
	class Beatmap* m_currentMap = nullptr;
	SkinHttp m_skinHttp;
	SkinIR m_skinIR;

	float m_deltaTime;
	float m_fpsTargetSleepMult = 1.0f;
	float m_appTime;
	bool m_allowMapConversion;
	bool m_hasUpdate = false;
	bool m_showFps = false;
	String m_updateUrl;
	String m_updateDownload;
	String m_updateVersion;
	String m_currentVersion;
	String m_skin;
	bool m_needSkinReload = false;
	Timer m_jobTimer;
	//gauge colors, 0 = normal fail, 1 = normal clear, 2 = hard lower, 3 = hard upper
	Color m_gaugeColors[4] = { Colori(0, 204, 255), Colori(255, 102, 255), Colori(200, 50, 0), Colori(255, 100, 0) };

	String m_multiRoomSecret;
	String m_multiRoomId;
	int m_multiRoomSize = 0;
	int m_multiRoomCount = 0;
	bool m_gaugeRemovedWarn = true;
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

class SharedTexture {
public:
	SharedTexture() = default;
	~SharedTexture();
	bool Valid();
	int nvgTexture = 0;
	Texture texture;
};