#include "stdafx.h"
#include "Input.hpp"
#include "GameConfig.hpp"

Input::~Input()
{
	// Shoud be set to null by Cleanup
	assert(!m_gamepad);
	assert(!m_window);
}

void Input::Init(Graphics::Window& wnd)
{
	Cleanup();
	m_window = &wnd;
	m_window->OnKeyPressed.Add(this, &Input::OnKeyPressed);
	m_window->OnKeyReleased.Add(this, &Input::OnKeyReleased);

	m_lastMousePos = m_window->GetMousePos();

	m_laserDevice = g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice);
	m_buttonDevice = g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice);

	m_keySensitivity = g_gameConfig.GetFloat(GameConfigKeys::Key_Sensitivity);

	m_mouseAxisMapping[0] = g_gameConfig.GetInt(GameConfigKeys::Mouse_Laser0Axis);
	m_mouseAxisMapping[1] = g_gameConfig.GetInt(GameConfigKeys::Mouse_Laser1Axis);
	m_mouseSensitivity = g_gameConfig.GetFloat(GameConfigKeys::Mouse_Sensitivity);

	m_controllerAxisMapping[0] = g_gameConfig.GetInt(GameConfigKeys::Controller_Laser0Axis);
	m_controllerAxisMapping[1] = g_gameConfig.GetInt(GameConfigKeys::Controller_Laser1Axis);
	m_controllerSensitivity = g_gameConfig.GetFloat(GameConfigKeys::Controller_Sensitivity);
	m_controllerDeadzone = g_gameConfig.GetFloat(GameConfigKeys::Controller_Deadzone);

	// Init controller mapping
	if(m_laserDevice == InputDevice::Controller || m_buttonDevice == InputDevice::Controller)
	{
		int32 deviceIndex = g_gameConfig.GetInt(GameConfigKeys::Controller_DeviceID);
		if(deviceIndex >= m_window->GetNumGamepads())
		{
			Logf("Out of range controller [%d], number of available controllers is %d", Logger::Error, deviceIndex, m_window->GetNumGamepads());
		}
		else
		{
			m_gamepad = m_window->OpenGamepad(deviceIndex);
			if(m_gamepad)
			{
				m_gamepad->OnButtonPressed.Add(this, &Input::m_OnGamepadButtonPressed);
				m_gamepad->OnButtonReleased.Add(this, &Input::m_OnGamepadButtonReleased);
			}
		}
		m_InitControllerMapping();
	}

	// Init keyboard mapping
	m_InitKeyboardMapping();
}
void Input::Cleanup()
{
	if(m_gamepad)
	{
		m_gamepad->OnButtonPressed.RemoveAll(this);
		m_gamepad->OnButtonReleased.RemoveAll(this);
		m_gamepad.Release();
	}
	if(m_window)
	{
		m_window->OnKeyPressed.RemoveAll(this);
		m_window->OnKeyReleased.RemoveAll(this);
		m_window = nullptr;
	}
}

void Input::Update(float deltaTime)
{
	for(auto it = m_mouseLocks.begin(); it != m_mouseLocks.end();)
	{
		if(it->GetRefCount() == 1)
		{
			it = m_mouseLocks.erase(it);
			continue;
		}
		it++;
	}

	Vector2i mousePos = m_window->GetMousePos();
	Vector2 mouseDelta = mousePos - m_lastMousePos;
	m_lastMousePos = mousePos;
	float* mouseAxes = &mouseDelta.x;

	// Reset mouse to edge of screen
	if(!m_mouseLocks.empty())
	{
		Vector2 mouseCenter = m_window->GetWindowSize() / 2;
		m_window->SetMousePos(mouseCenter);
		m_lastMousePos = mouseCenter;
	}
	
	if(m_laserDevice == InputDevice::Mouse)
	{
		for(uint32 i = 0; i < 2; i++)
		{
			if(m_mouseAxisMapping[i] < 0 || m_mouseAxisMapping[i] > 1)
			{
				// INVALID MAPPING
				m_laserStates[i] = 0.0f;
				continue;
			}
			
			m_laserStates[i] = mouseAxes[m_mouseAxisMapping[i]] * m_mouseSensitivity;
		}
	}


	if(m_gamepad)
	{
		// Poll controller laser input
		if(m_laserDevice == InputDevice::Controller)
		{
			for(uint32 i = 0; i < 2; i++)
			{
				float axisState = m_gamepad->GetAxis(m_controllerAxisMapping[i]);
				float delta = axisState - m_prevLaserStates[i];
				if (fabs(delta) > 1.5f)
					delta += 2 * (Math::Sign(delta) * -1);
				if (fabs(delta) < m_controllerDeadzone)
					m_laserStates[i] = 0.0f;
				else
					m_laserStates[i] = delta * m_controllerSensitivity;
				m_prevLaserStates[i] = axisState;
			}
		}
	}
}

bool Input::GetButton(Button button) const
{
	return m_buttonStates[(size_t)button];
}

String Input::GetControllerStateString() const
{
	if(m_gamepad)
	{
		String s = "Buttons\n";
		for(uint32 i = 0; i < m_gamepad->NumButtons(); i++)
		{
			s += Utility::Sprintf("  [%d]%d\n", i, m_gamepad->GetButton(i));
		}
		s += "\nAxes\n";
		for(uint32 i = 0; i < m_gamepad->NumAxes(); i++)
		{
			s += Utility::Sprintf("  [%d]%.2f\n", i, m_gamepad->GetAxis(i));
		}
		for (uint32 i = 0; i < 2; i++)
		{
			s += Utility::Sprintf("Delta for knob %d: %.2f\n", i, m_laserStates[i]);
		}
		return s;
	}
	return String();
}

Ref<int32> Input::LockMouse()
{
	return m_mouseLocks.Add(MouseLockHandle(new int32(m_mouseLockIndex++)));
}

float Input::GetInputLaserDir(uint32 laserIdx)
{
	return m_laserStates[laserIdx];
}
void Input::m_InitKeyboardMapping()
{
	memset(m_buttonStates, 0, sizeof(m_buttonStates));
	m_buttonMap.clear();

	if(m_buttonDevice == InputDevice::Keyboard)
	{
		// Button mappings
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_BTS), Button::BT_S);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_BT0), Button::BT_0);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_BT1), Button::BT_1);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_BT2), Button::BT_2);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_BT3), Button::BT_3);
		// Alternate button mappings
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_BT0Alt), Button::BT_0);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_BT1Alt), Button::BT_1);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_BT2Alt), Button::BT_2);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_BT3Alt), Button::BT_3);

		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_FX0), Button::FX_0);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_FX1), Button::FX_1);
		// Alternate button mappings
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_FX0Alt), Button::FX_0);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_FX1Alt), Button::FX_1);
	}

	if(m_laserDevice == InputDevice::Keyboard)
	{
		// Laser button mappings
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Laser0Neg), Button::LS_0Neg);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Laser0Pos), Button::LS_0Pos);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Laser1Neg), Button::LS_1Neg);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Laser1Pos), Button::LS_1Pos);
	}
}

void Input::m_InitControllerMapping()
{
	if(m_buttonDevice == InputDevice::Controller)
	{
		m_controllerMap.Add(g_gameConfig.GetInt(GameConfigKeys::Controller_BTS), Button::BT_S);
		m_controllerMap.Add(g_gameConfig.GetInt(GameConfigKeys::Controller_BT0), Button::BT_0);
		m_controllerMap.Add(g_gameConfig.GetInt(GameConfigKeys::Controller_BT1), Button::BT_1);
		m_controllerMap.Add(g_gameConfig.GetInt(GameConfigKeys::Controller_BT2), Button::BT_2);
		m_controllerMap.Add(g_gameConfig.GetInt(GameConfigKeys::Controller_BT3), Button::BT_3);
		m_controllerMap.Add(g_gameConfig.GetInt(GameConfigKeys::Controller_FX0), Button::FX_0);
		m_controllerMap.Add(g_gameConfig.GetInt(GameConfigKeys::Controller_FX1), Button::FX_1);
	}
}

void Input::m_OnButtonInput(Button b, bool pressed)
{
	bool& state = m_buttonStates[(size_t)b];
	if(state != pressed)
	{
		state = pressed;
		if(state)
		{
			OnButtonPressed.Call(b);
		}
		else
		{
			OnButtonReleased.Call(b);
		}
	}

	static Timer t;
	if(b >= Button::LS_0Neg)
	{
		int32 btnIdx = (int32)b - (int32)Button::LS_0Neg;
		int32 laserIdx = btnIdx / 2;
		// Set laser state based uppon the button that was pressed last
		if(pressed)
			m_laserStates[laserIdx] = (btnIdx % 2) == 0 ? -1.0f : 1.0f;
		else // If a button was released check if the other one is still held
		{
			if(m_buttonStates[(int32)Button::LS_0Neg + laserIdx * 2])
				m_laserStates[laserIdx] = -1;
			else if(m_buttonStates[(int32)Button::LS_0Neg + laserIdx * 2 + 1])
				m_laserStates[laserIdx] = 1;
			else
				m_laserStates[laserIdx] = 0;
		}
		m_laserStates[laserIdx] *= m_keySensitivity;
	}
}

void Input::m_OnGamepadButtonPressed(uint8 button)
{
	// Handle button mappings
	auto it = m_controllerMap.equal_range(button);
	for(auto it1 = it.first; it1 != it.second; it1++)
		m_OnButtonInput(it1->second, true);
}
void Input::m_OnGamepadButtonReleased(uint8 button)
{
	// Handle button mappings
	auto it = m_controllerMap.equal_range(button);
	for(auto it1 = it.first; it1 != it.second; it1++)
		m_OnButtonInput(it1->second, false);
}

void Input::OnKeyPressed(int32 key)
{
	// Handle button mappings
	auto it = m_buttonMap.equal_range(key);
	for(auto it1 = it.first; it1 != it.second; it1++)
		m_OnButtonInput(it1->second, true);
}
void Input::OnKeyReleased(int32 key)
{
	// Handle button mappings
	auto it = m_buttonMap.equal_range(key);
	for(auto it1 = it.first; it1 != it.second; it1++)
		m_OnButtonInput(it1->second, false);
}
