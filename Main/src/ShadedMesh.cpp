#include "stdafx.h"
#include "ShadedMesh.hpp"
#include "Application.hpp"
#include "lua.hpp"

static void stackDump(lua_State* L) {
	int i = lua_gettop(L);
	Log(" ----------------  Stack Dump ----------------");
	while (i) {
		int t = lua_type(L, i);
		switch (t) {
		case LUA_TSTRING:
			Logf("%d:`%s'",Logger::Info, i, lua_tostring(L, i));
			break;
		case LUA_TBOOLEAN:
			Logf("%d: %s", Logger::Info, i, lua_toboolean(L, i) ? "true" : "false");
			break;
		case LUA_TNUMBER:
			Logf("%d: %g", Logger::Info, i, lua_tonumber(L, i));
			break;
		default: Logf("%d: %s", Logger::Info, i, lua_typename(L, t)); break;
		}
		i--;
	}
	Log("--------------- Stack Dump Finished ---------------");
}

ShadedMesh::ShadedMesh() {
	m_material = g_application->LoadMaterial("guiTex");
	auto mainTex = TextureRes::Create(g_gl);
	SetParam("mainTex", mainTex);
	SetParam("color", Color::White);
	m_textures.Add("mainTex", mainTex);
	m_mesh = MeshRes::Create(g_gl);
	m_mesh->SetPrimitiveType(Graphics::PrimitiveType::TriangleList);
}

ShadedMesh::ShadedMesh(const String& name) {
	m_material = g_application->LoadMaterial(name);
	m_mesh = MeshRes::Create(g_gl);
	m_mesh->SetPrimitiveType(Graphics::PrimitiveType::TriangleList);
}

void ShadedMesh::Draw() {
	auto rq = g_application->GetRenderQueueBase();
	rq->DrawScissored(g_application->GetCurrentGUIScissor() ,g_application->GetCurrentGUITransform(), m_mesh, m_material, m_params);
}

void ShadedMesh::SetData(Vector<MeshGenerators::SimpleVertex>& data) {
	m_mesh->SetData(data);
}

void ShadedMesh::AddTexture(const String& name, const String& file) {
	auto newTex = g_application->LoadTexture(file, true);
	m_textures.Add(name, newTex);
	SetParam(name, newTex);
}

void ShadedMesh::AddSkinTexture(const String& name, const String& file) {
	auto newTex = g_application->LoadTexture(file);
	m_textures.Add(name, newTex);
	SetParam(name, newTex);
}

int lSetData(lua_State* L) {
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	Vector<MeshGenerators::SimpleVertex> newData;

	lua_pushvalue(L, 2);
	lua_pushnil(L);
	while (lua_next(L, -2) != 0)
	{
		lua_pushvalue(L, -1);
		lua_pushnil(L);
		lua_next(L, -2);
		float x, y, u, v;
		{

			lua_pushvalue(L, -1);
			lua_pushnil(L);
			{
				lua_next(L, -2);
				x = luaL_checknumber(L, -1);
				lua_pop(L, 1);
				lua_next(L, -2);
				y = luaL_checknumber(L, -1);
				lua_pop(L, 2);
			}
		}
		lua_pop(L, 2);
		lua_next(L, -2);
		{

			lua_pushvalue(L, -1);
			lua_pushnil(L);
			lua_next(L, -2);
			{
				u = luaL_checknumber(L, -1);
				lua_pop(L, 1);
				lua_next(L, -2);
				v = luaL_checknumber(L, -1);
				lua_pop(L, 2);
			}
		}
		lua_pop(L, 5);

		MeshGenerators::SimpleVertex newVert;
		newVert.pos = { x, y, 0.0f};
		newVert.tex = { u, v };
		newData.Add(newVert);
	}
	object->SetData(newData);
	return 0;
}

int lDraw(lua_State* L) {
	ShadedMesh** userdata = static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	if (userdata)
	{
		ShadedMesh* object = *userdata;
		object->Draw();
	}
	else {
		luaL_error(L, "null userdata");
	}
	return 0;
}

int lAddTexture(lua_State* L) {
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	object->AddTexture(luaL_checkstring(L, 2), luaL_checkstring(L, 3));
	return 0;
}

int lAddSkinTexture(lua_State* L) {
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	object->AddSkinTexture(luaL_checkstring(L, 2), luaL_checkstring(L, 3));
	return 0;
}

int lSetParam(lua_State* L) {
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	if (lua_isinteger(L, 3)) {
		String name = luaL_checkstring(L, 2);
		int value = luaL_checknumber(L, 3);
		object->SetParam(name, value);
		return 0;
	}
	else if (lua_isnumber(L, 3)) {
		String name = luaL_checkstring(L, 2);
		float value = luaL_checknumber(L, 3);
		object->SetParam(name, value);
		return 0;
	}
	return 0;
}


int __index(lua_State* L) {
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	String fname = lua_tostring(L, 2);
	Map<String, lua_CFunction> fmap;
	fmap.Add("Draw", lDraw);
	fmap.Add("AddTexture", lAddTexture);
	fmap.Add("AddSkinTexture", lAddSkinTexture);
	fmap.Add("SetParam", lSetParam);
	fmap.Add("SetData", lSetData);

	auto function = fmap.find(fname);

	if (function == fmap.end())
	{
		return luaL_error(L, *Utility::Sprintf("ShadedMesh has no: '%s'", *fname));
	}

	lua_pushcfunction(L, function->second);
	return 1;
}

int __gc(lua_State* L) {
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	delete object;
	return 0;
}

int ShadedMesh::lNew(lua_State* L)
{
	if (lua_gettop(L) == 1)
	{
		ShadedMesh** place = (ShadedMesh**)lua_newuserdata(L, sizeof(ShadedMesh*));
		*place = new ShadedMesh(luaL_checkstring(L, 1));
	}
	else {
		ShadedMesh** place = (ShadedMesh**)lua_newuserdata(L, sizeof(ShadedMesh*));
		*place = new ShadedMesh();
	}

	lua_newtable(L);

	lua_pushcfunction(L, __index);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, __gc);
	lua_setfield(L, -2, "__gc");

	lua_setmetatable(L, -2);
	return 1;

}
