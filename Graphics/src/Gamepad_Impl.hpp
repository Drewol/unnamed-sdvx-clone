#pragma once
#include "Gamepad.hpp"

#include "SDL2/SDL_joystick.h"

namespace Graphics
{
	class Window;
	class Gamepad_Impl : public Gamepad
	{
	public:
		~Gamepad_Impl();
		bool Init(Graphics::Window* window, uint32 deviceIndex);

		// Handles input events straight from the event loop
		void HandleInputEvent(uint32 buttonIndex, uint8 newState, int32 delta);
		void HandleAxisEvent(uint32 axisIndex, int16 newValue);
		void HandleHatEvent(uint32 hadIndex, uint8 newValue);

		class Window* m_window;
		uint32 m_deviceIndex;
		SDL_Joystick* m_joystick;

		Vector<float> m_axisState;
		Vector<uint8> m_buttonStates;

		virtual bool GetButton(uint8 button) const override;
		virtual float GetAxis(uint8 idx) const override;
		virtual uint32 NumButtons() const override;
		virtual uint32 NumAxes() const override;
	};
}