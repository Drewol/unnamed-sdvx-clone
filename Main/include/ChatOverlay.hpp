#pragma once
#include "ApplicationTickable.hpp"
#include "GameConfig.hpp"
#include "Application.hpp"
#include "Input.hpp"
#include <SDL2/SDL.h>
#include "GameConfig.hpp"
#include "json.hpp"

#include <string>

// NK imports
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#undef NK_IMPLEMENTATION
#ifdef EMBEDDED
#undef NK_SDL_GLES2_IMPLEMENTATION
#include "../third_party/nuklear/nuklear.h"
#include "nuklear/nuklear_sdl_gles2.h"
#else
#undef	NK_SDL_GL3_IMPLEMENTATION
#include "../third_party/nuklear/nuklear.h"
#include "nuklear/nuklear_sdl_gl3.h"
#endif

#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024
#define FULL_FONT_TEXTURE_HEIGHT 32768 //needed to load all CJK glyphs

class MultiplayerScreen;

class ChatOverlay: public IApplicationTickable
{
public:
	ChatOverlay(MultiplayerScreen* m) : m_multi(m), m_nctx(), m_eventQueue() {};
	~ChatOverlay();
	
	bool Init() override;
	void Tick(float deltaTime) override;
	void Render(float deltaTime) override;
	void NKRender();
	void UpdateNuklearInput(SDL_Event evt);
	void SendChatMessage(const String& message);
	void AddMessage(const String& message);
	void AddMessage(const String& message, int r, int g, int b);
	bool OnKeyPressedConsume(SDL_Scancode key);
	void OpenChat();
	void CloseChat();
    void ShutdownNuklear();
    void InitNuklearIfNeeded();
	bool IsOpen() {
		return m_isOpen;
	}
	void EnableOpeningChat() {
		m_canOpen = true;
	}
	void DisableOpeningChat() {
		m_canOpen = false;
		if (m_isOpen)
			CloseChat();
	}
private:
	bool m_handleChatReceived(nlohmann::json& packet);
	void m_drawWindow();
	void m_drawChatAlert();

    bool m_nuklearRunning = false;

	MultiplayerScreen* m_multi = NULL;
	struct nk_context* m_nctx = NULL;
	std::queue<SDL_Event> m_eventQueue;

	char m_chatDraft[512] = {0};
	bool m_isOpen = false;
	struct nk_scroll m_chatScroll = {0, 0};
	int m_newMessages = 0;
	bool m_inEdit = false;
	bool m_forceToBottom = false;
	bool m_focusText = false;
	Vector<std::pair<String,nk_color>> m_messages;
	bool m_canOpen = true;
};
