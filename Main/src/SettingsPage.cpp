#include "stdafx.h"
#include "SettingsPage.hpp"

#include "Application.hpp"

constexpr static int NK_PROPERTY_DEFAULT = 0;

static inline void nk_sdl_text(nk_flags event)
{
	if (event & NK_EDIT_ACTIVATED)
	{
		SDL_StartTextInput();
	}
	if (event & NK_EDIT_DEACTIVATED)
	{
		SDL_StopTextInput();
	}
}

static inline int nk_get_property_state(struct nk_context* ctx, const char* name)
{
	if (!ctx || !ctx->current || !ctx->current->layout) return NK_PROPERTY_DEFAULT;
	struct nk_window* win = ctx->current;
	nk_hash hash = 0;
	if (name[0] == '#') {
		hash = nk_murmur_hash(name, (int)nk_strlen(name), win->property.seq++);
		name++; /* special number hash */
	}
	else hash = nk_murmur_hash(name, (int)nk_strlen(name), 42);

	if (win->property.active && hash == win->property.name)
		return win->property.state;
	return NK_PROPERTY_DEFAULT;
}

void SettingsPage::SettingTextData::Load()
{
	String str = LoadConfig();

	if (str.length() >= m_buffer.size())
	{
		Logf("A config has been cropped due to being too long (%d)", Logger::Severity::Error, m_len);
		m_len = static_cast<int>(m_buffer.size() - 1);
	}
	else
	{
		m_len = static_cast<int>(str.length());
	}

	std::memcpy(m_buffer.data(), str.data(), m_len + 1);
}

void SettingsPage::SettingTextData::Save()
{
	String str = String(m_buffer.data(), m_len);

	str.TrimBack('\n');
	str.TrimBack(' ');

	SaveConfig(str);
}

void SettingsPage::SettingTextData::Render(nk_context* nctx)
{
	nk_sdl_text(nk_edit_string(nctx, NK_EDIT_FIELD, m_buffer.data(), &m_len, static_cast<int>(m_buffer.size()), nk_filter_default));
}

void SettingsPage::SettingTextData::RenderPassword(nk_context* nctx)
{
	// Hack taken from https://github.com/vurtun/nuklear/blob/a9e5e7299c19b8a8831a07173211fa8752d0cc8c/demo/overview.c#L549
	const int old_len = m_len;

	std::array<char, BUFFER_SIZE> tokenBuffer;
	std::fill(tokenBuffer.begin(), tokenBuffer.begin() + m_len, '*');

	nk_sdl_text(nk_edit_string(nctx, NK_EDIT_FIELD, tokenBuffer.data(), &m_len, 1024, nk_filter_default));

	if (old_len < m_len)
	{
		std::memcpy(m_buffer.data() + old_len, tokenBuffer.data() + old_len, m_len - old_len);
	}
}

String SettingsPage::GameConfigTextData::LoadConfig()
{
	return g_gameConfig.GetString(m_key);
}

void SettingsPage::GameConfigTextData::SaveConfig(const String& value)
{
	g_gameConfig.Set(m_key, value);
}

void SettingsPage::LayoutRowDynamic(int num_columns, float height)
{
	nk_layout_row_dynamic(m_nctx, height, num_columns);
}

void SettingsPage::Separator(float height)
{
	LayoutRowDynamic(1, height);
	Label(" ", nk_text_alignment::NK_TEXT_CENTERED);
}

void SettingsPage::Label(const std::string_view& label, enum nk_text_alignment alignment)
{
	nk_label(m_nctx, label.data(), alignment);
}

bool SettingsPage::ToggleInput(bool val, const std::string_view& label)
{
	constexpr static int TRUE_VAL = 0;
	constexpr static int FALSE_VAL = 1;

	int val_i = val ? TRUE_VAL : FALSE_VAL;
	nk_checkbox_label(m_nctx, label.data(), &val_i);

	return val_i == TRUE_VAL;
}

bool SettingsPage::ToggleSetting(GameConfigKeys key, const std::string_view& label)
{
	const bool value = g_gameConfig.GetBool(key);
	const bool newValue = ToggleInput(value, label);

	if (newValue != value)
	{
		g_gameConfig.Set(key, newValue);
		return true;
	}
	else
	{
		return false;
	}
}

int SettingsPage::SelectionInput(int val, const Vector<const char*>& options, const std::string_view& label)
{
	assert(options.size() > 0);

	nk_label(m_nctx, label.data(), nk_text_alignment::NK_TEXT_LEFT);
	nk_combobox(m_nctx, const_cast<const char**>(options.data()), static_cast<int>(options.size()), &val, m_buttonHeight, m_comboBoxSize);

	return val;
}

bool SettingsPage::SelectionSetting(GameConfigKeys key, const Vector<const char*>& options, const std::string_view& label)
{
	assert(options.size() > 0);

	const int value = g_gameConfig.GetInt(key) % options.size();
	const int newValue = SelectionInput(value, options, label);

	if (newValue != value)
	{
		g_gameConfig.Set(key, newValue);
		return true;
	}

	return false;
}

bool SettingsPage::StringSelectionSetting(GameConfigKeys key, const Vector<String>& options, const std::string_view& label)
{
	String value = g_gameConfig.GetString(key);
	int selection = 0;

	const auto stringSearch = std::find(options.begin(), options.end(), value);
	if (stringSearch != options.end())
	{
		selection = static_cast<int>(stringSearch - options.begin());
	}

	Vector<const char*> displayData;
	for (const String& s : options)
	{
		displayData.Add(s.data());
	}

	const int newSelection = SelectionInput(selection, displayData, label);

	if (newSelection != selection) {
		g_gameConfig.Set(key, options[newSelection]);
		return true;
	}
	return false;
}
	
int SettingsPage::IntInput(int val, const std::string_view& label, int min, int max, int step, float perPixel)
{
	const int oldState = nk_get_property_state(m_nctx, label.data());
	val = nk_propertyi(m_nctx, label.data(), min, val, max, step, perPixel);
	const int newState = nk_get_property_state(m_nctx, label.data());

	if (oldState != newState) {
		if (newState == NK_PROPERTY_DEFAULT)
			SDL_StopTextInput();
		else
			SDL_StartTextInput();
	}

	return val;
}

bool SettingsPage::IntSetting(GameConfigKeys key, const std::string_view& label, int min, int max, int step, float perPixel)
{
	const int value = g_gameConfig.GetInt(key);
	const int newValue = IntInput(value, label, min, max, step, perPixel);
	if (newValue != value) {
		g_gameConfig.Set(key, newValue);
		return true;
	}
	return false;
}

float SettingsPage::FloatInput(float val, const std::string_view& label, float min, float max, float step, float perPixel)
{
	const int oldState = nk_get_property_state(m_nctx, label.data());
	val = nk_propertyf(m_nctx, label.data(), min, val, max, step, perPixel);
	const int newState = nk_get_property_state(m_nctx, label.data());

	if (oldState != newState) {
		if (newState == NK_PROPERTY_DEFAULT)
			SDL_StopTextInput();
		else
			SDL_StartTextInput();
	}

	return val;
}

bool SettingsPage::FloatSetting(GameConfigKeys key, const std::string_view& label, float min, float max, float step)
{
	float value = g_gameConfig.GetFloat(key);
	const float prevValue = value;

	// nuklear supports precision only up to 2 decimal places (wtf)
	if (step >= 0.01f)
	{
		float incPerPixel = step;
		if (incPerPixel >= step / 2) incPerPixel = step * Math::Round(incPerPixel / step);

		value = FloatInput(value, label.data(), min, max, step, incPerPixel);
	}
	else
	{
		nk_labelf(m_nctx, nk_text_alignment::NK_TEXT_LEFT, label.data(), value);
		nk_slider_float(m_nctx, min, &value, max, step);
	}

	if (value != prevValue) {
		g_gameConfig.Set(key, value);
		return true;
	}

	return false;
}

bool SettingsPage::PercentSetting(GameConfigKeys key, const std::string_view& label)
{
	float value = g_gameConfig.GetFloat(key);
	const float prevValue = value;

	nk_labelf(m_nctx, nk_text_alignment::NK_TEXT_LEFT, label.data(), value * 100);
	nk_slider_float(m_nctx, 0, &value, 1, 0.005f);

	if (value != prevValue)
	{
		g_gameConfig.Set(key, value);
		return true;
	}
	else
	{
		return false;
	}
}

Color SettingsPage::ColorInput(const Color& val, const std::string_view& label, bool& useHSV) {
	LayoutRowDynamic(1);
	nk_label(m_nctx, label.data(), nk_text_alignment::NK_TEXT_LEFT);
	nk_colorf nkCol = { val.x, val.y, val.z, val.w };

	if (nk_combo_begin_color(m_nctx, nk_rgb_cf(nkCol), nk_vec2(200, 400))) {
		enum color_mode { COL_RGB, COL_HSV };

		LayoutRowDynamic(1, 120);
		nkCol = nk_color_picker(m_nctx, nkCol, NK_RGBA);

		LayoutRowDynamic(2, 25);
		useHSV = nk_option_label(m_nctx, "RGB", useHSV ? 1 : 0) == 1;
		useHSV = nk_option_label(m_nctx, "HSV", useHSV ? 0 : 1) == 0;

		LayoutRowDynamic(1, 25);
		if (useHSV == false) {
			nkCol.r = FloatInput(nkCol.r, "#R:", 0,  1.0f, 0.01f, 0.005f);
			nkCol.g = FloatInput(nkCol.g, "#G:", 0, 1.0f, 0.01f, 0.005f);
			nkCol.b = FloatInput(nkCol.b, "#B:", 0, 1.0f, 0.01f, 0.005f);
			nkCol.a = FloatInput(nkCol.a, "#A:", 0, 1.0f, 0.01f, 0.005f);
		}
		else {
			std::array<float, 4> hsva;
			nk_colorf_hsva_fv(hsva.data(), nkCol);
			hsva[0] = FloatInput(hsva[0], "#H:", 0, 1.0f, 0.01f, 0.05f);
			hsva[1] = FloatInput(hsva[1], "#S:", 0, 1.0f, 0.01f, 0.05f);
			hsva[2] = FloatInput(hsva[2], "#V:", 0, 1.0f, 0.01f, 0.05f);
			hsva[3] = FloatInput(hsva[3], "#A:", 0, 1.0f, 0.01f, 0.05f);
			nkCol = nk_hsva_colorfv(hsva.data());
		}
		nk_combo_end(m_nctx);
	}

	LayoutRowDynamic(1);

	return Color(nkCol.r, nkCol.g, nkCol.b, nkCol.a);
}

void SettingsPage::Init()
{
	Load();
}

void SettingsPage::Exit()
{
	Save();
}

void SettingsPage::Render(const struct nk_rect& rect)
{
	m_comboBoxSize.x = rect.x - 30;

	m_nctx->style.window.padding.x = 6.0f;
	m_nctx->style.window.padding.y = 0.0f;

	if (nk_begin(m_nctx, m_name.data(), rect, 0))
	{
		LayoutRowDynamic(1, 60);
		nk_labelf(m_nctx, nk_text_alignment::NK_TEXT_CENTERED, "%s Settings", m_name.data());

		RenderContents();
		nk_end(m_nctx);
	}
}

class SettingsPage_Profile : public SettingsPage
{
public:
	SettingsPage_Profile(nk_context* nctx, bool& forceReload) : SettingsPage(nctx, "Profile"), m_forceReload(forceReload) {}

protected:
	bool& m_forceReload;

	void Load() override
	{
		m_currentProfile = g_gameConfig.GetString(GameConfigKeys::CurrentProfileName);

		m_profiles.clear();
		m_profiles.push_back("Main");

		Vector<FileInfo> profiles = Files::ScanFiles(Path::Absolute("profiles/"), "cfg", NULL);

		for (const auto& file : profiles)
		{
			String profileName = "";
			String unused = Path::RemoveLast(file.fullPath, &profileName);
			profileName = profileName.substr(0, profileName.length() - 4); // Remove .cfg
			m_profiles.push_back(profileName);
		}
	}

	void Save() override
	{
		String newProfile = g_gameConfig.GetString(GameConfigKeys::CurrentProfileName);
		if (newProfile == m_currentProfile)
		{
			return;
		}

		// Save old settings
		g_gameConfig.Set(GameConfigKeys::CurrentProfileName, m_currentProfile);
		g_application->ApplySettings();

		// Load new settings
		g_application->ReloadConfig(newProfile);
		m_forceReload = true;
	}

	Vector<String> m_profiles;
	String m_currentProfile;

	void RenderContents() override
	{
		LayoutRowDynamic(1);

		if (StringSelectionSetting(GameConfigKeys::CurrentProfileName, m_profiles, "Selected Profile:"))
		{
			Save();
			return;
		}
	}
};

bool SettingsPageCollection::Init()
{
	BasicNuklearGui::Init();
	InitPages();

	g_gameWindow->OnMousePressed.Add(this, &SettingsPageCollection::OnMousePressed);

	return true;
}

SettingsPageCollection::~SettingsPageCollection()
{
	for (auto& page : m_pages)
	{
		page->Exit();
	}

	g_application->ApplySettings();

	g_gameWindow->OnMousePressed.RemoveAll(this);
}

void SettingsPageCollection::Tick(float deltaTime)
{
	if (m_forceReload)
	{
		Reload();
		return;
	}

	if (m_enableSwitchPageOnHover)
	{
		ProcessTabHandleMouseHover(g_gameWindow->GetMousePos());
	}

	BasicNuklearGui::Tick(deltaTime);
}

void SettingsPageCollection::Render(float deltaTime)
{
	if (IsSuspended())
	{
		return;
	}

	NVGcontext* vg = g_application->GetVGContext();

	nvgBeginFrame(vg, static_cast<float>(g_resolution.x), static_cast<float>(g_resolution.y), 1.0f);
	RenderPages();

	BasicNuklearGui::NKRender();

	nvgEndFrame(vg);
	g_application->GetRenderQueueBase()->Process();
}

void SettingsPageCollection::OnKeyPressed(SDL_Scancode code)
{
	if (IsSuspended())
	{
		return;
	}

	if (code == SDL_SCANCODE_ESCAPE)
	{
		Exit();
	}
}

void SettingsPageCollection::Exit()
{
	for (auto& page : m_pages)
	{
		page->Exit();
	}

	g_gameWindow->OnAnyEvent.RemoveAll(this);

	g_input.Cleanup();
	g_input.Init(*g_gameWindow);

	g_application->RemoveTickable(this);
}

void SettingsPageCollection::Reload()
{
	m_forceReload = false;

	for (auto& page : m_pages)
	{
		page->Exit();
		page->Init();
	}
}

void SettingsPageCollection::InitPages()
{
	const String fontPath = Path::Normalize(Path::Absolute("fonts/settings/NotoSans-Regular.ttf"));
	Graphics::Font font = g_application->LoadFont(fontPath, true);

	m_pages.clear();
	m_pageNames.clear();

	AddPages(m_pages);
	m_pages.emplace_back(std::make_unique<SettingsPage_Profile>(m_nctx, m_forceReload));

	for (const auto& page : m_pages)
	{
		page->Init();
		m_pageNames.Add(font->CreateText(Utility::ConvertToWString(page->GetName()), PAGE_NAME_SIZE));
	}

	m_currPage = 0;

	m_exitText = font->CreateText(L"Exit", PAGE_NAME_SIZE);
}

void SettingsPageCollection::UpdatePageRegions()
{
	const float SETTINGS_DESIRED_CONTENTS_WIDTH = g_resolution.y / 1.4f;
	const float SETTINGS_DESIRED_HEADERS_WIDTH = 120.0f;

	const float SETTINGS_WIDTH = Math::Min(SETTINGS_DESIRED_CONTENTS_WIDTH + SETTINGS_DESIRED_HEADERS_WIDTH, g_resolution.x - 5.0f);
	const float SETTINGS_CONTENTS_WIDTH = Math::Max(SETTINGS_WIDTH * 0.75f, SETTINGS_WIDTH - SETTINGS_DESIRED_HEADERS_WIDTH);
	const float SETTINGS_HEADERS_WIDTH = SETTINGS_WIDTH - SETTINGS_CONTENTS_WIDTH;

	// Better to keep current layout if there's not enough space
	if (SETTINGS_CONTENTS_WIDTH < 10.0f || SETTINGS_HEADERS_WIDTH < 10.0f)
	{
		return;
	}

	const float SETTINGS_OFFSET_X = (g_resolution.x - SETTINGS_WIDTH) / 2;
	const float SETTINGS_CONTENTS_OFFSET_X = SETTINGS_OFFSET_X + SETTINGS_HEADERS_WIDTH;

	m_pageContentRegion = { SETTINGS_CONTENTS_OFFSET_X, 0, SETTINGS_CONTENTS_WIDTH, (float)g_resolution.y };

	m_pageButtonHeight = Math::Clamp(m_pageContentRegion.h / (2 + m_pages.size()), static_cast<float>(PAGE_NAME_SIZE), 40.0f);

	float pageHeaderSpacing = Math::Max(0.0f, static_cast<float>(g_resolution.y) - m_pageButtonHeight * (2 + m_pages.size()));

	const float PAGE_HEADER_SPACING_TOP = Math::Min(pageHeaderSpacing * 0.1f, m_pageButtonHeight * 0.5f);
	pageHeaderSpacing -= PAGE_HEADER_SPACING_TOP;

	const float PAGE_HEADER_SPACING_MIDDLE = Math::Min(pageHeaderSpacing * 0.2f, m_pageButtonHeight * 3.0f);
	pageHeaderSpacing -= PAGE_HEADER_SPACING_MIDDLE;

	m_pageHeaderRegion = { SETTINGS_OFFSET_X, PAGE_HEADER_SPACING_TOP, SETTINGS_HEADERS_WIDTH, PAGE_HEADER_SPACING_TOP + m_pageButtonHeight * m_pages.size() };

	const float PAGE_HEADER_BOTTOM = static_cast<float>(g_resolution.y) - Math::Max(0.0f, pageHeaderSpacing);

	m_profileButtonRegion = { SETTINGS_OFFSET_X, PAGE_HEADER_BOTTOM - m_pageButtonHeight*2, SETTINGS_HEADERS_WIDTH, m_pageButtonHeight };
	m_exitButtonRegion = { SETTINGS_OFFSET_X, PAGE_HEADER_BOTTOM - m_pageButtonHeight, SETTINGS_HEADERS_WIDTH, m_pageButtonHeight };
}

void SettingsPageCollection::RenderPages()
{
	UpdatePageRegions();
	RenderPageHeaders();
	RenderPageContents();
}

static inline bool HitCheck(const struct nk_rect& rect, const Vector2i& pos)
{
	return rect.x <= pos.x && pos.x < rect.x + rect.w && rect.y <= pos.y && pos.y < rect.y + rect.h;
}

static inline void RenderButton(NVGcontext* vg, const struct nk_rect& rect, Ref<TextRes> textRes, bool activated, const Vector2i& mousePos)
{
	// Draw the button
	nvgStrokeWidth(vg, 1.0f);
	nvgStrokeColor(vg, nvgRGB(255, 255, 255));

	struct NVGcolor bgColor = activated ? nvgRGB(60, 60, 60) : HitCheck(rect, mousePos) ? nvgRGB(128, 128, 128) : nvgRGB(15, 15, 15);

	nvgFillColor(vg, bgColor);

	nvgBeginPath(vg);
	nvgMoveTo(vg, rect.x + rect.w, rect.y);
	nvgLineTo(vg, rect.x, rect.y);
	nvgLineTo(vg, rect.x, rect.y + rect.h);
	nvgLineTo(vg, rect.x + rect.w, rect.y + rect.h);

	nvgStroke(vg);
	nvgFill(vg);

	// Draw the text
	MaterialParameterSet params;
	params.SetParameter("color", Color::White);

	float text_x = rect.x + (rect.w - textRes->size.x) / 2;
	float text_y = rect.y + (rect.h - textRes->size.y) / 2;

	Transform transform = Transform::Translation(Vector2(Math::Round(text_x), Math::Round(text_y)));

	g_application->GetRenderQueueBase()->Draw(transform, textRes, g_application->GetFontMaterial(), params);
}

void SettingsPageCollection::RenderPageHeaders()
{
	NVGcontext* vg = g_application->GetVGContext();
	const Vector2i mousePos = g_gameWindow->GetMousePos();

	for (size_t i = 0; i+1 < m_pages.size(); ++i)
	{
		const struct nk_rect rect = { m_pageHeaderRegion.x, m_pageHeaderRegion.y + i * m_pageButtonHeight, m_pageHeaderRegion.w, m_pageButtonHeight };

		RenderButton(vg, rect, m_pageNames[i], m_currPage == i, mousePos);
	}

	RenderButton(vg, m_profileButtonRegion, *(m_pageNames.rbegin()), false, mousePos);
	RenderButton(vg, m_exitButtonRegion, m_exitText, false, mousePos);
}

void SettingsPageCollection::RenderPageContents()
{
	if (m_currPage >= m_pages.size())
	{
		return;
	}

	m_pages[m_currPage]->Render(m_pageContentRegion);
}

// Returns whether v is above(+1), on(0), or below(-1) the line connecting p1 and p2.
int CompareLine(const Vector2i& p1, const Vector2& p2, const Vector2i& v)
{
	const float vv = (p2.x - p1.x) * (v.y - p1.y);
	const float lv = (p2.y - p1.y) * (v.x - p1.x);

	return vv < lv ? +1 : vv > lv ? -1 : 0;
}

void SettingsPageCollection::ProcessTabHandleMouseHover(const Vector2i& mousePos)
{
	int currInd = GetPageIndFromMousePos(mousePos);
	if (currInd == -1 || currInd == m_prevMouseInd)
	{
		m_prevMousePos = mousePos;
		m_prevMouseInd = currInd;
		return;
	}
	
	// The Amazon dropdown trick from https://bjk5.com/post/44698559168/breaking-down-amazons-mega-dropdown
	if (m_prevMousePos.x <= mousePos.x && mousePos.x <= m_pageContentRegion.x)
	{
		if (CompareLine(m_prevMousePos, { m_pageContentRegion.x, m_pageContentRegion.y }, mousePos) <= 0
			&& CompareLine(m_prevMousePos, { m_pageContentRegion.x, m_pageContentRegion.y + m_pageContentRegion.h}, mousePos) >= 0)
		{
			currInd = m_prevMouseInd;
		}
	}

	if (currInd >= 0 && currInd < m_pages.size())
	{
		m_currPage = currInd;
	}

	m_prevMousePos = mousePos;
	m_prevMouseInd = currInd;
}

void SettingsPageCollection::OnMousePressed(MouseButton button)
{
	if (IsSuspended())
	{
		return;
	}

	const Vector2i mousePos = g_gameWindow->GetMousePos();

	if (HitCheck(m_exitButtonRegion, mousePos))
	{
		Exit();
		return;
	}

	const int index = GetPageIndFromMousePos(mousePos);
	if (index < 0) return;

	m_currPage = index;
}

int SettingsPageCollection::GetPageIndFromMousePos(const Vector2i& mousePos) const
{
	if (HitCheck(m_profileButtonRegion, mousePos))
	{
		return static_cast<int>(m_pages.size()) - 1;
	}

	if (!HitCheck(m_pageHeaderRegion, mousePos)) return -1;
	
	const int index = static_cast<int>((mousePos.y - m_pageHeaderRegion.y) / m_pageButtonHeight);

	if (index < 0 || index >= m_pages.size())
	{
		return -1;
	}

	return index;
}