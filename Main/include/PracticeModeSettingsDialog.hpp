#pragma once
#include "BaseGameSettingsDialog.hpp"
#include "Beatmap/BeatmapObjects.hpp"
#include "Game.hpp"

struct ChartIndex;
class PracticeModeSettingsDialog : public BaseGameSettingsDialog
{
public:
	virtual ~PracticeModeSettingsDialog() = default;
	PracticeModeSettingsDialog(Game& game, MapTime& lastMapTime,
		int32& tempOffset, Game::PlayOptions& playOptions, MapTimeRange& range);
	void InitTabs() override;

	Delegate<MapTime> onSetMapTime;
	Delegate<float> onSpeedChange;
	Delegate<> onSettingChange;
	Delegate<> onPressStart;
	Delegate<> onPressExit;

private:
	Tab m_CreateMainSettingTab();
	Tab m_CreateLoopingTab();
	Tab m_CreateLoopControlTab();
	Tab m_CreateFailConditionTab();
	Tab m_CreateGameSettingTab();

	inline MapTime m_MeasureToTime(int measure) const { return m_beatmap->GetMapTimeFromMeasureInd(measure-1); }
	inline int m_TimeToMeasure(MapTime time) const { return m_beatmap->GetMeasureIndFromMapTime(time)+1; }

	void m_SetStartTime(MapTime time, int measure = -1);
	void m_SetEndTime(MapTime time, int measure = -1);
	
	std::unique_ptr<GameFailCondition> m_CreateGameFailCondition(GameFailCondition::Type type);

	ChartIndex* m_chartIndex;
	Ref<Beatmap> m_beatmap;
	MapTime m_endTime;
	MapTime& m_lastMapTime;
	int32& m_tempOffset;

	Game::PlayOptions& m_playOptions;
	// for ranges, use m_range instead of m_playOptions
	MapTimeRange& m_range;

	// Offset by 1
	int m_startMeasure = 1;
	int m_endMeasure = 1;

	SettingData* m_setStartButton = nullptr;
	SettingData* m_setEndButton = nullptr;

	// Fail conditions
	int m_condScore = static_cast<int>(MAX_SCORE);
	GradeMark m_condGrade = GradeMark::PUC;
	int m_condMiss = 0;
	int m_condMissNear = 0;
	int m_condGauge = 0;
};

