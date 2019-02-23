#include "Scriptable.hpp"

int lMemberCallFunction(lua_State* L)
{
	std::function<int(lua_State*)>** t = static_cast<std::function<int(lua_State*)>**>(luaL_checkudata(L, 1, "Scriptable_Callback"));
	return (**t)(L);
}

int lIndexFunction(lua_State* L)
{
	Scriptable** t = static_cast<Scriptable**>(luaL_checkudata(L, 1, "Scriptable"));
	String lookup = luaL_checkstring(L, 2);

	if (luaL_newmetatable(L, "Scriptable_Callback") == 1)
	{
		lua_pushcfunction(L, lMemberCallFunction);
		lua_setfield(L, -2, "__call");
	}

	std::function<int(lua_State*)>** ud = static_cast<std::function<int(lua_State*)>**>(lua_newuserdata(L, sizeof(std::function<int(lua_State*)>*)));
	*(ud) = &(*t)->m_functions[lookup];

	luaL_setmetatable(L, "Scriptable_Callback");

	return 1;
}

void Scriptable::m_RegisterMemberFunction(const char* memberName, std::function<int(lua_State*)> f)
{
	m_functions[memberName] = f;
}

void Scriptable::m_CreateGlobalObject(const char* globalName)
{
	if (luaL_newmetatable(L, "Scriptable") == 1)
	{
		lua_pushcfunction(L, lIndexFunction);
		lua_setfield(L, -2, "__index");
	}

	Scriptable** ud = static_cast<Scriptable**>(lua_newuserdata(L, sizeof(Scriptable*)));
	*(ud) = this;

	luaL_setmetatable(L, "Scriptable");
	lua_setglobal(L, globalName);
}