#pragma once
#include "Application.hpp"
#include "Input.hpp"
#include "ApplicationTickable.hpp"
#include "Beatmap/MapDatabase.hpp"

class TextInputCollectionDialog;
class LuaBindable;

class CollectionDialog
{
public:
	~CollectionDialog();
	bool Init(class MapDatabase* songdb);
	void Tick(float deltaTime);
	void Render(float deltaTime);
	void Open(const ChartIndex* song);

	//Call to start closing the dialog
	void Close();
	bool IsActive();
	bool IsInitialized();

	Delegate<> OnCompletion;

private:
	int lConfirm(struct lua_State* L);
	int lCancel(struct lua_State* L);
	//change between inputting a new category name and the category list
	int lChangeState(struct lua_State* L);

	void m_ChangeState();
	void m_AdvanceSelection(int steps);
	void m_OnButtonPressed(Input::Button button);
	void m_OnKeyPressed(SDL_Scancode code);
	void m_OnEntryReturn(const String& name);

	//Call when closing has been completed
	void m_Finish();

	struct lua_State* m_lua = nullptr;
	Ref<LuaBindable> m_bindable;
	Ref<TextInputCollectionDialog> m_nameEntry;
	int m_currentId;
	class MapDatabase* m_songdb;
	float m_knobAdvance = 0.0f;
	float m_sensMult = 1.0f;
	bool m_active = false;
	bool m_closing = false;
	bool m_isInitialized = false;
	bool m_shouldChangeState = false;
};