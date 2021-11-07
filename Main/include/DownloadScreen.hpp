#pragma once
#include "ApplicationTickable.hpp"
#include "Shared/LuaBindable.hpp"
#include "Input.hpp"
#include "PreviewPlayer.hpp"

class TextInput;

struct ArchiveRequest
{
	cpr::AsyncResponse response;
	String id;
	int callback;
};

struct ArchiveResponse
{
	struct archive* a;
	Buffer data;
	String id;
	int callback;
};

class DownloadScreen : public IApplicationTickable
{
public:
	DownloadScreen();
	~DownloadScreen();
	bool Init() override;
	void OnSearchTermChanged(const String& search);
	// Tick for tickable
	void Tick(float deltaTime) override;
	void Render(float deltaTime) override;

	void OnKeyPressed(SDL_Scancode code, int32 delta) override;
	void OnKeyReleased(SDL_Scancode code, int32 delta) override;
private:
	struct lua_State* m_lua;
	LuaBindable* m_bindable;
	float m_advanceSong = 0.0f;
	Mutex m_archiveLock;
	std::queue<ArchiveRequest> m_archiveReqs;
	std::queue<ArchiveResponse> m_archiveResps;
	Thread m_archiveThread;
	bool m_running = true;

	Ref<TextInput> m_searchInput;

	void m_ArchiveLoop();
	void m_OnButtonPressed(Input::Button buttonCode, int32 delta);
	void m_OnButtonReleased(Input::Button buttonCode, int32 delta);
	void m_OnMouseScroll(int32 steps);
	void m_ProcessArchiveResponses();
	int m_Exit(struct lua_State* L);
	int m_DownloadArchive(struct lua_State* L);
	int m_PlayPreview(struct lua_State* L);
	int m_StopPreview(struct lua_State* L);
	int m_GetSongsPath(struct lua_State* L);
	bool m_extractFile(struct archive* a, String path);
	Map<String, String> m_mapFromLuaTable(int index);

	PreviewPlayer m_previewPlayer;
	PreviewParams m_previewParams;

	// -1 if no search update needed
	float m_timeSinceSearchChange = -1;
	String m_lastSearch = "";
};