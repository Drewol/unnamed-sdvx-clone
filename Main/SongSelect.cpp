#include "stdafx.h"
#include "SongSelect.hpp"
#include "TitleScreen.hpp"
#include "Application.hpp"
#include <Shared/Profiling.hpp>
#include "Scoring.hpp"
#include "Input.hpp"
#include <GUI/GUI.hpp>
#include "SongSelectItem.hpp"
#include <Beatmap/MapDatabase.hpp>
#include "Game.hpp"
#include "TransitionScreen.hpp"
#include "GameConfig.hpp"
#include <Audio/Audio.hpp>
#ifdef _WIN32
#include "SDL_keycode.h"
#else
#include "SDL2/SDL_keycode.h"
#endif

/*
	Song preview player with fade-in/out
*/
class PreviewPlayer
{
public:
	void FadeTo(AudioStream stream)
	{
		// Already existing transition?
		if(m_nextStream)
		{
			if(m_currentStream)
			{
				m_currentStream.Destroy();
			}
			m_currentStream = m_nextStream;
		}
		m_nextStream = stream;
		m_nextSet = true;
		if(m_nextStream)
		{
			m_nextStream->SetVolume(0.0f);
			m_nextStream->Play();
		}
		m_fadeTimer = 0.0f;
	}
	void Update(float deltaTime)
	{
		if(m_nextSet)
		{
			m_fadeTimer += deltaTime;
			if(m_fadeTimer >= m_fadeDuration)
			{
				if(m_currentStream)
				{
					m_currentStream.Destroy();
				}
				m_currentStream = m_nextStream;
				if(m_currentStream)
					m_currentStream->SetVolume(1.0f);
				m_nextStream.Release();
				m_nextSet = false;
			}
			else
			{
				float fade = m_fadeTimer / m_fadeDuration;

				if(m_currentStream)
					m_currentStream->SetVolume(1.0f - fade);
				if(m_nextStream)
					m_nextStream->SetVolume(fade);
			}
		}
	}
	void Pause()
	{
		if(m_nextStream)
			m_nextStream->Pause();
		if(m_currentStream)
			m_currentStream->Pause();
	}
	void Restore()
	{
		if(m_nextStream)
			m_nextStream->Play();
		if(m_currentStream)
			m_currentStream->Play();
	}

private:
	static const float m_fadeDuration;
	float m_fadeTimer = 0.0f;
	AudioStream m_nextStream;
	AudioStream m_currentStream;
	bool m_nextSet = false;
};
const float PreviewPlayer::m_fadeDuration = 0.5f;

/*
	Song selection wheel
*/
class SelectionWheel : public Canvas
{
	Map<MapIndex*, Ref<SongSelectItem>> m_guiElements;

	Map<int32, MapIndex*> m_maps;

	Map<int32, MapIndex*> m_mapFilter;
	bool m_filterSet = false;

	// Currently selected map ID
	int32 m_currentlySelectedId = 0;
	// Currently selected sub-widget
	Ref<SongSelectItem> m_currentSelection;

	// Current difficulty index
	int32 m_currentlySelectedDiff = 0;

	// Style to use for everything song select related
	Ref<SongSelectStyle> m_style;

public:
	SelectionWheel(Ref<SongSelectStyle> style) : m_style(style)
	{
	}
	void OnMapsAdded(Vector<MapIndex*> maps)
	{
		for(auto m : maps)
		{
			m_maps.Add(m->id, m);
		}
		if(!m_currentSelection)
			AdvanceSelection(0);
	}
	void OnMapsRemoved(Vector<MapIndex*> maps)
	{
		for(auto m : maps)
		{
			m_maps.erase(m->id);
			auto it = m_guiElements.find(m);
			if(it != m_guiElements.end())
			{
				// Clear selection if a removed item was selected
				if(m_currentSelection == it->second)
					m_currentSelection.Release();

				// Remove this item from the canvas that displays the items
				Remove(it->second.As<GUIElementBase>());
				m_guiElements.erase(it);
			}
		}
		if(!m_maps.Contains(m_currentlySelectedId))
		{
			AdvanceSelection(1);
		}
	}
	void OnMapsUpdated(Vector<MapIndex*> maps)
	{
		for(auto m : maps)
		{
			auto it = m_guiElements.find(m);
			if(it != m_guiElements.end())
			{
				it->second->SetMap(m);
			}
		}
	}
	void OnMapsCleared(Map<int32, MapIndex*> newList)
	{
		m_currentSelection.Release();
		for(auto g : m_guiElements)
		{
			Remove(g.second.As<GUIElementBase>());
		}
		m_guiElements.clear();
		m_filterSet = false;
		m_mapFilter.clear();
		m_maps.clear();
		m_maps = newList;
		if(m_maps.size() > 0)
		{
			AdvanceSelection(0);
		}
	}
	void SelectRandom()
	{
		if(m_SourceCollection().empty())
			return;
		uint32 selection = Random::IntRange(0, (int32)m_SourceCollection().size() - 1);
		auto it = m_SourceCollection().begin();
		std::advance(it, selection);
		SelectMap(it->first);
	}
	void SelectMap(int32 newIndex)
	{
		Set<MapIndex*> visibleMaps;
		auto& srcCollection = m_SourceCollection();
		auto it = srcCollection.find(newIndex);
		if(it != srcCollection.end())
		{
			const float initialSpacing = 0.65f * m_style->frameMain->GetSize().y;
			const float spacing = 0.8f * m_style->frameSub->GetSize().y;
			const Anchor anchor = Anchor(0.0f, 0.5f, 1.0f, 0.5f);

			static const int32 numItems = 10;

			int32 istart;
			for(istart = 0; istart > -numItems;)
			{
				if(it == srcCollection.begin())
					break;
				it--;
				istart--;
			}

			for(int32 i = istart; i <= numItems; i++)
			{
				if(it != srcCollection.end())
				{
					visibleMaps.Add(it->second);

					// Add a new map slot
					bool newItem = m_guiElements.find(it->second) == m_guiElements.end();
					Ref<SongSelectItem> item = m_GetMapGUIElement(it->second);
					float offset = 0;
					if(i != 0)
					{
						offset = initialSpacing * Math::Sign(i) +
							spacing * (i - Math::Sign(i));
					}
					Canvas::Slot* slot = Add(item.As<GUIElementBase>());

					int32 z = -abs(i);
					slot->SetZOrder(z);

					slot->anchor = anchor;
					slot->autoSizeX = true;
					slot->autoSizeY = true;
					slot->alignment = Vector2(0, 0.5f);
					if(newItem)
					{
						// Hard set target position
						slot->offset.pos = Vector2(0, offset);
						slot->offset.size.x = z * 50.0f;
					}
					else
					{
						// Animate towards target position
						item->AddAnimation(Ref<IGUIAnimation>(
							new GUIAnimation<Vector2>(&slot->offset.pos, Vector2(0, offset), 0.1f)), true);
						item->AddAnimation(Ref<IGUIAnimation>(
							new GUIAnimation<float>(&slot->offset.size.x, z * 50.0f, 0.1f)), true);
					}

					item->fade = 1.0f - ((float)abs(i) / (float)numItems);
					item->innerOffset = item->fade * 100.0f;

					if(i == 0)
					{
						m_OnMapSelected(it->second);
					}

					it++;
				}
			}
		}
		m_currentlySelectedId = newIndex;

		// Cleanup invisible elements
		for(auto it = m_guiElements.begin(); it != m_guiElements.end();)
		{
			if(!visibleMaps.Contains(it->first))
			{
				Remove(it->second.As<GUIElementBase>());
				it = m_guiElements.erase(it);
				continue;
			}
			it++;
		}
	}
	void AdvanceSelection(int32 offset)
	{
		auto& srcCollection = m_SourceCollection();
		auto it = srcCollection.find(m_currentlySelectedId);
		if(it == srcCollection.end())
		{
			if(srcCollection.empty())
			{
				// Remove all elements, empty
				m_currentSelection.Release();
				Clear();
				m_guiElements.clear();
				return;
			}
			it = srcCollection.begin();
		}
		for(uint32 i = 0; i < (uint32)abs(offset); i++)
		{
			auto itn = it;
			if(offset < 0)
			{
				if(itn == srcCollection.begin())
					break;
				itn--;
			}
			else
				itn++;
			if(itn == srcCollection.end())
				break;
			it = itn;
		}
		if(it != srcCollection.end())
		{
			SelectMap(it->first);
		}
	}
	void SelectDifficulty(int32 newDiff)
	{
		m_currentSelection->SetSelectedDifficulty(newDiff);
		m_currentlySelectedDiff = newDiff;

		MapIndex** map = m_maps.Find(m_currentlySelectedId);
		if(map)
		{
			OnDifficultySelected.Call(map[0]->difficulties[m_currentlySelectedDiff]);
		}
	}
	void AdvanceDifficultySelection(int32 offset)
	{
		if(!m_currentSelection)
			return;
		MapIndex* map = m_maps[m_currentlySelectedId];
		int32 newIdx = m_currentlySelectedDiff + offset;
		newIdx = Math::Clamp(newIdx, 0, (int32)map->difficulties.size() - 1);
		SelectDifficulty(newIdx);
	}

	// Called when a new map is selected
	Delegate<MapIndex*> OnMapSelected;
	Delegate<DifficultyIndex*> OnDifficultySelected;

	// Set display filter
	void SetFilter(Map<int32, MapIndex *> filter)
	{
		m_mapFilter = filter;
		m_filterSet = true;
		AdvanceSelection(0);
	}
	void ClearFilter()
	{
		if(m_filterSet)
		{
			m_filterSet = false;
			AdvanceSelection(0);
		}
	}

	MapIndex* GetSelection() const
	{
		MapIndex*const* map = m_maps.Find(m_currentlySelectedId);
		if(map)
			return *map;
		return nullptr;
	}
	DifficultyIndex* GetSelectedDifficulty() const
	{
		MapIndex*const* map = m_maps.Find(m_currentlySelectedId);
		if(map)
		{
			return map[0]->difficulties[m_currentlySelectedDiff];
		}
		return nullptr;
	}

private:
	const Map<int32, MapIndex*>& m_SourceCollection()
	{
		return m_filterSet ? m_mapFilter : m_maps;
	}
	Ref<SongSelectItem> m_GetMapGUIElement(MapIndex* index)
	{
		auto it = m_guiElements.find(index);
		if(it != m_guiElements.end())
			return it->second;

		Ref<SongSelectItem> newItem = Ref<SongSelectItem>(new SongSelectItem(m_style));

		// Send first map as metadata settings
		const BeatmapSettings& firstSettings = index->difficulties[0]->settings;
		newItem->SetMap(index);
		m_guiElements.Add(index, newItem);
		return newItem;
	}
	void m_OnMapSelected(MapIndex* map)
	{
		// Update compact mode selection views
		if(m_currentSelection)
			m_currentSelection->SwitchCompact(true);
		m_currentSelection = m_guiElements[map];
		m_currentSelection->SwitchCompact(false);

		//if(map && map->id == m_currentlySelectedId)
		//	return;

		// Clamp diff selection
		int32 selectDiff = m_currentlySelectedDiff;
		if(m_currentlySelectedDiff >= (int32)map->difficulties.size())
		{
			selectDiff = (int32)map->difficulties.size() - 1;
		}
		SelectDifficulty(selectDiff);

		OnMapSelected.Call(map);
	}
};

/*
	Song select window/screen
*/
class SongSelect_Impl : public SongSelect
{
private:
	Timer m_dbUpdateTimer;
	Ref<Canvas> m_canvas;
	MapDatabase m_mapDatabase;

	Ref<SongSelectStyle> m_style;
	Ref<CommonGUIStyle> m_commonGUIStyle;

	// Shows additional information about a map
	Ref<SongStatistics> m_statisticsWindow;
	// Map selection wheel
	Ref<SelectionWheel> m_selectionWheel;
	// Search field
	Ref<TextInputField> m_searchField;

	// Player of preview music
	PreviewPlayer m_previewPlayer;

	// Current map that has music being preview played
	MapIndex* m_currentPreviewAudio;

	// Select sound
	Sample m_selectSound;

    // Navigation variables
    float m_advanceSong = 0.0f;
    float m_advanveDiff = 0.0f;
	MouseLockHandle m_lockMouse;
	bool m_suspended = false;
	bool m_previewLoaded = true;
	uint64_t m_previewDelayTicks = 0;

public:
	bool Init() override
	{
		m_commonGUIStyle = g_commonGUIStyle;
		m_canvas = Utility::MakeRef(new Canvas());

		// Load textures for song select
		m_style = SongSelectStyle::Get(g_application);

		// Split between statistics and selection wheel (in percentage)
		const float screenSplit = 0.0f;

		// Statistics window
		m_statisticsWindow = Ref<SongStatistics>(new SongStatistics(m_style));
		Canvas::Slot* statisticsSlot = m_canvas->Add(m_statisticsWindow.As<GUIElementBase>());
		statisticsSlot->anchor = Anchor(0, 0, screenSplit, 1.0f);
		statisticsSlot->SetZOrder(2);

        // Set up input
		g_input.OnButtonPressed.Add(this, &SongSelect_Impl::m_OnButtonPressed);
        
		Panel* background = new Panel();
		background->imageFillMode = FillMode::Fill;
		background->texture = g_application->LoadTexture("bg.png");
		background->color = Color(0.5f);
		Canvas::Slot* bgSlot = m_canvas->Add(background->MakeShared());
		bgSlot->anchor = Anchors::Full;
		bgSlot->SetZOrder(-2);

		LayoutBox* box = new LayoutBox();
		Canvas::Slot* boxSlot = m_canvas->Add(box->MakeShared());
		boxSlot->anchor = Anchor(screenSplit, 0, 1.0f, 1.0f);
		box->layoutDirection = LayoutBox::Vertical;
		{
			m_searchField = Ref<TextInputField>(new TextInputField(m_commonGUIStyle));
			LayoutBox::Slot* searchFieldSlot = box->Add(m_searchField.As<GUIElementBase>());
			searchFieldSlot->fillX = true;
			m_searchField->OnTextUpdated.Add(this, &SongSelect_Impl::OnSearchTermChanged);

			m_selectionWheel = Ref<SelectionWheel>(new SelectionWheel(m_style));
			LayoutBox::Slot* selectionSlot = box->Add(m_selectionWheel.As<GUIElementBase>());
			selectionSlot->fillY = true;
			m_selectionWheel->OnMapSelected.Add(this, &SongSelect_Impl::OnMapSelected);
			m_selectionWheel->OnDifficultySelected.Add(this, &SongSelect_Impl::OnDifficultySelected);
		}

		// Select interface sound
		m_selectSound = g_audio->CreateSample("audio/menu_click.wav");

		// Setup the map database
		m_mapDatabase.AddSearchPath(g_gameConfig.GetString(GameConfigKeys::SongFolder));

		m_mapDatabase.OnMapsAdded.Add(m_selectionWheel.GetData(), &SelectionWheel::OnMapsAdded);
		m_mapDatabase.OnMapsUpdated.Add(m_selectionWheel.GetData(), &SelectionWheel::OnMapsUpdated);
		m_mapDatabase.OnMapsRemoved.Add(m_selectionWheel.GetData(), &SelectionWheel::OnMapsRemoved);
		m_mapDatabase.OnMapsCleared.Add(m_selectionWheel.GetData(), &SelectionWheel::OnMapsCleared);
		m_mapDatabase.StartSearching();

		m_selectionWheel->SelectRandom();

		return true;
	}
	~SongSelect_Impl()
	{
		// Clear callbacks
		m_mapDatabase.OnMapsCleared.Clear();
	}

	// When a map is selected in the song wheel
	void OnMapSelected(MapIndex* map)
	{
		if (map == m_currentPreviewAudio){
			if (m_previewDelayTicks){
				--m_previewDelayTicks;
			}else if (!m_previewLoaded){
				// Set current preview audio
				DifficultyIndex* previewDiff = m_currentPreviewAudio->difficulties[0];
				String audioPath = m_currentPreviewAudio->path + Path::sep + previewDiff->settings.audioNoFX;

				AudioStream previewAudio = g_audio->CreateStream(audioPath);
				if (previewAudio)
				{
					previewAudio->SetPosition(previewDiff->settings.previewOffset);
					m_previewPlayer.FadeTo(previewAudio);
				}
				else
				{
					Logf("Failed to load preview audio from [%s]", Logger::Warning, audioPath);
					m_previewPlayer.FadeTo(AudioStream());
				}
				m_previewLoaded = true;
				// m_previewPlayer.Restore();
			}
		} else{
			// Wait at least 15 ticks before attempting to load song to prevent loading songs while scrolling very fast
			m_previewDelayTicks = 15;
			m_currentPreviewAudio = map;
			m_previewLoaded = false;
		}
	}
	// When a difficulty is selected in the song wheel
	void OnDifficultySelected(DifficultyIndex* diff)
	{
	}
	void OnSearchTermChanged(const WString& search)
	{
		if(search.empty())
			m_selectionWheel->ClearFilter();
		else
		{
			String utf8Search = Utility::ConvertToUTF8(search);
			Map<int32, MapIndex*> filter = m_mapDatabase.FindMaps(utf8Search);
			m_selectionWheel->SetFilter(filter);
		}
	}
    

    void m_OnButtonPressed(Input::Button buttonCode)
    {
		if (m_suspended)
			return;
	    if(buttonCode == Input::Button::BT_S && !IsSuspended())
        {
            
			bool autoplay = (g_gameWindow->GetModifierKeys() & ModifierKeys::Ctrl) == ModifierKeys::Ctrl;
			MapIndex* map = m_selectionWheel->GetSelection();
			if(map)
			{
				DifficultyIndex* diff = m_selectionWheel->GetSelectedDifficulty();

				Game* game = Game::Create(*diff);
				if(!game)
				{
					Logf("Failed to start game", Logger::Error);
					return;
				}
				game->GetScoring().autoplay = autoplay;

				// Transition to game
				TransitionScreen* transistion = TransitionScreen::Create(game);
				g_application->AddTickable(transistion);
            }
        }
    }

	virtual void OnKeyPressed(int32 key)
	{
		if(key == SDLK_DOWN)
		{
			m_selectionWheel->AdvanceSelection(1);
		}
		else if(key == SDLK_UP)
		{
			m_selectionWheel->AdvanceSelection(-1);
		}
		else if(key == SDLK_PAGEDOWN)
		{
			m_selectionWheel->AdvanceSelection(5);
		}
		else if(key == SDLK_PAGEUP)
		{
			m_selectionWheel->AdvanceSelection(-5);
		}
		else if(key == SDLK_LEFT)
		{
			m_selectionWheel->AdvanceDifficultySelection(-1);
		}
		else if(key == SDLK_RIGHT)
		{
			m_selectionWheel->AdvanceDifficultySelection(1);
		}
		else if(key == SDLK_F5)
		{
			m_mapDatabase.StartSearching();
		}
		else if(key == SDLK_F2)
		{
			m_selectionWheel->SelectRandom();
		}
		else if (key == SDLK_ESCAPE)
		{
			m_suspended = true;
			g_application->RemoveTickable(this);
		}
	}
	virtual void OnKeyReleased(int32 key)
	{
		
	}
	virtual void Tick(float deltaTime) override
	{
		if(m_dbUpdateTimer.Milliseconds() > 500)
		{
			m_mapDatabase.Update();
			m_dbUpdateTimer.Restart();
		}
        
        // Tick navigation
		if (!IsSuspended())
            TickNavigation(deltaTime);

		// Background
		m_previewPlayer.Update(deltaTime);
	}

    void TickNavigation(float deltaTime)
    {

		// Lock mouse to screen when active 
		if(g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice) == InputDevice::Mouse && g_gameWindow->IsActive())
		{
			m_lockMouse = g_input.LockMouse();
		    g_gameWindow->SetCursorVisible(false);
		}
		else
		{
			if(m_lockMouse)
				m_lockMouse.Release();
			g_gameWindow->SetCursorVisible(true);
		}

		
		
        // Song navigation using laser inputs
        float diff_input = g_input.GetInputLaserDir(0);
        float song_input = g_input.GetInputLaserDir(1);
        
        m_advanveDiff += diff_input;
        m_advanceSong += song_input;

        m_selectionWheel->AdvanceDifficultySelection((int)m_advanveDiff);
        m_selectionWheel->AdvanceSelection((int)m_advanceSong);
        
        m_advanveDiff -= (int)m_advanveDiff;
        m_advanceSong -= (int)m_advanceSong; 
    }

	virtual void OnSuspend()
	{
		m_suspended = true;
		m_previewPlayer.Pause();
		m_mapDatabase.StopSearching();
		if (m_lockMouse)
			m_lockMouse.Release();

		g_rootCanvas->Remove(m_canvas.As<GUIElementBase>());
	}
	virtual void OnRestore()
	{
		m_suspended = false;
		m_previewPlayer.Restore();
		m_mapDatabase.StartSearching();

		OnSearchTermChanged(m_searchField->GetText());
		
		Canvas::Slot* slot = g_rootCanvas->Add(m_canvas.As<GUIElementBase>());
		slot->anchor = Anchors::Full;
	}
};

SongSelect* SongSelect::Create()
{
	SongSelect_Impl* impl = new SongSelect_Impl();
	return impl;
}
