#pragma once
#include "ApplicationTickable.hpp"
#include "GameConfig.hpp"
#include "Application.hpp"
#include "Input.hpp"
#include "GameConfig.hpp"
#include "GuiUtils.hpp"

class MultiplayerScreen;

// TODO(itszn) inherit BasicNuklearGui to reduce duplciated code
class ChatOverlay: public BasicNuklearGui
{
public:
	ChatOverlay(MultiplayerScreen* m) : m_multi(m) {};
	~ChatOverlay();
	
	bool Init() override;
	void Tick(float deltaTime) override;
	void Render(float deltaTime) override;

	void UpdateNuklearInput(SDL_Event evt);
	void SendChatMessage(const String& message);
	void AddMessage(const String& message);
	void AddMessage(const String& message, int r, int g, int b);
	bool OnKeyPressedConsume(SDL_Scancode key);
	void OpenChat();
	void CloseChat();
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

	MultiplayerScreen* m_multi = NULL;

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
