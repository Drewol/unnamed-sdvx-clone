#pragma once
#include "Application.hpp"
#include "SongSort.hpp"
#include "SongFilter.hpp"
#include <Beatmap/MapDatabase.hpp>

class TextInput
{
public:
	String input;
	String composition;
	uint32 backspaceCount;
	bool active = false;
	Delegate<const String&> OnTextChanged;

	~TextInput()
	{
		g_gameWindow->OnTextInput.RemoveAll(this);
		g_gameWindow->OnTextComposition.RemoveAll(this);
		g_gameWindow->OnKeyRepeat.RemoveAll(this);
		g_gameWindow->OnKeyPressed.RemoveAll(this);
	}

	void OnTextInput(const String& wstr)
	{
		input += wstr;
		OnTextChanged.Call(input);
	}
	void OnTextComposition(const Graphics::TextComposition& comp)
	{
		composition = comp.composition;
	}
	void OnKeyRepeat(SDL_Scancode key)
	{
		if (key == SDL_SCANCODE_BACKSPACE)
		{
			if (input.empty())
				backspaceCount++; // Send backspace
			else
			{
				auto it = input.end(); // Modify input string instead
				--it;
				while ((*it & 0b11000000) == 0b10000000)
				{
					input.erase(it);
					--it;
				}
				input.erase(it);
				OnTextChanged.Call(input);
			}
		}
	}
	void OnKeyPressed(SDL_Scancode code, int32 delta)
	{
		SDL_Keycode key = SDL_GetKeyFromScancode(code);
		if (key == SDLK_v)
		{
			if (g_gameWindow->GetModifierKeys() == ModifierKeys::Ctrl)
			{
				if (g_gameWindow->GetTextComposition().composition.empty())
				{
					// Paste clipboard text into input buffer
					input += g_gameWindow->GetClipboard();
				}
			}
		}
	}
	void SetActive(bool state)
	{
		active = state;
		if (state)
		{
			SDL_StartTextInput();
			g_gameWindow->OnTextInput.Add(this, &TextInput::OnTextInput);
			g_gameWindow->OnTextComposition.Add(this, &TextInput::OnTextComposition);
			g_gameWindow->OnKeyRepeat.Add(this, &TextInput::OnKeyRepeat);
			g_gameWindow->OnKeyPressed.Add(this, &TextInput::OnKeyPressed);
		}
		else
		{
			SDL_StopTextInput();
			g_gameWindow->OnTextInput.RemoveAll(this);
			g_gameWindow->OnTextComposition.RemoveAll(this);
			g_gameWindow->OnKeyRepeat.RemoveAll(this);
			g_gameWindow->OnKeyPressed.RemoveAll(this);
		}
	}
	void Reset()
	{
		backspaceCount = 0;
		input.clear();
	}
	void Tick()
	{
	}
};

class SearchParser {
public:
	using SearchKey = std::pair<String, String*>;
	static String Parse(const String& input, const Vector<SearchKey> keys);
};
