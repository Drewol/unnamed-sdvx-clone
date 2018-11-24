#pragma once

#include "stdafx.h"
#include "lua.hpp"

#include <functional>

class Scriptable
{
protected:
	lua_State* L;

	virtual void m_InitScriptState() {};

	template<typename T>
	std::function<int(lua_State*)> m_Bind(int(T::*member)(lua_State*))
	{
		T* thisAsT = static_cast<T*>(this);
		return std::bind(member, thisAsT, std::placeholders::_1);
	}

	void m_RegisterMemberFunction(const char* memberName, std::function<int(lua_State*)> f);
	void m_CreateGlobalObject(const char* globalName);

	friend class Application;

private:
	Map<String, std::function<int(lua_State*)>> m_functions;

	friend int lIndexFunction(lua_State* L);
};
