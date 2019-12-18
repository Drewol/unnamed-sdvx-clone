#pragma once
#include <Graphics/Keys.hpp>
#include <Graphics/Gamepad.hpp>
#include <SDL2/SDL.h>

namespace Graphics
{
	// Windowed or bordered window style
	enum class WindowStyle
	{
		Windowed, Borderless
	};

	// Text input data
	struct TextComposition
	{
		WString composition;
		int32 cursor;
		int32 selectionLength;
	};

	/*
		Simple window class that manages window messages, window style and input
		Renamed from Window to DesktopWindow to avoid conflicts with libX11 on Linux
	*/
	class Window : Unique
	{
	public:
		Window(Vector2i size = Vector2i(800, 600), uint8 samplecount = 0);
		~Window();
		// Show the window
		void Show();
		// Hide the window
		void Hide();
		// Call every frame to update the window message loop
		// returns false if the window received a close message
		bool Update();
		// On windows: returns the HWND
		void* Handle();
		// Set the window title (caption)
		void SetCaption(const WString& cap);
		// Closes the window
		void Close();

		Vector2i GetMousePos();
		void SetMousePos(const Vector2i& pos);
		void SetRelativeMouseMode(bool enabled);
		bool GetRelativeMouseMode();

		// Sets cursor to use
		void SetCursor(Ref<class ImageRes> image, Vector2i hotspot = Vector2i(0,0));
		void SetCursorVisible(bool visible);

		// Switches between borderless and windowed
		void SetWindowStyle(WindowStyle style);

		// Get full window position
		Vector2i GetWindowPos() const;
		// Set full window position
		void SetWindowPos(const Vector2i& pos);

		// Window Client area size
		Vector2i GetWindowSize() const;

		// Set vsync setting
		void SetVSync(int8 setting);

		// Window is active
		bool IsActive() const;
		// Set window client area size
		void SetWindowSize(const Vector2i& size);
		void SwitchFullscreen(int w, int h, int fsw = -1, int fsh = -1, uint32 monitorID = -1, bool windowedFullscreen = false);
		bool IsFullscreen() const;

		int GetDisplayIndex() const;

		// Checks if a key is pressed
		bool IsKeyPressed(int32 key) const;

		ModifierKeys GetModifierKeys() const;

		// Start allowing text input
		void StartTextInput();
		// Stop allowing text input
		void StopTextInput();
		// Used to get current IME working data
		const TextComposition& GetTextComposition() const;

		// Show a simple message box
		// level 0 = error, 1 = warning, 2 = info
		void ShowMessageBox(String title, String message, int severity);

		// Get the text currently in the clipboard
		WString GetClipboard() const;

		// The number of available gamepad devices
		int32 GetNumGamepads() const;
		// List of gamepad device names
		Vector<String> GetGamepadDeviceNames() const;
		// Open a gamepad within the range of the number of gamepads
		Ref<Gamepad> OpenGamepad(int32 deviceIndex);

		Delegate<int32> OnKeyPressed;
		Delegate<int32> OnKeyReleased;
		Delegate<MouseButton> OnMousePressed;
		Delegate<MouseButton> OnMouseReleased;
		Delegate<int32, int32> OnMouseMotion;
		Delegate<SDL_Event> OnAnyEvent;
		// Mouse scroll wheel
		//	Positive for scroll down
		//	Negative for scroll up
		Delegate<int32> OnMouseScroll;
		// Called for the initial an repeating presses of a key
		Delegate<int32> OnKeyRepeat;
		Delegate<const WString&> OnTextInput;
		Delegate<const TextComposition&> OnTextComposition;
		Delegate<const Vector2i&> OnResized;

	private:
		class Window_Impl* m_impl;
	};
}