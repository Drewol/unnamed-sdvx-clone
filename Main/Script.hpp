#pragma once
#include "AsyncLoadable.hpp"

class Script : public IAsyncLoadable
{
protected:
	Script() = default;

public:
	virtual ~Script() = default;
	static Script* Create(const String& scriptPath);
};

