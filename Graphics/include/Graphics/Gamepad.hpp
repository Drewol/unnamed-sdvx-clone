#pragma once

/*
	Gamepad Abstraction
*/
class Gamepad
{
public:
	virtual ~Gamepad() = default;

	virtual bool GetButton(uint8 button) const = 0;
	virtual float GetAxis(uint8 idx) const = 0;

	virtual uint32 NumButtons() const = 0;
	virtual uint32 NumAxes() const = 0;

	// Gamepad button event
	Delegate<uint8, int32> OnButtonPressed;
	// Gamepad button event
	Delegate<uint8, int32> OnButtonReleased;
};
