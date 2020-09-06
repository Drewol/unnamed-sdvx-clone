#include "stdafx.h"
#include "GameplaySettingsDialog.hpp"
#include "HitStat.hpp"
#include "SongSelect.hpp"

GameplaySettingsDialog::GameplaySettingsDialog(SongSelect* songSelectScreen)
    : songSelectScreen(songSelectScreen)
{
}

void GameplaySettingsDialog::InitTabs()
{
    Tab offsetTab = std::make_unique<TabData>();
    offsetTab->name = "Offsets";
    offsetTab->settings.push_back(CreateIntSetting(GameConfigKeys::GlobalOffset, "Global Offset", {-200, 200}));
    offsetTab->settings.push_back(CreateIntSetting(GameConfigKeys::InputOffset, "Input Offset", {-200, 200}));
    offsetTab->settings.push_back(m_CreateSongOffsetSetting());

    Tab speedTab = std::make_unique<TabData>();
    speedTab->name = "HiSpeed";
    speedTab->settings.push_back(CreateEnumSetting<Enum_SpeedMods>(GameConfigKeys::SpeedMod, "Speed Mod"));
    speedTab->settings.push_back(CreateFloatSetting(GameConfigKeys::HiSpeed, "HiSpeed", {0.1f, 16.f}));
    speedTab->settings.push_back(CreateFloatSetting(GameConfigKeys::ModSpeed, "ModSpeed", {50, 1500}, 20.0f));

    Tab gameTab = std::make_unique<TabData>();
    gameTab->name = "Game";
    gameTab->settings.push_back(CreateEnumSetting<Enum_GaugeTypes>(GameConfigKeys::GaugeType, "Gauge"));
    gameTab->settings.push_back(CreateBoolSetting(GameConfigKeys::RandomizeChart, "Random"));
    gameTab->settings.push_back(CreateBoolSetting(GameConfigKeys::MirrorChart, "Mirror"));
    gameTab->settings.push_back(CreateBoolSetting(GameConfigKeys::DisableBackgrounds, "Hide Backgrounds"));
    gameTab->settings.push_back(CreateEnumSetting<Enum_ScoreDisplayModes>(GameConfigKeys::ScoreDisplayMode, "Score Display"));
    gameTab->settings.push_back(CreateButton("Autoplay", [this](const auto&) { onPressAutoplay.Call(); }));
    gameTab->settings.push_back(CreateButton("Practice", [this](const auto&) { onPressPractice.Call(); }));

    Tab hidsudTab = std::make_unique<TabData>();
    hidsudTab->name = "Hid/Sud";
    hidsudTab->settings.push_back(CreateBoolSetting(GameConfigKeys::EnableHiddenSudden, "Enable Hidden / Sudden"));
    hidsudTab->settings.push_back(CreateFloatSetting(GameConfigKeys::HiddenCutoff, "Hidden Cutoff", { 0.f, 1.f }));
    hidsudTab->settings.push_back(CreateFloatSetting(GameConfigKeys::HiddenFade, "Hidden Fade", { 0.f, 1.f }));
    hidsudTab->settings.push_back(CreateFloatSetting(GameConfigKeys::SuddenCutoff, "Sudden Cutoff", { 0.f, 1.f }));
    hidsudTab->settings.push_back(CreateFloatSetting(GameConfigKeys::SuddenFade, "Sudden Fade", { 0.f, 1.f }));
    hidsudTab->settings.push_back(CreateBoolSetting(GameConfigKeys::ShowCover, "Show Track Cover"));

    Tab judgeWindowTab = std::make_unique<TabData>();
    judgeWindowTab->name = "Judgement";
    judgeWindowTab->settings.push_back(CreateIntSetting(GameConfigKeys::HitWindowPerfect, "Crit Window", {0, HitWindow::NORMAL.perfect}));
    judgeWindowTab->settings.push_back(CreateIntSetting(GameConfigKeys::HitWindowGood, "Near Window", { 0, HitWindow::NORMAL.good }));
    judgeWindowTab->settings.push_back(CreateIntSetting(GameConfigKeys::HitWindowHold, "Hold Window", { 0, HitWindow::NORMAL.hold }));
    judgeWindowTab->settings.push_back(CreateButton("Set to NORMAL", [this](const auto&) { HitWindow::NORMAL.SaveConfig(); }));
    judgeWindowTab->settings.push_back(CreateButton("Set to HARD", [this](const auto&) { HitWindow::HARD.SaveConfig(); }));

    AddTab(std::move(offsetTab));
    AddTab(std::move(speedTab));
    AddTab(std::move(gameTab));
    AddTab(std::move(hidsudTab));
    AddTab(std::move(judgeWindowTab));

    SetCurrentTab(g_gameConfig.GetInt(GameConfigKeys::GameplaySettingsDialogLastTab));
}

void GameplaySettingsDialog::OnAdvanceTab()
{
    g_gameConfig.Set(GameConfigKeys::GameplaySettingsDialogLastTab, GetCurrentTab());
}

GameplaySettingsDialog::Setting GameplaySettingsDialog::m_CreateSongOffsetSetting()
{
    Setting songOffsetSetting = std::make_unique<SettingData>("Song Offset", SettingType::Integer);
    songOffsetSetting->name = "Song Offset";
    songOffsetSetting->type = SettingType::Integer;
    songOffsetSetting->intSetting.val = 0;
    songOffsetSetting->intSetting.min = -200;
    songOffsetSetting->intSetting.max = 200;
    songOffsetSetting->setter.AddLambda([this](const auto& data) { onSongOffsetChange.Call(data.intSetting.val); });
    songOffsetSetting->getter.AddLambda([this](auto& data) {
        if (const ChartIndex* chart = songSelectScreen->GetCurrentSelectedChart())
        {
            data.intSetting.val = chart->custom_offset;
        }
    });

    return songOffsetSetting;
}
