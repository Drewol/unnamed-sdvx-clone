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
	m_window->OnMouseMotion.Add(this, &Input::OnMouseMotion);


	m_lastMousePos[0] = m_window->GetMousePos().x;
	m_lastMousePos[1] = m_window->GetMousePos().y;

	m_laserDevice = g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::LaserInputDevice);
	m_buttonDevice = g_gameConfig.GetEnum<Enum_InputDevice>(GameConfigKeys::ButtonInputDevice);

	m_keySensitivity = g_gameConfig.GetFloat(GameConfigKeys::Key_Sensitivity);
	m_keyLaserReleaseTime = g_gameConfig.GetFloat(GameConfigKeys::Key_LaserReleaseTime);

	m_mouseAxisMapping[0] = g_gameConfig.GetInt(GameConfigKeys::Mouse_Laser0Axis);
	m_mouseAxisMapping[1] = g_gameConfig.GetInt(GameConfigKeys::Mouse_Laser1Axis);

	m_mouseSensitivity = CalculateRealMouseSens(g_gameConfig.GetFloat(GameConfigKeys::Mouse_Sensitivity));

	m_controllerAxisMapping[0] = g_gameConfig.GetInt(GameConfigKeys::Controller_Laser0Axis);
	m_controllerAxisMapping[1] = g_gameConfig.GetInt(GameConfigKeys::Controller_Laser1Axis);
	m_controllerSensitivity = g_gameConfig.GetFloat(GameConfigKeys::Controller_Sensitivity);
	m_controllerDeadzone = g_gameConfig.GetFloat(GameConfigKeys::Controller_Deadzone);
	m_controllerDirectMode = g_gameConfig.GetBool(GameConfigKeys::Controller_DirectMode);
	m_backComboHold = g_gameConfig.GetEnum<Enum_ButtonComboModeSettings>(GameConfigKeys::UseBackCombo) == ButtonComboModeSettings::Hold;
	m_backComboInstant = g_gameConfig.GetEnum<Enum_ButtonComboModeSettings>(GameConfigKeys::UseBackCombo) == ButtonComboModeSettings::Instant;

	switch (g_gameConfig.GetEnum<Enum_LaserAxisOption>(GameConfigKeys::InvertLaserInput)) {

	case LaserAxisOption::Left:
		m_laserDirections[0] = -1.0f;
		m_laserDirections[1] = 1.0f;
		break;
	case LaserAxisOption::Right:
		m_laserDirections[0] = 1.0f;
		m_laserDirections[1] = -1.0f;
		break;
	case LaserAxisOption::Both:
		m_laserDirections[0] = -1.0f;
		m_laserDirections[1] = -1.0f;
		break;
	default:
		m_laserDirections[0] = 1.0f;
		m_laserDirections[1] = 1.0f;
		break;
	}


	// Init controller mapping
	if(m_laserDevice == InputDevice::Controller || m_buttonDevice == InputDevice::Controller)
	{
		const auto deviceId = g_gameConfig.GetBlob<16>(GameConfigKeys::Controller_DeviceID);
		int deviceIndex = -1;

		for (int32 i = 0; i < m_window->GetNumGamepads(); i++)
		{
			auto id = SDL_JoystickGetDeviceGUID(i);
			if (memcmp(deviceId.data(), id.data, 16) == 0)
			{
				deviceIndex = i; //always set device index since it is a match, but check input count in case of multiple matches
				auto joystick = SDL_JoystickOpen(i);

				int numInputs = 0;
				numInputs += SDL_JoystickNumAxes(joystick);
				numInputs += SDL_JoystickNumButtons(joystick);
				numInputs += SDL_JoystickNumHats(joystick);

				SDL_JoystickClose(joystick);
				if (numInputs > 0)
					break;
			}
		}



		if(deviceIndex < 0)
		{
			Log("Could not find bound gamepad", Logger::Severity::Warning);
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
		m_gamepad.reset();
	}
	if(m_window)
	{
		m_window->OnKeyPressed.RemoveAll(this);
		m_window->OnKeyReleased.RemoveAll(this);
		m_window->OnMouseMotion.RemoveAll(this);
		m_window = nullptr;
	}
}

void Input::Update(float deltaTime)
{
	for(auto it = m_mouseLocks.begin(); it != m_mouseLocks.end();)
	{
		if(it->use_count() == 1)
		{
			it = m_mouseLocks.erase(it);
			continue;
		}
		it++;
	}

	if(!m_mouseLocks.empty())
	{
		if (!m_window->GetRelativeMouseMode())
			m_window->SetRelativeMouseMode(true);
	}
	else if (m_window->GetRelativeMouseMode())
	{
		m_window->SetRelativeMouseMode(false);
	}

	if(m_laserDevice == InputDevice::Mouse)
	{
		for(uint32 i = 0; i < 2; i++)
		{
			if(m_mouseAxisMapping[i] > 1)
			{
				// INVALID MAPPING
				m_laserStates[i] = 0.0f;
				m_rawKeyLaserStates[i] = 0.0f;
				continue;
			}

			m_laserStates[i] = m_mouseSensitivity * m_mousePos[m_mouseAxisMapping[i]];
			m_rawLaserStates[i] = m_mousePos[m_mouseAxisMapping[i]];
			m_mousePos[m_mouseAxisMapping[i]] = 0;
		}
	}

	if(m_laserDevice == InputDevice::Keyboard)
	{
		for(uint32 i = 0; i < 2; i++)
		{
			m_laserStates[i] = m_rawKeyLaserStates[i] * deltaTime;

			// if neither laser button is being held fade out the laser input
			if (!m_buttonStates[(int32)Button::LS_0Neg + i * 2 + 1] && !m_buttonStates[(int32)Button::LS_0Neg + i * 2])
			{
				if (m_keyLaserReleaseTime != 0.f)
				{
					float reduction = m_keySensitivity * deltaTime / m_keyLaserReleaseTime;
					if (reduction > fabs(m_rawKeyLaserStates[i]))
						m_rawKeyLaserStates[i] = 0.f;
					else
						m_rawKeyLaserStates[i] -= reduction * Math::Sign(m_rawKeyLaserStates[i]);
				}
				else
				{
					m_rawKeyLaserStates[i] = 0.f;
				}
			}
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
				float delta = axisState;
				if (!m_controllerDirectMode)
					delta -= m_prevLaserStates[i];
				if (fabs(delta) > 1.5f)
					delta += 2 * (Math::Sign(delta) * -1);
				if (fabs(delta) < m_controllerDeadzone)
				{
					m_laserStates[i] = 0.0f;
					m_rawLaserStates[i] = 0.0f;
				}
				else
				{
					m_rawLaserStates[i] = delta;
					m_laserStates[i] = delta * m_controllerSensitivity;
				}
				m_prevLaserStates[i] = axisState;
			}
		}
	}

	for (size_t i = 0; i < 2; i++)
	{
		m_laserStates[i] *= m_laserDirections[i];
		m_absoluteLaserStates[i] = fmodf(m_absoluteLaserStates[i] + m_laserStates[i], Math::pi * 2);
	}


	//back combo checks
	if (m_backComboHold && m_buttonStates[(size_t)Button::BT_S] && Are3BTsHeld())
	{
		m_comboHoldTimer += deltaTime;
		if (m_comboHoldTimer >= 0.5 && !m_backSent)
		{
			OnButtonPressed.Call(Button::Back, 0);
			m_backSent = true;
		}
	}
	else
	{
		m_comboHoldTimer = 0.0f;
		m_backSent = false;
	}
}

bool Input::GetButton(Button button) const
{
	return m_buttonStates[(size_t)button];
}

float Input::GetAbsoluteLaser(int laser) const
{
	return m_absoluteLaserStates[laser];
}

bool Input::Are3BTsHeld() const
{
	bool bta = GetButton(Input::Button::BT_0);
	bool btb = GetButton(Input::Button::BT_1);
	bool btc = GetButton(Input::Button::BT_2);
	bool btd = GetButton(Input::Button::BT_3);

	return (bta && btb && btc) || (bta && btb && btd) || (bta && btc && btd) || (btb && btc && btd);
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
float Input::GetAbsoluteInputLaserDir(uint32 laserIdx)
{
	return m_rawLaserStates[laserIdx];
}

//To give the sensitivity slider a good feel and put reasonable sensitivities in a reasonable range.
const double Input::mouseSensVars[] = { 20.0, 0.1, 1.2 };

double Input::CalculateSensFromPpr(double ppr)
{
	double pprSign = Math::Sign(ppr);

	return pprSign * (mouseSensVars[0] / mouseSensVars[1]) / pow(fabs(ppr), 1.0 / mouseSensVars[2]);
}
double Input::CalculateRealMouseSens(double sensSetting)
{
	double sensSign = Math::Sign(sensSetting);
	double ppr = EstimatePprFromSens(abs(sensSetting));
	return sensSign * 6.0 / (ppr);
}
double Input::EstimatePprFromSens(double sens)
{
	return pow(mouseSensVars[0] / (fmax(sens, 0.001) * mouseSensVars[1]), mouseSensVars[2]);
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

		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_FX0), Button::FX_0);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_FX1), Button::FX_1);

		// Alternate button mappings
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_BTSAlt), Button::BT_S);

		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_BT0Alt), Button::BT_0);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_BT1Alt), Button::BT_1);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_BT2Alt), Button::BT_2);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_BT3Alt), Button::BT_3);

		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_FX0Alt), Button::FX_0);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_FX1Alt), Button::FX_1);
	}

	//Always bind back button
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Back), Button::Back);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_BackAlt), Button::Back);

	if(m_laserDevice == InputDevice::Keyboard)
	{
		// Laser button mappings
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Laser0Neg), Button::LS_0Neg);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Laser0Pos), Button::LS_0Pos);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Laser1Neg), Button::LS_1Neg);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Laser1Pos), Button::LS_1Pos);

		// Alternate laser button mappings
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Laser0NegAlt), Button::LS_0Neg);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Laser0PosAlt), Button::LS_0Pos);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Laser1NegAlt), Button::LS_1Neg);
		m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Laser1PosAlt), Button::LS_1Pos);
	}

	// Always bind auxillary controls
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_SongSelect_Up), Button::SongSelect_Up);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_SongSelect_Down), Button::SongSelect_Down);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_SongSelect_FastUp), Button::SongSelect_FastUp);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_SongSelect_FastDown), Button::SongSelect_FastDown);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_SongSelect_Easier), Button::SongSelect_Easier);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_SongSelect_Harder), Button::SongSelect_Harder);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_SongSelect_Collections), Button::SongSelect_Collections);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_SongSelect_Random), Button::SongSelect_Random);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_SongSelect_ReloadSongs), Button::SongSelect_ReloadSongs);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_SongSelect_Demo), Button::SongSelect_Demo);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_SongSelect_OpenEditor), Button::SongSelect_OpenEditor);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_SongSelect_OpenDirectory), Button::SongSelect_OpenDirectory);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_SongSelect_OpenSearch), Button::SongSelect_OpenSearch);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_SongSelect_CloseSearch), Button::SongSelect_CloseSearch);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_SongSelect_StartPractice), Button::SongSelect_StartPractice);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Game_Bailout), Button::Game_Bailout);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Game_Pause), Button::Game_Pause);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Game_SkipIntro), Button::Game_SkipIntro);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Game_Advance), Button::Game_Advance);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Game_Restart), Button::Game_Restart);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Game_DebugHUD), Button::Game_DebugHUD);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Result_Continue), Button::Result_Continue);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_Result_Screenshot), Button::Result_Screenshot);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_ReloadSkin), Button::ReloadSkin);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_ScrollLeft), Button::ScrollLeft);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_ScrollRight), Button::ScrollRight);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_ScrollUp), Button::ScrollUp);
	m_buttonMap.Add(g_gameConfig.GetInt(GameConfigKeys::Key_ScrollDown), Button::ScrollDown);
}

void Input::m_InitControllerMapping()
{
	m_controllerMap.clear();
	if(m_buttonDevice == InputDevice::Controller)
	{
		m_controllerMap.Add(g_gameConfig.GetInt(GameConfigKeys::Controller_BTS), Button::BT_S);
		m_controllerMap.Add(g_gameConfig.GetInt(GameConfigKeys::Controller_BT0), Button::BT_0);
		m_controllerMap.Add(g_gameConfig.GetInt(GameConfigKeys::Controller_BT1), Button::BT_1);
		m_controllerMap.Add(g_gameConfig.GetInt(GameConfigKeys::Controller_BT2), Button::BT_2);
		m_controllerMap.Add(g_gameConfig.GetInt(GameConfigKeys::Controller_BT3), Button::BT_3);
		m_controllerMap.Add(g_gameConfig.GetInt(GameConfigKeys::Controller_FX0), Button::FX_0);
		m_controllerMap.Add(g_gameConfig.GetInt(GameConfigKeys::Controller_FX1), Button::FX_1);
		m_controllerMap.Add(g_gameConfig.GetInt(GameConfigKeys::Controller_Back), Button::Back);
	}
}

void Input::m_OnButtonInput(Button b, bool pressed, int32 delta)
{
	bool& state = m_buttonStates[(size_t)b];
	if(state != pressed)
	{
		state = pressed;
		if(state)
		{
			if (b == Button::BT_S && m_backComboInstant && Are3BTsHeld())
			{
				OnButtonPressed.Call(Button::Back, delta);
			}
			else if (b == Button::BT_S && m_backComboHold && Are3BTsHeld());
			else
			{
				OnButtonPressed.Call(b, delta);
			}
		}
		else
		{
			OnButtonReleased.Call(b, delta);
		}
	}

	static Timer t;
	if(b >= Button::LS_0Neg && b <= Button::LS_1Pos)
	{
		int32 btnIdx = (int32)b - (int32)Button::LS_0Neg;
		int32 laserIdx = btnIdx / 2;
		// Set laser state based uppon the button that was pressed last
		if(pressed)
			m_rawKeyLaserStates[laserIdx] = (btnIdx % 2) == 0 ? -m_keySensitivity : m_keySensitivity;
		else // If a button was released check if the other one is still held
		{
			if(m_buttonStates[(int32)Button::LS_0Neg + laserIdx * 2])
				m_rawKeyLaserStates[laserIdx] = -m_keySensitivity;
			else if(m_buttonStates[(int32)Button::LS_0Neg + laserIdx * 2 + 1])
				m_rawKeyLaserStates[laserIdx] = m_keySensitivity;

		}
	}
}

void Input::m_OnGamepadButtonPressed(uint8 button, int32 delta)
{
	// Handle button mappings
	auto it = m_controllerMap.equal_range(button);
	for(auto it1 = it.first; it1 != it.second; it1++)
		m_OnButtonInput(it1->second, true, delta);
}
void Input::m_OnGamepadButtonReleased(uint8 button, int32 delta)
{
	// Handle button mappings
	auto it = m_controllerMap.equal_range(button);
	for(auto it1 = it.first; it1 != it.second; it1++)
		m_OnButtonInput(it1->second, false, delta);
}

void Input::OnKeyPressed(SDL_Scancode code, int32 delta)
{
	// Handle button mappings
	auto it = m_buttonMap.equal_range(static_cast<int32>(code));
	for(auto it1 = it.first; it1 != it.second; it1++)
		m_OnButtonInput(it1->second, true, delta);
}
void Input::OnKeyReleased(SDL_Scancode code, int32 delta)
{
	// Handle button mappings
	auto it = m_buttonMap.equal_range(static_cast<int32>(code));
	for(auto it1 = it.first; it1 != it.second; it1++)
		m_OnButtonInput(it1->second, false, delta);
}

void Input::OnMouseMotion(int32 x, int32 y)
{
	m_mousePos[0] += x;
	m_mousePos[1] += y;
}
