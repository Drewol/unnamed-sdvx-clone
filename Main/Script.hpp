#pragma once

class Script
{
protected:
	Script() = default;

public:
	virtual ~Script() = default;
	static Script* Create(const String& scriptPath);
};

