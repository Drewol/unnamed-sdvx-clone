#pragma once
#include "ApplicationTickable.hpp"

class TitleScreen : public IApplicationTickable
{
protected:
	TitleScreen() = default;
public:
	virtual ~TitleScreen() = default;
	static TitleScreen* Create();
private:
	void m_mousePressed(int32 button);
};
