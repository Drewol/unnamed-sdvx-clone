#include "stdafx.h"
#include "PracticeModeSettingsDialog.hpp"
#include "Beatmap/MapDatabase.hpp"

PracticeModeSettingsDialog::PracticeModeSettingsDialog(Game& game, MapTime& lastMapTime,
    int32& tempOffset, Game::PlayOptions& playOptions, MapTimeRange& range)
    : m_chartIndex(game.GetChartIndex()), m_beatmap(game.GetBeatmap()),
    m_endTime(m_beatmap->GetLastObjectTime()),  m_lastMapTime(lastMapTime),
    m_tempOffset(tempOffset), m_playOptions(playOptions), m_range(range)
{
    m_pos = { 0.75f, 0.75f };
}

void PracticeModeSettingsDialog::InitTabs()
{
    AddTab(m_CreateMainSettingTab());
    AddTab(m_CreateLoopingTab());
    AddTab(m_CreateLoopControlTab());
    AddTab(m_CreateFailConditionTab());
    AddTab(m_CreateGameSettingTab());

    SetCurrentTab(0);
}

PracticeModeSettingsDialog::Tab PracticeModeSettingsDialog::m_CreateMainSettingTab()
{
    Tab mainSettingTab = std::make_unique<TabData>();
    mainSettingTab->name = "Main";

    Setting loopBeginButton = CreateButton("Set start point (0ms) to here", [this](const auto&) {
        m_SetStartTime(Math::Clamp(m_lastMapTime, 0, m_endTime));
    });
    m_setStartButton = loopBeginButton.get();
    mainSettingTab->settings.emplace_back(std::move(loopBeginButton));

    Setting loopEndButton = CreateButton("Set end point (0ms) to here", [this](const auto&) {
        m_SetEndTime(m_lastMapTime);
    });
    m_setEndButton = loopEndButton.get();
    mainSettingTab->settings.emplace_back(std::move(loopEndButton));

    Setting loopOnSuccess = CreateBoolSetting("Loop on success", m_playOptions.loopOnSuccess);
    mainSettingTab->settings.emplace_back(std::move(loopOnSuccess));

    Setting loopOnFail = CreateBoolSetting("Loop on fail", m_playOptions.loopOnFail);
    mainSettingTab->settings.emplace_back(std::move(loopOnFail));

    Setting enableNavSetting = CreateBoolSetting(GameConfigKeys::PracticeSetupNavEnabled, "Enable navigation inputs for the setup");
    enableNavSetting->setter.Clear();
    enableNavSetting->setter.AddLambda([this](const SettingData& data) {
        g_gameConfig.Set(GameConfigKeys::PracticeSetupNavEnabled, data.boolSetting.val);
        onSettingChange.Call();
    });
    mainSettingTab->settings.emplace_back(std::move(enableNavSetting));

    Setting speedSetting = std::make_unique<SettingData>("Playback speed (%)", SettingType::Integer);
    speedSetting->intSetting.min = 25;
    speedSetting->intSetting.max = 100;
    speedSetting->intSetting.val = Math::RoundToInt(m_playOptions.playbackSpeed * 100);
    speedSetting->setter.AddLambda([this](const SettingData& data) { onSpeedChange.Call(data.intSetting.val == 100 ? 1.0f : data.intSetting.val / 100.0f); });
    speedSetting->getter.AddLambda([this](SettingData& data) { data.intSetting.val = Math::RoundToInt(m_playOptions.playbackSpeed * 100); });
    mainSettingTab->settings.emplace_back(std::move(speedSetting));

    mainSettingTab->settings.emplace_back(CreateButton("Start practice", [this](const auto&) { onPressStart.Call(); }));
    mainSettingTab->settings.emplace_back(CreateButton("Exit", [this](const auto&) { onPressExit.Call(); }));

    return mainSettingTab;
}

void PracticeModeSettingsDialog::m_SetStartTime(MapTime time, int measure)
{
    m_range.begin = time;
    m_startMeasure = measure >= 0 ? measure : m_TimeToMeasure(time);
    m_setStartButton->name = Utility::Sprintf("Set the start point (%dms) to here", time);
    onSetMapTime.Call(time);
    if (m_range.end < time)
    {
        m_range.end = time;
        m_endMeasure = m_startMeasure;
    }
}

void PracticeModeSettingsDialog::m_SetEndTime(MapTime time, int measure)
{
    m_range.end = time;
    m_endMeasure = measure >= 0 ? measure : m_TimeToMeasure(time);
    m_setEndButton->name = Utility::Sprintf("Set the end point (%dms) to here", time);

    if(time != 0) onSetMapTime.Call(time);
}


PracticeModeSettingsDialog::Tab PracticeModeSettingsDialog::m_CreateLoopingTab()
{
    Tab loopingTab = std::make_unique<TabData>();
    loopingTab->name = "Looping";

    // Loop begin
    {
        m_SetStartTime(m_range.begin);

        Setting loopBeginButton = CreateButton("Set the start point to here", [this](const auto&) {
            m_SetStartTime(Math::Clamp(m_lastMapTime, 0, m_endTime));
            });
        loopingTab->settings.emplace_back(std::move(loopBeginButton));

        Setting loopBeginMeasureSetting = CreateIntSetting("- in measure no.", m_startMeasure, {1, m_TimeToMeasure(m_endTime)});
        loopBeginMeasureSetting->setter.AddLambda([this](const SettingData& data) {
            m_SetStartTime(m_MeasureToTime(data.intSetting.val), data.intSetting.val);
        });
        loopingTab->settings.emplace_back(std::move(loopBeginMeasureSetting));

        Setting loopBeginMSSetting = CreateIntSetting("- in milliseconds", m_range.begin, {0, m_endTime}, 50);
        loopBeginMSSetting->setter.AddLambda([this](const SettingData& data) {
            m_SetStartTime(data.intSetting.val);
        });
        loopingTab->settings.emplace_back(std::move(loopBeginMSSetting));
    }

    // Loop end
    {
        m_SetEndTime(m_range.end);

        Setting loopEndButton = CreateButton("Set the end point to here", [this](const auto&) {
            m_SetEndTime(m_lastMapTime);
            });
        loopingTab->settings.emplace_back(std::move(loopEndButton));

        Setting loopEndMeasureSetting = CreateIntSetting("- in measure no.", m_endMeasure, {1, m_TimeToMeasure(m_endTime)});
        loopEndMeasureSetting->setter.AddLambda([this](const SettingData& data) {
            m_SetEndTime(m_MeasureToTime(data.intSetting.val), data.intSetting.val);
        });
        loopingTab->settings.emplace_back(std::move(loopEndMeasureSetting));

        Setting loopEndMSSetting = CreateIntSetting("- in milliseconds", m_range.end, {0, m_endTime}, 50);
        loopEndMSSetting->setter.AddLambda([this](const SettingData& data) {
            m_SetEndTime(data.intSetting.val);
        });
        loopingTab->settings.emplace_back(std::move(loopEndMSSetting));
    }
    
    // Clearing
    {   
        Setting loopStartClearButton = CreateButton("Clear the start point", [this](const auto&) {
            m_SetStartTime(0);
        });
        loopingTab->settings.emplace_back(std::move(loopStartClearButton));
        
        Setting loopEndClearButton = CreateButton("Clear the end point", [this](const auto&) {
            m_SetEndTime(0);
        });
        loopingTab->settings.emplace_back(std::move(loopEndClearButton));
    }

    return loopingTab;
}

PracticeModeSettingsDialog::Tab PracticeModeSettingsDialog::m_CreateLoopControlTab()
{
    Tab loopControlTab = std::make_unique<TabData>();
    loopControlTab->name = "LoopControl";

    {
        Setting loopOnSuccess = CreateBoolSetting("Loop on success", m_playOptions.loopOnSuccess);
        loopControlTab->settings.emplace_back(std::move(loopOnSuccess));

        Setting loopOnFail = CreateBoolSetting("Loop on fail", m_playOptions.loopOnFail);
        loopControlTab->settings.emplace_back(std::move(loopOnFail));
    }

    {
        Setting incSpeedOnSuccess = CreateBoolSetting("Increase speed on success", m_playOptions.incSpeedOnSuccess);
        incSpeedOnSuccess->setter.AddLambda([this](const SettingData& data) { if(data.boolSetting.val) m_playOptions.loopOnSuccess = true; });
        loopControlTab->settings.emplace_back(std::move(incSpeedOnSuccess));

        Setting incSpeedAmount = std::make_unique<SettingData>("- increment (%p)", SettingType::Integer);
        incSpeedAmount->intSetting.min = 1;
        incSpeedAmount->intSetting.max = 10;
        incSpeedAmount->intSetting.val = Math::RoundToInt(m_playOptions.incSpeedAmount * 100);
        incSpeedAmount->setter.AddLambda([this](const SettingData& data) {
            m_playOptions.incSpeedOnSuccess = m_playOptions.loopOnSuccess = true;
            m_playOptions.incSpeedAmount = data.intSetting.val / 100.0f;
        });
        incSpeedAmount->getter.AddLambda([this](SettingData& data) {data.intSetting.val = Math::RoundToInt(m_playOptions.incSpeedAmount * 100); });
        loopControlTab->settings.emplace_back(std::move(incSpeedAmount));

        Setting incStreak = CreateIntSetting("- required streakes", m_playOptions.incStreak, { 1, 10 });
        incStreak->setter.AddLambda([this](const SettingData&) { m_playOptions.incSpeedOnSuccess = m_playOptions.loopOnSuccess = true; });
        loopControlTab->settings.emplace_back(std::move(incStreak));
    }

    {
        Setting decSpeedOnFail = CreateBoolSetting("Decrease speed on fail", m_playOptions.decSpeedOnFail);
        decSpeedOnFail->setter.AddLambda([this](const SettingData& data) { if (data.boolSetting.val) m_playOptions.loopOnFail = true; });
        loopControlTab->settings.emplace_back(std::move(decSpeedOnFail));

        Setting decSpeedAmount = std::make_unique<SettingData>("- decrement (%p)", SettingType::Integer);
        decSpeedAmount->intSetting.min = 1;
        decSpeedAmount->intSetting.max = 10;
        decSpeedAmount->intSetting.val = Math::RoundToInt(m_playOptions.decSpeedAmount * 100);
        decSpeedAmount->setter.AddLambda([this](const SettingData& data) {
            m_playOptions.decSpeedOnFail = true;
            m_playOptions.loopOnFail = true;
            m_playOptions.decSpeedAmount = data.intSetting.val / 100.0f;
        });
        decSpeedAmount->getter.AddLambda([this](SettingData& data) { data.intSetting.val = Math::RoundToInt(m_playOptions.decSpeedAmount * 100); });
        loopControlTab->settings.emplace_back(std::move(decSpeedAmount));

        Setting minSpeed = std::make_unique<SettingData>("- minimum speed (%)", SettingType::Integer);
        minSpeed->intSetting.min = 1;
        minSpeed->intSetting.max = 10;
        minSpeed->intSetting.val = Math::RoundToInt(m_playOptions.minPlaybackSpeed * 100);
        minSpeed->setter.AddLambda([this](const SettingData& data) {
            m_playOptions.minPlaybackSpeed = data.intSetting.val / 100.0f;
        });
        minSpeed->getter.AddLambda([this](SettingData& data) { data.intSetting.val = Math::RoundToInt(m_playOptions.minPlaybackSpeed * 100); });
        loopControlTab->settings.emplace_back(std::move(minSpeed));
    }

    {
        Setting maxRewindMeasure = CreateBoolSetting("Set maximum amount of rewinding on fail", m_playOptions.enableMaxRewind);
        maxRewindMeasure->setter.AddLambda([this](const SettingData& data) { if (data.boolSetting.val) m_playOptions.loopOnFail = true; });
        loopControlTab->settings.emplace_back(std::move(maxRewindMeasure));

        Setting rewindMeasure = CreateIntSetting("- amount in # of measures", m_playOptions.maxRewindMeasure, { 0, 20 });
        loopControlTab->settings.emplace_back(std::move(rewindMeasure));
    }

    return loopControlTab;
}

std::unique_ptr<GameFailCondition> PracticeModeSettingsDialog::m_CreateGameFailCondition(GameFailCondition::Type type)
{
    switch (type)
    {
    case GameFailCondition::Type::Score: return std::make_unique<GameFailCondition::Score>(m_condScore);
    case GameFailCondition::Type::Grade: return std::make_unique<GameFailCondition::Grade>(m_condGrade);
    case GameFailCondition::Type::Miss: return std::make_unique<GameFailCondition::MissCount>(m_condMiss);
    case GameFailCondition::Type::MissAndNear: return std::make_unique<GameFailCondition::MissAndNearCount>(m_condMissNear);
    case GameFailCondition::Type::Gauge: return std::make_unique<GameFailCondition::Gauge>(m_condGauge);
    default: return nullptr;
    }
}

PracticeModeSettingsDialog::Tab PracticeModeSettingsDialog::m_CreateFailConditionTab()
{
    Tab conditionTab = std::make_unique<TabData>();
    conditionTab->name = "Mission";

    Setting conditionType = std::make_unique<SettingData>("Fail condition", SettingType::Enum);
    for (const char* str : GameFailCondition::TYPE_STR)
        conditionType->enumSetting.options.Add(str);
    conditionType->enumSetting.val = static_cast<int>(GameFailCondition::Type::None);
    conditionType->getter.AddLambda([this](SettingData& data) {
        GameFailCondition::Type type = m_playOptions.failCondition ? m_playOptions.failCondition.get()->GetType() : GameFailCondition::Type::None;
        data.enumSetting.val = static_cast<int>(type);
    });
    conditionType->setter.AddLambda([this](const SettingData& data) {
        m_playOptions.failCondition = m_CreateGameFailCondition(static_cast<GameFailCondition::Type>(data.enumSetting.val));
    });
    conditionTab->settings.emplace_back(std::move(conditionType));

    Setting scoreCondition = CreateIntSetting("Score less than", m_condScore, { 800 * 10000, 1000 * 10000 }, 10000);
    scoreCondition->getter.AddLambda([this](SettingData& data) {
        data.intSetting.val = m_condScore;
    });
    scoreCondition->setter.AddLambda([this](const SettingData& data) {
        m_playOptions.failCondition = std::make_unique<GameFailCondition::Score>(data.intSetting.val);
    });
    conditionTab->settings.emplace_back(std::move(scoreCondition));

    Setting gradeCondition = std::make_unique<SettingData>("Grade less than", SettingType::Enum);
    for (const char* str : GRADE_MARK_STR)
        gradeCondition->enumSetting.options.Add(str);
    gradeCondition->enumSetting.val = static_cast<int>(m_condGrade);
    gradeCondition->getter.AddLambda([this](SettingData& data) {
        data.enumSetting.val = static_cast<int>(m_condGrade);
    });
    gradeCondition->setter.AddLambda([this](const SettingData& data) {
        m_condGrade = static_cast<GradeMark>(data.enumSetting.val);
        m_playOptions.failCondition = std::make_unique<GameFailCondition::Grade>(m_condGrade);
    });
    conditionTab->settings.emplace_back(std::move(gradeCondition));

    Setting missCondition = CreateIntSetting("Miss more than", m_condMiss, { 0, 100 });
    missCondition->getter.AddLambda([this](SettingData& data) {
        data.intSetting.val = m_condMiss;
    });
    missCondition->setter.AddLambda([this](const SettingData& data) {
        m_playOptions.failCondition = std::make_unique<GameFailCondition::MissCount>(data.intSetting.val);
    });
    conditionTab->settings.emplace_back(std::move(missCondition));

    Setting missNearCondition = CreateIntSetting("Miss+Near more than", m_condMissNear, { 0, 100 });
    missNearCondition->getter.AddLambda([this](SettingData& data) {
        data.intSetting.val = m_condMissNear;
    });
    missNearCondition->setter.AddLambda([this](const SettingData& data) {
        m_playOptions.failCondition = std::make_unique<GameFailCondition::MissAndNearCount>(data.intSetting.val);
    });
    conditionTab->settings.emplace_back(std::move(missNearCondition));

    Setting gaugeCondition = CreateIntSetting("Gauge less than", m_condGauge, { 0, 100 });
    gaugeCondition->getter.AddLambda([this](SettingData& data) {
        data.intSetting.val = m_condGauge;
    });
    gaugeCondition->setter.AddLambda([this](const SettingData& data) {
        m_playOptions.failCondition = std::make_unique<GameFailCondition::Gauge>(data.intSetting.val);
    });
    conditionTab->settings.emplace_back(std::move(gaugeCondition));

    return conditionTab;
}

PracticeModeSettingsDialog::Tab PracticeModeSettingsDialog::m_CreateGameSettingTab()
{
    Tab gameSettingTab = std::make_unique<TabData>();
    gameSettingTab->name = "Settings";

    Setting globalOffsetSetting = CreateIntSetting(GameConfigKeys::GlobalOffset, "Global offset", { -200, 200 });
    globalOffsetSetting->setter.Clear();
    globalOffsetSetting->setter.AddLambda([this](const SettingData& data) {
        g_gameConfig.Set(GameConfigKeys::GlobalOffset, data.intSetting.val);
        onSettingChange.Call();
    });
    gameSettingTab->settings.emplace_back(std::move(globalOffsetSetting));

    if (m_chartIndex)
    {
        Setting chartOffsetSetting = CreateIntSetting("Chart offset", m_chartIndex->custom_offset, { -200, 200 });
        chartOffsetSetting->setter.Clear();
        chartOffsetSetting->setter.AddLambda([this](const SettingData& data) {
            m_chartIndex->custom_offset = data.intSetting.val;
            onSettingChange.Call();
        });
        gameSettingTab->settings.emplace_back(std::move(chartOffsetSetting));
    }

    Setting tempOffsetSetting = CreateIntSetting("Temporary offset", m_tempOffset, { -200, 200 });
    tempOffsetSetting->setter.Clear();
    tempOffsetSetting->setter.AddLambda([this](const SettingData& data) {
        m_tempOffset = data.intSetting.val;
        onSettingChange.Call();
    });
    gameSettingTab->settings.emplace_back(std::move(tempOffsetSetting));

    Setting leadInTimeSetting = CreateIntSetting(GameConfigKeys::PracticeLeadInTime, "Lead-in time for practices (ms)", { 250, 10000 }, 250);
    leadInTimeSetting->setter.Clear();
    leadInTimeSetting->setter.AddLambda([this](const SettingData& data) {
        g_gameConfig.Set(GameConfigKeys::PracticeLeadInTime, data.intSetting.val);
        onSettingChange.Call();
    });
    gameSettingTab->settings.emplace_back(std::move(leadInTimeSetting));

    Setting enableNavSetting = CreateBoolSetting(GameConfigKeys::PracticeSetupNavEnabled, "Enable navigation inputs for the setup");
    enableNavSetting->setter.Clear();
    enableNavSetting->setter.AddLambda([this](const SettingData& data) {
        g_gameConfig.Set(GameConfigKeys::PracticeSetupNavEnabled, data.boolSetting.val);
        onSettingChange.Call();
    });
    gameSettingTab->settings.emplace_back(std::move(enableNavSetting));

    Setting revertToSetupSetting = CreateBoolSetting(GameConfigKeys::RevertToSetupAfterScoreScreen, "Revert to the setup after the result is shown");
    revertToSetupSetting->setter.Clear();
    revertToSetupSetting->setter.AddLambda([this](const SettingData& data) {
        g_gameConfig.Set(GameConfigKeys::RevertToSetupAfterScoreScreen, data.boolSetting.val);
        onSettingChange.Call();
    });
    gameSettingTab->settings.emplace_back(std::move(revertToSetupSetting));

    return gameSettingTab;
}
