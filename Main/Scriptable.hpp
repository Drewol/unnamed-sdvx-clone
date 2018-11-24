#pragma once

#include "lua.hpp"

class Scriptable
{
public:
	virtual void InitScriptState(lua_State* L) {};
};
