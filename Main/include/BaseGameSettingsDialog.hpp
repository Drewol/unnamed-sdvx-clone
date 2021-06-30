#pragma once

#include "GameConfig.hpp"
#include "Input.hpp"

enum class SettingType
{
    Integer,
    Floating,
    Boolean,
    Enum,
    Button,
};

// Base class for popup dialog for game settings
class BaseGameSettingsDialog
{
public:
    typedef struct SettingData
    {
        SettingData(const String& name, SettingType type) : name(name), type(type) {}

        String name;
        SettingType type;

        // Called to update setting
        Delegate<SettingData&> getter;
        // Called when the setting is updated
        Delegate<const SettingData&> setter;

        struct
        {
            float val;
            float min;
            float max;
            float mult;
        } floatSetting;

        struct
        {
            int val;
            int min;
            int max;
            int step = 1;
            int div = 1; // For decimal values
        } intSetting;

        struct
        {
            int val;
            Map<uint32, int> enumToVal;
            Vector<String> options;
        } enumSetting;

        struct
        {
            bool val;
        } boolSetting;

    } SettingData;
    typedef std::unique_ptr<SettingData> Setting;

    typedef struct TabData
    {
        String name;
        Vector<Setting> settings;

        void SetLua(struct lua_State* lua);
    } TabData;
    typedef std::unique_ptr<TabData> Tab;

public:
    ~BaseGameSettingsDialog();

    void Tick(float deltaTime);
    void Render(float deltaTime);
    bool Init();
    bool IsSelectionOnPressable();

    inline void AddTab(Tab tab) { m_tabs.emplace_back(std::move(tab)); }

    inline bool IsActive() const { return m_active; }
    inline bool IsInitialized() const { return m_isInitialized; }

    inline void Open() { assert(!m_active); m_targetActive = true; }
    inline void Close() { assert(m_active); m_targetActive = false; }

    void ResetTabs();

    Delegate<> onClose;

protected:
    virtual void InitTabs() = 0;
    virtual void OnAdvanceTab() {};

    Setting CreateBoolSetting(GameConfigKeys key, String name);
    Setting CreateIntSetting(GameConfigKeys key, String name, Vector2i range, int step = 1);
    Setting CreateFloatSetting(GameConfigKeys key, String name, Vector2 range, float mult = 1.0f);

    Setting CreateBoolSetting(String label, bool& val);
    Setting CreateIntSetting(String label, int& val, Vector2i range, int step = 1);
    Setting CreateButton(String label, std::function<void(const BaseGameSettingsDialog::SettingData&)>&& callback);

    template <typename EnumClass>
    Setting CreateEnumSetting(GameConfigKeys key, String name);

    inline int GetCurrentTab() const { return m_currentTab; }
    inline void SetCurrentTab(int tabIndex) { m_currentTab = tabIndex; }

    Vector2 m_pos = { 0.5f, 0.5f };

private:
    void m_SetTables();
    void m_AdvanceSelection(int steps);
    void m_AdvanceTab(int steps);
    void m_ChangeStepSetting(int steps); //int, enum, toggle, are all advanced in distinct steps
    void m_PressSetting();
    void m_OnButtonPressed(Input::Button button);
    void m_OnButtonReleased(Input::Button button);
    void m_OnKeyPressed(SDL_Scancode code);
    void m_ResetTabs();

    // Set a target in open/close and apply it in the next tick because stuff
    bool m_targetActive = false;
    bool m_active = false;
    bool m_closing = false;
    bool m_isInitialized = false;

    bool m_enableFXInputs = false;

    bool m_needsToResetTabs = false;

    int m_currentTab = 0;
    int m_currentSetting = 0;

    float m_knobAdvance[2] = {0.0f, 0.0f};
    float m_sensMult = 1.0f;

    struct lua_State* m_lua = nullptr;
    Vector<Tab> m_tabs;
};

template <typename EnumClass>
BaseGameSettingsDialog::Setting BaseGameSettingsDialog::CreateEnumSetting(GameConfigKeys key, String name)
{
    Setting s = std::make_unique<SettingData>(name, SettingType::Enum);

    int ind = 0;

    EnumStringMap<typename EnumClass::EnumType> nameMap = EnumClass::GetMap();
    for (auto& it : nameMap)
    {
        s->enumSetting.options.Add(it.second);
        s->enumSetting.enumToVal.Add(static_cast<int>(it.first), ind++);
    }

    s->enumSetting.val = s->enumSetting.enumToVal[static_cast<uint32>(g_gameConfig.GetEnum<EnumClass>(key))];

    auto getter = [key](SettingData& data) {
        data.enumSetting.val = data.enumSetting.enumToVal[static_cast<uint32>(g_gameConfig.GetEnum<EnumClass>(key))];
    };

    auto setter = [key](const SettingData& data) {
        g_gameConfig.SetEnum<EnumClass>(key, EnumClass::FromString(data.enumSetting.options[data.enumSetting.val]));
    };

    s->getter.AddLambda(std::move(getter));
    s->setter.AddLambda(std::move(setter));

    return s;
}