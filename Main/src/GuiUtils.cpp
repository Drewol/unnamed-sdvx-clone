#include "stdafx.h"
#include "GuiUtils.hpp"
#include "Application.hpp"

BasicNuklearGui::~BasicNuklearGui()
{
	ShutdownNuklear();
}

void BasicNuklearGui::UpdateNuklearInput(SDL_Event evt)
{
	if (!m_isOpen)
		return;
	m_eventQueue.push(evt);
}

void BasicNuklearGui::ShutdownNuklear()
{
    if (!m_nuklearRunning)
        return;

    g_gameWindow->OnAnyEvent.RemoveAll(this);
    nk_sdl_shutdown_keep_font();

	if (!g_gameConfig.GetBool(GameConfigKeys::KeepFontTexture)) {
		glDeleteTextures(1, &s_fontTexture);
		s_fontTexture = 0;
		s_hasFontTexture = false;
	}

    m_nuklearRunning = false;
}

bool BasicNuklearGui::Init()
{
	if (m_backgroundFrame)
	{
		m_fromTexture = TextureRes::CreateFromFrameBuffer(g_gl, g_resolution);
		m_bgMesh = MeshGenerators::Quad(g_gl,
			Vector2(0, static_cast<float>(g_resolution.y)),
			Vector2(static_cast<float>(g_resolution.x), static_cast<float>(-g_resolution.y)));
	}
    InitNuklearIfNeeded();
    return true;
}

void BasicNuklearGui::InitNuklearIfNeeded()
{
    if (m_nuklearRunning) {
        return;
	}
	m_nctx = nk_sdl_init((SDL_Window*)g_gameWindow->Handle());

	g_gameWindow->OnAnyEvent.Add(this, &BasicNuklearGui::UpdateNuklearInput);

	InitNuklearFontAtlas();

	m_nctx->style.text.color = nk_rgb(255, 255, 255);
	m_nctx->style.button.border_color = nk_rgb(0, 128, 255);
	m_nctx->style.button.padding = nk_vec2(5,5);
	m_nctx->style.button.rounding = 0;
	m_nctx->style.window.fixed_background = nk_style_item_color(nk_rgb(40, 40, 40));
	m_nctx->style.slider.bar_normal = nk_rgb(20, 20, 20);
	m_nctx->style.slider.bar_hover = nk_rgb(20, 20, 20);
	m_nctx->style.slider.bar_active = nk_rgb(20, 20, 20);

    m_nuklearRunning = true;
}

static void ExtendFontAtlas(struct nk_font_atlas* atlas, const std::string_view& fontPath, float pixelHeight, const nk_rune* ranges)
{
	struct nk_font_config cfg = nk_font_config(pixelHeight);
	cfg.merge_mode = nk_true;
	cfg.range = ranges;

	nk_font_atlas_add_from_file(atlas, fontPath.data(), pixelHeight, &cfg);
}

Mutex BasicNuklearGui::s_mutex;
nk_font_atlas* BasicNuklearGui::s_atlas = nullptr;
nk_font* BasicNuklearGui::s_font = nullptr;
GLuint BasicNuklearGui::s_fontTexture = 0;
bool BasicNuklearGui::s_hasFontTexture = false;

void BasicNuklearGui::StartFontInit()
{
	// This font should cover latin and cyrillic fonts.
	const String defaultFontPath = Path::Normalize(Path::Absolute("fonts/settings/NotoSans-Regular.ttf"));
	const float fontSize = 24.f;

	s_atlas = new nk_font_atlas();
	nk_atlas_font_stash_begin(s_atlas);

	struct nk_font* font = nk_font_atlas_add_from_file(s_atlas, defaultFontPath.data(), fontSize, 0);

	if (!g_gameConfig.GetBool(GameConfigKeys::LimitSettingsFont))
	{
		InitNuklearFontAtlasFallback(s_atlas, fontSize);
	}
	s_font = font;
}

int BasicNuklearGui::s_fontImageWidth = 0;
int BasicNuklearGui::s_fontImageHeight = 0;

void BasicNuklearGui::BakeFontWithLock()
{
	BasicNuklearGui::s_mutex.lock();
	BakeFont();
	BasicNuklearGui::s_mutex.unlock();
}

void BasicNuklearGui::BakeFont()
{
	if (s_atlas->pixel || s_hasFontTexture)
		return;
	usc_nk_bake_atlas(s_atlas, s_fontImageWidth, s_fontImageHeight);
}

void BasicNuklearGui::DestroyFont()
{
	if (s_hasFontTexture)
	{
		glDeleteTextures(1, &s_fontTexture);
		s_hasFontTexture = false;
	}
	if (s_atlas)
	{
		nk_font_atlas_clear(s_atlas);
		delete s_atlas;
		s_atlas = nullptr;
		s_font = nullptr;
	}
}

void BasicNuklearGui::InitNuklearFontAtlas()
{
	BasicNuklearGui::s_mutex.lock();
	if (s_atlas == nullptr)
	{
		StartFontInit();
	}

	assert(s_atlas);
	if (!s_hasFontTexture && s_atlas->pixel == nullptr)
	{
		// Our thread didn't work
		Log("Baking nuklear font on main thread", Logger::Severity::Warning);
		BakeFont();
	}

	if (!s_hasFontTexture)
	{
		// Also assigns the atlas to the current sdl
		s_fontTexture = usc_nk_sdl_generate_texture(s_atlas, s_atlas->pixel, s_fontImageWidth, s_fontImageHeight);
		s_hasFontTexture = true;
	} 
	else
	{
		usc_nk_sdl_use_atlas(s_atlas, s_fontTexture);
	}
	BasicNuklearGui::s_mutex.unlock();

	assert(s_font);
	nk_style_set_font(m_nctx, &s_font->handle);
}

void BasicNuklearGui::InitNuklearFontAtlasFallback(struct nk_font_atlas* atlas, float fontSize)
{
	const String cjkFontPath = Path::Normalize(Path::Absolute("fonts/settings/DroidSansFallback.ttf"));

	// Essentials
	constexpr int CJK_SIZE_SMALL = 1024;
	static const nk_rune cjk_ranges_small[] = {
		// CJK symbols and punctuation
		0x3000, 0x303F,
		// Hiragana
		0x3040, 0x309F,
		// Katakana
		0x30A0, 0x30FF,
		0x31F0, 0x31FF,
		// Fullwidth and halfwidth characters
		0xFF00, 0xFFEF,
		0
	};

	// Basically all BMP characters in the font file
	constexpr int CJK_SIZE_LARGE = 8192;
	static const nk_rune cjk_ranges_large[] = {
		0x0E3F, 0xFFFF,
		0
	};

	int maxTextureSize = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
	Logf("System max texture size: %d (cjk small: %d / large: %d)", Logger::Severity::Info,
		maxTextureSize, CJK_SIZE_SMALL, CJK_SIZE_LARGE);

	if (maxTextureSize >= CJK_SIZE_LARGE)
	{
		ExtendFontAtlas(atlas, cjkFontPath, fontSize, cjk_ranges_large);
	}
	else if (maxTextureSize >= CJK_SIZE_SMALL)
	{
		ExtendFontAtlas(atlas, cjkFontPath, fontSize, cjk_ranges_small);
	}
}

void BasicNuklearGui::Tick(float deltatime)
{
	nk_input_begin(m_nctx);
	while (!m_eventQueue.empty())
	{
		nk_sdl_handle_event(&m_eventQueue.front());
		m_eventQueue.pop();
	}
	nk_input_end(m_nctx);
}

void BasicNuklearGui::NKRender()
{
	nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
}

void BasicNuklearGui::Render(float deltatime)
{
	if (m_backgroundFrame)
	{
		auto rq = g_application->GetRenderQueueBase();
		Transform t;
		MaterialParameterSet params;
		params.SetParameter("mainTex", m_fromTexture);
		params.SetParameter("color", Vector4(1.0f));
		rq->Draw(t, m_bgMesh, g_application->GetGuiTexMaterial(), params);
	}
	
	g_application->ForceRender();
	NKRender();
}


void BasicWindow::Tick(float deltatime)
{
	BasicNuklearGui::Tick(deltatime);

	if (!m_isOpen) {
		Close();
	}
}

bool nk_edit_isfocused(struct nk_context *ctx)
{
	struct nk_window *win;
	if (!ctx || !ctx->current) return false;

	win = ctx->current;
	return win->edit.active;
}

void BasicWindow::EnableInputForEdit(int widgetWidth, int widgetHeight)
{
	bool isFocused = nk_edit_isfocused(m_nctx);
	if (!m_inEdit && isFocused)
	{
		SDL_StartTextInput();

		// XXX(itszn) idk if this works for ime input yet
		SDL_Rect boxrect;

		struct nk_vec2 textBoxPos = nk_widget_position(m_nctx);
		boxrect.x = static_cast<int>(textBoxPos.x);
		boxrect.y = static_cast<int>(textBoxPos.y);
		boxrect.w = widgetWidth;
		boxrect.h = widgetHeight;

		SDL_SetTextInputRect(&boxrect);
	}

	m_isFocused |= isFocused;
}

void BasicWindow::Render(float deltatime)
{
	m_isFocused = false;

	if (m_isOpen)
	{
		if (nk_begin(
			m_nctx,
			*m_name,
			nk_rect(
				static_cast<float>(g_resolution.x / 2 - m_width / 2),
				static_cast<float>(g_resolution.y / 2 - m_height / 2),
				static_cast<float>(m_width),
				static_cast<float>(m_height)),
			m_windowFlag))
		{
			// Window contents
			DrawWindow();
		}
		else
		{
			m_isOpen = false;
		}
		nk_end(m_nctx);
	}

	if (m_inEdit && !m_isFocused)
	{
		SDL_StopTextInput();
	}

	m_inEdit = m_isFocused;

	BasicNuklearGui::Render(deltatime);
}

void BasicWindow::Close()
{
	m_isOpen = false;
	OnClose();

	if (m_inEdit)
	{
		SDL_StopTextInput();
		m_inEdit = false;
	}

	g_application->RemoveTickable(this);
}

bool BasicWindow::OnKeyPressedConsume(SDL_Scancode code)
{
	if (m_inEdit)
		return true;

	return BasicNuklearGui::OnKeyPressedConsume(code);
}

bool BasicPrompt::Init()
{
	if (!BasicWindow::Init())
		return false;
	return true;
}

bool BasicPrompt::OnKeyPressedConsume(SDL_Scancode code)
{
	// XXX this actually doesn't trigger while doing SDL input :think:
	if (code == SDL_SCANCODE_ESCAPE && m_isOpen)
	{
		Close();
		return true;
	}

	if (code == SDL_SCANCODE_RETURN && m_isOpen)
	{
		m_submitted = true;
		Close();
		return true;
	}
	return BasicWindow::OnKeyPressedConsume(code);
}

void BasicPrompt::OnClose()
{
	OnResult.Call(m_submitted, m_submitted ? m_data : nullptr);
}

void nk_multiline_label(struct nk_context* ctx, const char* allText, nk_text_alignment ali, float width)
{
	const struct nk_user_font *font = ctx->style.font;

	const char* text = allText;
	const char* end = NULL;
	do
	{
		// Also split on new lines
		const char* next = strchr(text, '\n');

		end = next;
		if (end == NULL)
			end = text + strlen(text);

		// Try to wrap text if needed
		int textEnd = 0;
		int maxlen = 0;
		do
		{
			maxlen = static_cast<int>(end - text);
			textEnd = maxlen;

			// We are going to try to find the max length that fits
			while (
				font->width(font->userdata, font->height, text, textEnd) > width
				&& textEnd > 1)
			{
				// Decrease the size until it fits
				textEnd--;
			}

			// Draw that part of the text
			nk_text(ctx, text, textEnd, ali);

			// Truncate to rest of string
			text += textEnd;
		} while (textEnd < maxlen);

		text = next;
		if (text)
			text++;
	} while (text != NULL);
}

void BasicPrompt::DrawWindow()
{
	struct nk_vec2 start_pos = nk_widget_position(m_nctx);

	nk_layout_set_min_row_height(m_nctx, 20);
	nk_layout_row_dynamic(m_nctx, 20, 1);

	nk_multiline_label(m_nctx, *m_text, NK_TEXT_LEFT, static_cast<float>(m_width - 30));

	nk_layout_row_dynamic(m_nctx, 40, 1);

	if (m_forceFocus)
	{
		nk_edit_focus(m_nctx, NK_EDIT_ALWAYS_INSERT_MODE);
		// XXX(itszn) not ideal but works. For some reason calling only once stoped working
		//m_forceFocus = false;
	}
	EnableInputForEdit(100, 40);

	nk_edit_string_zero_terminated(m_nctx, NK_EDIT_FIELD, m_data, sizeof(m_data) - 1, nk_filter_default);

	nk_layout_row_dynamic(m_nctx, 40, 2);

	if (nk_button_label(m_nctx, "Cancel"))
	{
		m_isOpen = false;
	}

	if (nk_button_label(m_nctx, *m_submitText))
	{
		m_submitted = true;
		m_isOpen = false;
	}

	struct nk_vec2 end_pos = nk_widget_position(m_nctx);

	if (m_shrinkWindow) {
		nk_window_set_size(m_nctx, *m_name, nk_vec2(
			static_cast<float>(m_width),
			end_pos.y - start_pos.y + 60.0f
		));
		m_shrinkWindow = false;
	}
}
