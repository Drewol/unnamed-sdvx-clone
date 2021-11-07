#include "stdafx.h"
#include "Gamepad_Impl.hpp"
#include "Window.hpp"

namespace Graphics
{

	Gamepad_Impl::~Gamepad_Impl()
	{
		SDL_JoystickClose(m_joystick);
	}
	bool Gamepad_Impl::Init(Window* window, uint32 deviceIndex)
	{
		Logf("Trying to open joystick %d", Logger::Severity::Info, deviceIndex);

		m_window = window;
		m_deviceIndex = deviceIndex;
		m_joystick = SDL_JoystickOpen(deviceIndex);
		if(!m_joystick)
		{
			Logf("Failed to open joystick %d", Logger::Severity::Error, deviceIndex);
			return false;
		}

		for(int32 i = 0; i < SDL_JoystickNumButtons(m_joystick); i++)
			m_buttonStates.Add(0);
		for(int32 i = 0; i < SDL_JoystickNumAxes(m_joystick); i++)
			m_axisState.Add(0.0f);

		String deviceName = SDL_JoystickName(m_joystick);
		Logf("Joystick device \"%s\" opened with %d buttons and %d axes", Logger::Severity::Info,
			deviceName, m_buttonStates.size(), m_axisState.size());

		return true;
	}

	void Gamepad_Impl::HandleInputEvent(uint32 buttonIndex, uint8 newState, int32 delta)
	{
		m_buttonStates[buttonIndex] = newState;
		if(newState != 0)
			OnButtonPressed.Call(buttonIndex, delta);
		else
			OnButtonReleased.Call(buttonIndex, delta);
	}
	void Gamepad_Impl::HandleAxisEvent(uint32 axisIndex, int16 newValue)
	{
		m_axisState[axisIndex] = (float)newValue / (float)0x7fff;
	}
	void Gamepad_Impl::HandleHatEvent(uint32 hadIndex, uint8 newValue)
	{
		// TODO Maybe use this if required
	}

	bool Gamepad_Impl::GetButton(uint8 button) const
	{
		if(button >= m_buttonStates.size())
			return false;
		return m_buttonStates[button] != 0;
	}
	float Gamepad_Impl::GetAxis(uint8 idx) const
	{
		if(idx >= m_axisState.size())
			return 0.0f;
		return m_axisState[idx];
	}
	uint32 Gamepad_Impl::NumButtons() const
	{
		return (uint32)m_buttonStates.size();
	}
	uint32 Gamepad_Impl::NumAxes() const
	{
		return (uint32)m_axisState.size();
	}
}