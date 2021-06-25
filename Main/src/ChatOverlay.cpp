#include "stdafx.h"
#include "ChatOverlay.hpp"
#include "GuiUtils.hpp"

#include "MultiplayerScreen.hpp"

ChatOverlay::~ChatOverlay()
{
    ShutdownNuklear();
}


void ChatOverlay::UpdateNuklearInput(SDL_Event evt)
{
	if (!m_isOpen || m_forceToBottom)
		return;
	m_eventQueue.push(evt);
}

bool ChatOverlay::Init()
{
	m_backgroundFrame = false;

	BasicNuklearGui::Init();

	// Init the socket callbacks
	m_multi->GetTCP().SetTopicHandler("server.chat.received", this, &ChatOverlay::m_handleChatReceived);

	AddMessage("Note: This chat is currently not encrypted", 179, 73, 73); 
	return true;
}

void ChatOverlay::Tick(float deltatime)
{
	BasicNuklearGui::Tick(deltatime);

	if (m_isOpen && nk_window_find(m_nctx, "Multiplayer Chat") && 
			nk_window_is_closed(m_nctx, "Multiplayer Chat"))
	{
		CloseChat();
	}
}

void ChatOverlay::m_drawChatAlert()
{
	// Don't add alert if we can't open chat
	if (!m_canOpen)
		return;

	const int windowFlag = NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR;

	float w = Math::Min(g_resolution.y / 1.4, g_resolution.x - 5.0);
	float x = g_resolution.x / 2 - w / 2;

	if (!nk_begin(m_nctx, "Chat Alert", nk_rect(g_resolution.x - 250, 0/*g_resolution.y - 40*/, 250, 40), windowFlag))
	{
		return;
	}


	nk_layout_row_dynamic(m_nctx, 30, 1);

	if (m_newMessages > 0)
	{
		String s = Utility::Sprintf("Press F8 to chat (%u new)", m_newMessages);
		const char* cs = *s;
		nk_text_colored(m_nctx, cs, strlen(cs), NK_TEXT_CENTERED, nk_rgb(255, 175, 38));
	}
	else
	{
		const char* cs = "Press F8 to chat";
		nk_text_colored(m_nctx, cs, strlen(cs), NK_TEXT_CENTERED, nk_rgb(255,255,255));
	}

	nk_end(m_nctx);
}

void ChatOverlay::m_drawWindow()
{
	//const int windowFlag = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_CLOSABLE;
	const int windowFlag = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE ;

	float w = Math::Min(g_resolution.y / 1.4, g_resolution.x - 5.0);
	float x = g_resolution.x / 2 - w / 2;


	if (!nk_begin(m_nctx, "Multiplayer Chat", nk_rect(0, g_resolution.y - 400, g_resolution.x, 400), windowFlag))
	{
		return;
	}

	struct nk_vec2 chat_box_pos = nk_widget_position(m_nctx);

	nk_layout_set_min_row_height(m_nctx, 20);
	float chat_box_height = nk_window_get_height(m_nctx) - 110;
	float chat_box_width = nk_window_get_width(m_nctx) - 60;

	nk_layout_row_dynamic(m_nctx, chat_box_height, 1);

	if (nk_group_scrolled_begin(m_nctx, &m_chatScroll, "Chatbox", NK_WINDOW_BORDER)) {

		struct nk_vec2 scroll_start_pos = nk_widget_position(m_nctx);

		nk_layout_row_dynamic(m_nctx, 30, 1);

		const struct nk_user_font *font = m_nctx->style.font;

		for(auto v : m_messages) {
			const char* s = v.first.c_str();

			// Try to wrap text if needed
			int textEnd = 0;
			int maxlen = 0;
			do
			{
				maxlen = strlen(s);
				textEnd = maxlen;

				// We are going to try to find the max length that fits
				while(
					font->width(font->userdata, font->height, s, textEnd) > chat_box_width
					&& textEnd > 1)
				{
					// Decrease the size until it fits
					textEnd--;
				}

				// Draw that part of the text
				nk_text_colored(m_nctx, s, textEnd, NK_LEFT, v.second);

				// Render the rest if there is more
				s += textEnd;
			} while( textEnd < maxlen);
		}
		
		struct nk_vec2 scroll_end_pos = nk_widget_position(m_nctx);
		if (m_newMessages > 0 || m_forceToBottom)
		{
			// TODO this is broken atm
			float scroll_height = scroll_end_pos.y - scroll_start_pos.y;
			float bottom_rel = scroll_end_pos.y - chat_box_height - chat_box_pos.y;
			if (bottom_rel < 200 || m_forceToBottom)
			{
				m_chatScroll.y = scroll_height;
			}
			m_newMessages = 0;
			m_forceToBottom = false;
		}

		nk_group_scrolled_end(m_nctx);
	}


	nk_layout_row_dynamic(m_nctx, 40, 1);

	bool res = nk_input_is_key_pressed(&m_nctx->input, NK_KEY_TEXT_SELECT_ALL);


	if (m_focusText)
	{
		nk_edit_focus(m_nctx, NK_EDIT_ALWAYS_INSERT_MODE);
		m_focusText = false;
	}
	bool isFocused = nk_edit_isfocused(m_nctx);
	if (!m_inEdit && isFocused)
	{
		SDL_StartTextInput();

		SDL_Rect boxrect;

		struct nk_vec2 textBoxPos = nk_widget_position(m_nctx);
		boxrect.x = textBoxPos.x;
		boxrect.y = textBoxPos.y;
		boxrect.w = chat_box_width;
		boxrect.h = 40;

		SDL_SetTextInputRect(&boxrect);
	}
	if (m_inEdit && !isFocused)
	{
		SDL_StopTextInput();
	}

	m_inEdit = isFocused;

	nk_flags event = nk_edit_string_zero_terminated(m_nctx, NK_EDIT_FIELD, m_chatDraft, sizeof(m_chatDraft)-1, nk_filter_default);

	nk_end(m_nctx);
}

void ChatOverlay::Render(float deltatime)
{
	float w = Math::Min(g_resolution.y / 1.4, g_resolution.x - 5.0);
	float x = g_resolution.x / 2 - w / 2;


	if (m_isOpen)
	{
		m_drawWindow();
	}
	else
	{
		m_drawChatAlert();
	}

	BasicNuklearGui::Render(deltatime);
}

void ChatOverlay::CloseChat()
{
	m_isOpen = false;
	if (m_inEdit)
	{
		SDL_StopTextInput();
		m_inEdit = false;
	}
	m_forceToBottom = false;
	m_focusText = false;
}

void ChatOverlay::OpenChat()
{
	m_isOpen = true;
	m_forceToBottom = true;
	m_focusText = true;
}

bool ChatOverlay::OnKeyPressedConsume(SDL_Scancode code)
{
	if (code == SDL_SCANCODE_ESCAPE && m_isOpen)
	{
		CloseChat();
		return true;
	}

	if (code == SDL_SCANCODE_RETURN && m_isOpen)
	{
		// Send message if there is one
		if (strlen(m_chatDraft) > 0) {
			SendChatMessage(m_chatDraft);
			memset(m_chatDraft, 0, sizeof(m_chatDraft));
		}
		return true;
	}


	if (code == SDL_SCANCODE_F8 && m_canOpen)
	{
		// Toggle open
		if (m_isOpen)
			CloseChat();
		else
			OpenChat();
		return true;
	}

	if (m_inEdit)
		return true;

	return m_isOpen;

}

void ChatOverlay::SendChatMessage(const String& message)
{
	nlohmann::json packet;
	if (m_multi->InRoom())
		packet["topic"] = "room.chat.send";
	else 
		packet["topic"] = "server.chat.send";

	packet["message"] = message;
	m_multi->GetTCP().SendJSON(packet);

	time_t t = time(NULL);
	struct tm ttm = * localtime(&t);
	String out = Utility::Sprintf("%02u:%02u [%s] %s", ttm.tm_hour, ttm.tm_min, m_multi->GetUserName(), message);


	m_forceToBottom = true;
	AddMessage(out, 200, 200, 200);
}

bool ChatOverlay::m_handleChatReceived(nlohmann::json& packet)
{
	String message;
	packet["message"].get_to(message);

	time_t t = time(NULL);
	struct tm ttm = * localtime(&t);

	String out = Utility::Sprintf("%02u:%02u %s", ttm.tm_hour, ttm.tm_min, message);
	m_newMessages++;
	AddMessage(out);
	return true;
}

void ChatOverlay::AddMessage(const String& message, int r, int g, int b)
{
	m_messages.push_back(std::make_pair(message, nk_rgb(r,g,b)));
}

void ChatOverlay::AddMessage(const String& message)
{
	m_messages.push_back(std::make_pair(message, nk_rgb(256, 256, 256)));
}
