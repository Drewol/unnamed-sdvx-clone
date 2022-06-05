#pragma once
#include "lua.hpp"
#include "Bindable.hpp"
#include "String.hpp"
#include "Map.hpp"

static int lMemberCallFunction(lua_State* L);
static int lIndexFunction(lua_State* L);

class LuaBindable
{
public:
	Map<String, IFunctionBinding<int, lua_State*>*> Bindings;
	LuaBindable(lua_State* L, String name) : m_name(name)
	{
		m_lua = L;
	}
	~LuaBindable()
	{
		for (auto b : Bindings)
		{
			delete b.second;
		}
	}

	template<typename Class>
	void AddFunction(String name, Class* object, int (Class::* func)(lua_State*))
	{
		auto binding = new ObjectBinding<Class, int, lua_State*>(object, func);
		Bindings.Add(name, binding);
	}
	void Push()
	{
		if (luaL_newmetatable(m_lua, "Scriptable") == 1)
		{
			lua_pushcfunction(m_lua, lIndexFunction);
			lua_setfield(m_lua, -2, "__index");
		}

		LuaBindable** ud = static_cast<LuaBindable**>(lua_newuserdata(m_lua, sizeof(LuaBindable*)));
		*(ud) = this;

		luaL_setmetatable(m_lua, "Scriptable");
		lua_setglobal(m_lua, *m_name);
	}
	String GetName()
	{
		return m_name;
	}

private:
	String m_name;
	lua_State* m_lua;
};

static int lMemberCallFunction(lua_State* L)
{
	IFunctionBinding<int, lua_State*>** t = (IFunctionBinding<int, lua_State*>**)(luaL_checkudata(L, 1, "Scriptable_Callback"));
	if (*t)
		return (*t)->Call(L);
	else
	{
		lua_error(L);
		return 0;
	}
}

static int lIndexFunction(lua_State* L)
{
	LuaBindable** t = static_cast<LuaBindable**>(luaL_checkudata(L, 1, "Scriptable"));
	String lookup = luaL_checkstring(L, 2);

	if (luaL_newmetatable(L, "Scriptable_Callback") == 1)
	{
		lua_pushcfunction(L, lMemberCallFunction);
		lua_setfield(L, -2, "__call");
	}

	IFunctionBinding<int, lua_State*>** ud = static_cast<IFunctionBinding<int, lua_State*>**>(lua_newuserdata(L, sizeof(IFunctionBinding<int, lua_State*>*)));
	*(ud) = (*t)->Bindings[lookup];

	luaL_setmetatable(L, "Scriptable_Callback");

	return 1;
}
