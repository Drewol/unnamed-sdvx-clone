#include "stdafx.h"
#include "Window.hpp"
#include "KeyMap.hpp"
#include "Image.hpp"
#include "Gamepad_Impl.hpp"

#include <Shared/Rect.hpp>
#include <Shared/Profiling.hpp>

static void GetDisplayBounds(Vector<Shared::Recti>& bounds)
{
	const int displayNum = SDL_GetNumVideoDisplays();
	if (displayNum <= 0) return;

	for (int monitorId = 0; monitorId < displayNum; ++monitorId)
	{
		SDL_Rect rect;
		if (SDL_GetDisplayBounds(monitorId, &rect) < 0) break;

		bounds.emplace_back(Shared::Recti{ {rect.x, rect.y}, {rect.w, rect.h} });
	}
}

namespace Graphics
{
	/* SDL Instance singleton */
	class SDL
	{
	protected:
		SDL()
		{
			SDL_SetMainReady();
			int r = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
			if (r != 0)
			{
				Logf("SDL_Init Failed: %s", Logger::Severity::Error, SDL_GetError());
				assert(false);
			}
		}

	public:
		~SDL()
		{
			SDL_Quit();
		}
		static SDL &Main()
		{
			static SDL sdl;
			return sdl;
		}
	};

	class Window_Impl
	{
	public:
		// Handle to outer class to send delegates
		Window &outer;

	public:
		Window_Impl(Window &outer, Vector2i size, uint8 sampleCount) : outer(outer)
		{
			ProfilerScope $("Creating Window");
			SDL::Main();

			m_clntSize = size;

#ifdef _DEBUG
			m_caption = L"USC-Game Debug";
#else
			m_caption = L"USC-Game";
#endif
			String titleUtf8 = Utility::ConvertToUTF8(m_caption);

			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, sampleCount);
			SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 2);
			SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

			m_window = SDL_CreateWindow(*titleUtf8, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
										m_clntSize.x, m_clntSize.y, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
			assert(m_window);

			uint32 numJoysticks = SDL_NumJoysticks();
			if (numJoysticks == 0)
			{
				Log("No joysticks found", Logger::Severity::Warning);
			}
			else
			{
				Logf("Listing %d Joysticks:", Logger::Severity::Info, numJoysticks);
				for (uint32 i = 0; i < numJoysticks; i++)
				{
					SDL_Joystick *joystick = SDL_JoystickOpen(i);
					if (!joystick)
					{
						Logf("[%d] <failed to open>", Logger::Severity::Warning, i);
						continue;
					}
					String deviceName = SDL_JoystickName(joystick);

					Logf("[%d] \"%s\" (%d buttons, %d axes, %d hats)", Logger::Severity::Info,
						 i, deviceName, SDL_JoystickNumButtons(joystick), SDL_JoystickNumAxes(joystick), SDL_JoystickNumHats(joystick));

					SDL_JoystickClose(joystick);
				}
			}
		}
		~Window_Impl()
		{
			// Release gamepads
			for (auto it : m_gamepads)
			{
				it.second.reset();
			}

			SDL_DestroyWindow(m_window);
		}

		void SetWindowPos(const Vector2i &pos)
		{
			SDL_SetWindowPosition(m_window, pos.x, pos.y);
		}

		void SetWindowPosToCenter(int32 monitorId)
		{
			SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED_DISPLAY(monitorId), SDL_WINDOWPOS_CENTERED_DISPLAY(monitorId));
		}

		void ShowMessageBox(String title, String message, int severity)
		{
			uint32 flags = 0;
			switch (severity)
			{
			case 0:
				flags = SDL_MESSAGEBOX_ERROR;
				break;
			case 1:
				flags = SDL_MESSAGEBOX_WARNING;
				break;
			default:
				flags = SDL_MESSAGEBOX_INFORMATION;
			}
			SDL_ShowSimpleMessageBox(flags, title.c_str(), message.c_str(), m_window);
		}

		bool ShowYesNoMessage(String title, String message)
		{
			const SDL_MessageBoxButtonData buttons[] =
				{
					{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "no"},
					{SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "yes"},
				};
			const SDL_MessageBoxData messageboxdata =
				{
					SDL_MESSAGEBOX_INFORMATION,
					NULL,
					*title,
					*message,
					SDL_arraysize(buttons),
					buttons,
					NULL};
			int buttonid;
			if (SDL_ShowMessageBox(&messageboxdata, &buttonid) < 0)
			{
				Logf("Could not display message box for '%s'", Logger::Severity::Info, *message);
				return false;
			}
			return buttonid == 1;
		}

		Vector2i GetWindowPos() const
		{
			Vector2i res;
			SDL_GetWindowPosition(m_window, &res.x, &res.y);
			return res;
		}

		void SetWindowSize(const Vector2i &size)
		{
			SDL_SetWindowSize(m_window, size.x, size.y);
		}

		Vector2i GetWindowSize() const
		{
			Vector2i res;
			SDL_GetWindowSize(m_window, &res.x, &res.y);
			return res;
		}

		void SetVSync(int8 setting)
		{
			if (SDL_GL_SetSwapInterval(setting) == -1)
				Logf("Failed to set VSync: %s", Logger::Severity::Error, SDL_GetError());
		}

		void SetWindowStyle(WindowStyle style)
		{
		}

		/* input handling */
		void HandleKeyEvent(const SDL_Keysym &keySym, uint8 newState, int32 repeat)
		{
			const SDL_Scancode code = keySym.scancode;
			SDL_Keymod m = static_cast<SDL_Keymod>(keySym.mod);

			m_modKeys = ModifierKeys::None;

			if ((m & KMOD_ALT) != 0)
				(uint8 &)m_modKeys |= (uint8)ModifierKeys::Alt;
			if ((m & KMOD_CTRL) != 0)
				(uint8 &)m_modKeys |= (uint8)ModifierKeys::Ctrl;
			if ((m & KMOD_SHIFT) != 0)
				(uint8 &)m_modKeys |= (uint8)ModifierKeys::Shift;

			uint8 &currentState = m_keyStates[code];

			if (currentState != newState)
			{
				currentState = newState;
				if (newState == 1)
				{
					outer.OnKeyPressed.Call(code);
				}
				else
				{
					outer.OnKeyReleased.Call(code);
				}
			}
			if (currentState == 1)
			{
				outer.OnKeyRepeat.Call(code);
			}
		}

		/* Window show hide, positioning, etc.*/
		void Show()
		{
			SDL_ShowWindow(m_window);
		}
		void Hide()
		{
			SDL_HideWindow(m_window);
		}
		void SetCaption(const WString &cap)
		{
			m_caption = L"Window";
			String titleUtf8 = Utility::ConvertToUTF8(m_caption);
			SDL_SetWindowTitle(m_window, *titleUtf8);
		}

		void SetCursor(Ref<class ImageRes> image, Vector2i hotspot)
		{
#ifdef _WIN32
			if (currentCursor)
			{
				SDL_FreeCursor(currentCursor);
				currentCursor = nullptr;
			}
			if (image)
			{
				Vector2i size = image->GetSize();
				void *bits = image->GetBits();
				SDL_Surface *surf = SDL_CreateRGBSurfaceFrom(bits, size.x, size.y, 32, size.x * 4,
															 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
				if (surf)
				{
					currentCursor = SDL_CreateColorCursor(surf, hotspot.x, hotspot.y);
				}
			}
			SDL_SetCursor(currentCursor);
#endif
			/// NOTE: Cursor transparency is broken on linux
		}

		// Update loop
		Timer t;
		bool Update()
		{
			SDL_Event evt;
			while (SDL_PollEvent(&evt))
			{
				if (evt.type == SDL_EventType::SDL_KEYDOWN)
				{
					HandleKeyEvent(evt.key.keysym, 1, evt.key.repeat);
				}
				else if (evt.type == SDL_EventType::SDL_KEYUP)
				{
					HandleKeyEvent(evt.key.keysym, 0, 0);
				}
				else if (evt.type == SDL_EventType::SDL_JOYBUTTONDOWN)
				{
					Gamepad_Impl **gp = m_joystickMap.Find(evt.jbutton.which);
					if (gp)
						gp[0]->HandleInputEvent(evt.jbutton.button, true);
				}
				else if (evt.type == SDL_EventType::SDL_JOYBUTTONUP)
				{
					Gamepad_Impl **gp = m_joystickMap.Find(evt.jbutton.which);
					if (gp)
						gp[0]->HandleInputEvent(evt.jbutton.button, false);
				}
				else if (evt.type == SDL_EventType::SDL_JOYAXISMOTION)
				{
					Gamepad_Impl **gp = m_joystickMap.Find(evt.jaxis.which);
					if (gp)
						gp[0]->HandleAxisEvent(evt.jaxis.axis, evt.jaxis.value);
				}
				else if (evt.type == SDL_EventType::SDL_JOYHATMOTION)
				{
					Gamepad_Impl **gp = m_joystickMap.Find(evt.jhat.which);
					if (gp)
						gp[0]->HandleHatEvent(evt.jhat.hat, evt.jhat.value);
				}
				else if (evt.type == SDL_EventType::SDL_MOUSEBUTTONDOWN)
				{
					switch (evt.button.button)
					{
					case SDL_BUTTON_LEFT:
						outer.OnMousePressed.Call(MouseButton::Left);
						break;
					case SDL_BUTTON_MIDDLE:
						outer.OnMousePressed.Call(MouseButton::Middle);
						break;
					case SDL_BUTTON_RIGHT:
						outer.OnMousePressed.Call(MouseButton::Right);
						break;
					}
				}
				else if (evt.type == SDL_EventType::SDL_MOUSEBUTTONUP)
				{
					switch (evt.button.button)
					{
					case SDL_BUTTON_LEFT:
						outer.OnMouseReleased.Call(MouseButton::Left);
						break;
					case SDL_BUTTON_MIDDLE:
						outer.OnMouseReleased.Call(MouseButton::Middle);
						break;
					case SDL_BUTTON_RIGHT:
						outer.OnMouseReleased.Call(MouseButton::Right);
						break;
					}
				}
				else if (evt.type == SDL_EventType::SDL_MOUSEWHEEL)
				{
					if (evt.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
					{
						outer.OnMouseScroll.Call(evt.wheel.y);
					}
					else
					{
						outer.OnMouseScroll.Call(-evt.wheel.y);
					}
				}
				else if (evt.type == SDL_EventType::SDL_MOUSEMOTION)
				{
					outer.OnMouseMotion.Call(evt.motion.xrel, evt.motion.yrel);
				}
				else if (evt.type == SDL_EventType::SDL_QUIT)
				{
					m_closed = true;
				}
				else if (evt.type == SDL_EventType::SDL_WINDOWEVENT)
				{
					if (evt.window.windowID == SDL_GetWindowID(m_window))
					{
						if (evt.window.event == SDL_WindowEventID::SDL_WINDOWEVENT_SIZE_CHANGED)
						{
							Vector2i newSize(evt.window.data1, evt.window.data2);
							outer.OnResized.Call(newSize);
						}
						else if (evt.window.event == SDL_WindowEventID::SDL_WINDOWEVENT_FOCUS_GAINED)
						{
							outer.OnFocusChanged.Call(true);
						}
						else if (evt.window.event == SDL_WindowEventID::SDL_WINDOWEVENT_FOCUS_LOST)
						{
							outer.OnFocusChanged.Call(false);
						}
						else if (evt.window.event == SDL_WindowEventID::SDL_WINDOWEVENT_MOVED)
						{
							Vector2i newPos(evt.window.data1, evt.window.data2);
							outer.OnMoved.Call(newPos);
						}
					}
				}
				else if (evt.type == SDL_EventType::SDL_TEXTINPUT)
				{
					outer.OnTextInput.Call(evt.text.text);
				}
				else if (evt.type == SDL_EventType::SDL_TEXTEDITING)
				{
					SDL_Rect scr;
					SDL_GetWindowPosition(m_window, &scr.x, &scr.y);
					SDL_GetWindowSize(m_window, &scr.w, &scr.h);
					SDL_SetTextInputRect(&scr);

					m_textComposition.composition = evt.edit.text;
					m_textComposition.cursor = evt.edit.start;
					m_textComposition.selectionLength = evt.edit.length;
					outer.OnTextComposition.Call(m_textComposition);
				}
				outer.OnAnyEvent.Call(evt);
			}
			return !m_closed;
		}

		void SetWindowed(const Vector2i& pos, const Vector2i& size)
		{
			SDL_SetWindowFullscreen(m_window, 0);
			SDL_RestoreWindow(m_window);

			SetWindowSize(size);

			SDL_SetWindowResizable(m_window, SDL_TRUE);
			SDL_SetWindowBordered(m_window, SDL_TRUE);

			SetWindowPos(pos);

			m_fullscreen = false;
		}

		void SetWindowedFullscreen(int32 monitorId)
		{
			if (monitorId == -1)
			{
				monitorId = SDL_GetWindowDisplayIndex(m_window);
			}

			SDL_DisplayMode dm;
			SDL_GetDesktopDisplayMode(monitorId, &dm);

			SDL_Rect bounds;
			SDL_GetDisplayBounds(monitorId, &bounds);

			SDL_RestoreWindow(m_window);
			SDL_SetWindowSize(m_window, dm.w, dm.h);
			SDL_SetWindowPosition(m_window, bounds.x, bounds.y);
			SDL_SetWindowResizable(m_window, SDL_FALSE);

			m_fullscreen = true;
		}

		void SetFullscreen(int32 monitorId, const Vector2i& res)
		{
			if (monitorId == -1)
			{
				monitorId = SDL_GetWindowDisplayIndex(m_window);
			}

			SDL_DisplayMode dm;
			SDL_GetDesktopDisplayMode(monitorId, &dm);

			if (res.x != -1)
			{
				dm.w = res.x;
			}

			if (res.y != -1)
			{
				dm.h = res.y;
			}

			// move to correct display
			SetWindowPosToCenter(monitorId);

			SDL_SetWindowDisplayMode(m_window, &dm);
			SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN);

			m_fullscreen = true;
		}

		void SetPosAndShape(const Window::PosAndShape& posAndShape, bool ensureInBound)
		{
			int32 monitorId = posAndShape.monitorId;
			if (monitorId == -1)
			{
				monitorId = SDL_GetWindowDisplayIndex(m_window);
			}

			if (ensureInBound)
			{
				// Adjust the monitor to use, if the monitor does not exist.
				SDL_DisplayMode dm;
				if (SDL_GetDesktopDisplayMode(monitorId, &dm) < 0)
				{
					Logf("Monitor %d is not available; using 0 instead", Logger::Severity::Warning, monitorId);
					monitorId = 0;
				}
			}

			switch (posAndShape.mode)
			{
			case Window::PosAndShape::Mode::Windowed:
			{
				Vector2i windowPos = posAndShape.windowPos;
				Vector2i windowSize = posAndShape.windowSize;

				SetWindowed(windowPos, windowSize);

				// Adjust window position and size, if the window can't be fit into the display region.
				if (ensureInBound)
				{
					int borderTop = 0, borderLeft = 0, borderBottom = 0, borderRight = 0;
					SDL_GetWindowBordersSize(m_window, &borderTop, &borderLeft, &borderBottom, &borderRight);

					Shared::Recti windowRect(windowPos - Vector2i{ borderLeft, borderTop }, windowSize + Vector2i{ borderLeft+borderRight, borderTop+borderBottom });

					Vector<Shared::Recti> bounds;
					GetDisplayBounds(bounds);

					if (bounds.empty())
					{
						break;
					}

					Logf("Adjusting window size for %u windows...", Logger::Severity::Info, bounds.size());

					if (monitorId >= static_cast<int>(bounds.size()))
					{
						monitorId = 0;
					}

					bool foundContained = bounds[monitorId].Contains(windowRect);
					if (foundContained)
					{
						break;
					}

					bool foundLargeEnough = bounds[monitorId].NotSmallerThan(windowRect.size);
					if (!foundLargeEnough)
					{

						for (int i = 0; i < static_cast<int>(bounds.size()); ++i)
						{
							if (bounds[i].Contains(windowRect))
							{
								foundContained = true;
								monitorId = i;
								break;
							}

							if (bounds[i].NotSmallerThan(windowRect.size))
							{
								foundLargeEnough = true;
								monitorId = i;
								break;
							}
						}
					}

					if (foundContained)
					{
						break;
					}

					// Optimally, this should be set to the largest rectangle inside `bounds intersect windowRect`.
					Shared::Recti adjustedRect = bounds[monitorId];
					if (foundLargeEnough)
					{
						adjustedRect.size = windowRect.size;
					}

					adjustedRect.pos = bounds[monitorId].pos + (bounds[monitorId].size - adjustedRect.size) / 2;
					
					windowPos = adjustedRect.pos + Vector2i{ borderLeft, borderTop };
					windowSize = adjustedRect.size - Vector2i{ borderLeft+borderRight, borderTop+borderBottom };

					SetWindowSize(windowSize);
					SetWindowPosToCenter(monitorId);
				}
			}
				break;
			case Window::PosAndShape::Mode::WindowedFullscreen:
				SetWindowedFullscreen(monitorId);
				break;
			case Window::PosAndShape::Mode::Fullscreen:
				SetFullscreen(monitorId, posAndShape.fullscreenSize);
				break;
			}
		}

		inline bool IsFullscreen() const { return m_fullscreen; }

		SDL_Window *m_window;

		SDL_Cursor *currentCursor = nullptr;

		// Window Input State
		Map<SDL_Scancode, uint8> m_keyStates;
		KeyMap m_keyMapping;
		ModifierKeys m_modKeys = ModifierKeys::None;

		// Gamepad input
		Map<int32, Ref<Gamepad_Impl>> m_gamepads;
		Map<SDL_JoystickID, Gamepad_Impl *> m_joystickMap;

		// Text input / IME stuff
		TextComposition m_textComposition;

		// Various window state
		bool m_active = true;
		bool m_closed = false;

		bool m_fullscreen = false;
		bool m_windowedFullscreen = false;

		uint32 m_style;
		Vector2i m_clntSize;
		WString m_caption;
	};

	Window::Window(Vector2i size, uint8 samplecount)
	{
		m_impl = new Window_Impl(*this, size, samplecount);
	}
	Window::~Window()
	{
		delete m_impl;
	}
	void Window::Show()
	{
		m_impl->Show();
	}
	void Window::Hide()
	{
		m_impl->Hide();
	}
	bool Window::Update()
	{
		return m_impl->Update();
	}
	void *Window::Handle()
	{
		return m_impl->m_window;
	}
	void Window::SetCaption(const WString &cap)
	{
		m_impl->SetCaption(cap);
	}
	void Window::Close()
	{
		m_impl->m_closed = true;
	}

	Vector2i Window::GetMousePos()
	{
		Vector2i res;
		SDL_GetMouseState(&res.x, &res.y);
		return res;
	}
	void Window::SetCursor(Ref<class ImageRes> image, Vector2i hotspot /*= Vector2i(0,0)*/)
	{
		m_impl->SetCursor(image, hotspot);
	}
	void Window::SetCursorVisible(bool visible)
	{
		SDL_ShowCursor(visible);
	}

	void Window::SetWindowStyle(WindowStyle style)
	{
		m_impl->SetWindowStyle(style);
	}

	Vector2i Window::GetWindowPos() const
	{
		return m_impl->GetWindowPos();
	}

	Vector2i Window::GetWindowSize() const
	{
		return m_impl->GetWindowSize();
	}

	void Window::SetVSync(int8 setting)
	{
		m_impl->SetVSync(setting);
	}

	void Window::SetPosAndShape(const PosAndShape& posAndShape, bool ensureInBound)
	{
		m_impl->SetPosAndShape(posAndShape, ensureInBound);
	}

	bool Window::IsFullscreen() const
	{
		return m_impl->IsFullscreen();
	}

	int Window::GetDisplayIndex() const
	{
		return SDL_GetWindowDisplayIndex(m_impl->m_window);
	}

	bool Window::IsKeyPressed(SDL_Scancode key) const
	{
		return m_impl->m_keyStates[key] > 0;
	}

	Graphics::ModifierKeys Window::GetModifierKeys() const
	{
		return m_impl->m_modKeys;
	}

	bool Window::IsActive() const
	{
		return SDL_GetWindowFlags(m_impl->m_window) & SDL_WindowFlags::SDL_WINDOW_INPUT_FOCUS;
	}

	void Window::StartTextInput()
	{
		SDL_StartTextInput();
	}
	void Window::StopTextInput()
	{
		SDL_StopTextInput();
	}
	const Graphics::TextComposition &Window::GetTextComposition() const
	{
		return m_impl->m_textComposition;
	}

	void Window::ShowMessageBox(String title, String message, int severity)
	{
		m_impl->ShowMessageBox(title, message, severity);
	}

	bool Window::ShowYesNoMessage(String title, String message)
	{
		return m_impl->ShowYesNoMessage(title, message);
	}

	String Window::GetClipboard() const
	{
		char *utf8Clipboard = SDL_GetClipboardText();
		String ret(utf8Clipboard);
		SDL_free(utf8Clipboard);

		return ret;
	}

	int32 Window::GetNumGamepads() const
	{
		return SDL_NumJoysticks();
	}
	Vector<String> Window::GetGamepadDeviceNames() const
	{
		Vector<String> ret;
		uint32 numJoysticks = SDL_NumJoysticks();
		for (uint32 i = 0; i < numJoysticks; i++)
		{
			SDL_Joystick *joystick = SDL_JoystickOpen(i);
			if (!joystick)
			{
				continue;
			}
			String deviceName = SDL_JoystickName(joystick);
			ret.Add(deviceName);

			SDL_JoystickClose(joystick);
		}
		return ret;
	}

	Ref<Gamepad> Window::OpenGamepad(int32 deviceIndex)
	{
		Ref<Gamepad_Impl> *openGamepad = m_impl->m_gamepads.Find(deviceIndex);
		if (openGamepad)
			return Utility::CastRef<Gamepad_Impl, Gamepad>(*openGamepad);
		Ref<Gamepad_Impl> newGamepad;

		Gamepad_Impl *gamepadImpl = new Gamepad_Impl();
		// Try to initialize new device
		if (gamepadImpl->Init(this, deviceIndex))
		{
			newGamepad = Ref<Gamepad_Impl>(gamepadImpl);

			// Receive joystick events
			SDL_JoystickEventState(SDL_ENABLE);
		}
		else
		{
			delete gamepadImpl;
		}
		if (newGamepad)
		{
			m_impl->m_gamepads.Add(deviceIndex, newGamepad);
			m_impl->m_joystickMap.Add(SDL_JoystickInstanceID(gamepadImpl->m_joystick), gamepadImpl);
		}
		return Utility::CastRef<Gamepad_Impl, Gamepad>(newGamepad);
	}

	void Window::SetMousePos(const Vector2i &pos)
	{
		SDL_WarpMouseInWindow(m_impl->m_window, pos.x, pos.y);
	}

	void Window::SetRelativeMouseMode(bool enabled)
	{
		if (SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE) != 0)
			Logf("SetRelativeMouseMode failed: %s", Logger::Severity::Warning, SDL_GetError());
	}

	bool Window::GetRelativeMouseMode()
	{
		return SDL_GetRelativeMouseMode() == SDL_TRUE;
	}
} // namespace Graphics

namespace Graphics
{
	ImplementBitflagEnum(ModifierKeys);
}
