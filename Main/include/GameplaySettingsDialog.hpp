#pragma once
#include "BaseGameSettingsDialog.hpp"

class SongSelect;
class MultiplayerScreen;

class GameplaySettingsDialog: public BaseGameSettingsDialog
{
public:
    GameplaySettingsDialog(SongSelect* songSelectScreen) : m_songSelectScreen(songSelectScreen) {}
    GameplaySettingsDialog(MultiplayerScreen* multiplayerScreen) : m_multiPlayerScreen(multiplayerScreen) {}
    GameplaySettingsDialog() {};

    void InitTabs() override;
    void OnAdvanceTab() override;

    Delegate<> onPressAutoplay;
    Delegate<> onPressPractice;
    Delegate<int> onSongOffsetChange;
    Delegate<> onPressComputeSongOffset;

private:
    SongSelect* m_songSelectScreen = nullptr;
    MultiplayerScreen* m_multiPlayerScreen = nullptr;
    Setting m_CreateSongOffsetSetting();
    Setting m_CreateProfileSetting(const String&);
};