#pragma once
#include "ApplicationTickable.hpp"

/*
	Simple test scene that can be freely used when developing new graphics features or other things
	this scene is used when -test is provided at application startup
*/
class Test : public IApplicationTickable
{
protected:
	Test() = default;
public:
	virtual ~Test() = default;
	static Test* Create();
};
