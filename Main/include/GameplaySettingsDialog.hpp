#pragma once
#include "BaseGameSettingsDialog.hpp"

class SongSelect;

class GameplaySettingsDialog: public BaseGameSettingsDialog
{
public:
    GameplaySettingsDialog(SongSelect* songSelectScreen);

    void InitTabs() override;
    void OnAdvanceTab() override;

    Delegate<> onPressAutoplay;
    Delegate<> onPressPractice;
    Delegate<int> onSongOffsetChange;

private:
    SongSelect* songSelectScreen;
    Setting m_CreateSongOffsetSetting();
};