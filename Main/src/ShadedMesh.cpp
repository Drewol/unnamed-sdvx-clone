#include "stdafx.h"
#include "ShadedMesh.hpp"
#include "Application.hpp"
#include "Game.hpp"
#include "Camera.hpp"
#include "Track.hpp"
#include "lua.hpp"

ShadedMesh::ShadedMesh() {
	m_material = g_application->LoadMaterial("guiTex");
	m_material->opaque = false;
	auto mainTex = TextureRes::Create(g_gl);
	SetParam("mainTex", mainTex);
	SetParam("color", Color::White);
	m_textures.Add("mainTex", mainTex);
	m_mesh = MeshRes::Create(g_gl);
	m_mesh->SetPrimitiveType(Graphics::PrimitiveType::TriangleList);
}

ShadedMesh::ShadedMesh(const String& name) {
	m_material = g_application->LoadMaterial(name);
	m_material->opaque = false;
	m_mesh = MeshRes::Create(g_gl);
	m_mesh->SetPrimitiveType(Graphics::PrimitiveType::TriangleList);
}

ShadedMesh::ShadedMesh(const String& name, const String& path) {
	m_material = g_application->LoadMaterial(name, path);
	m_material->opaque = false;
	m_mesh = MeshRes::Create(g_gl);
	m_mesh->SetPrimitiveType(Graphics::PrimitiveType::TriangleList);
}

void ShadedMesh::Draw() {
	auto rq = g_application->GetRenderQueueBase();
	Transform t = g_application->GetCurrentGUITransform();
	t *= Transform::Translation(m_pos);
	t *= Transform::Scale(m_scale);
	t *= Transform::Rotation(m_rotation);
	rq->DrawScissored(g_application->GetCurrentGUIScissor() , t, m_mesh, m_material, m_params);
}

void ShadedMeshOnTrack::DrawOnTrack() {
	RenderState rs = m_game->GetCamera().CreateRenderState(m_clip);
	RenderQueue rq(g_gl, rs);
	Transform t = m_game->GetTrack().trackOrigin;
	t *= Transform::Translation(m_pos);
	t *= Transform::Scale(m_scale);
	t *= Transform::Rotation(m_rotation);
	rq.Draw(t, m_mesh, m_material, m_params);
	rq.Process();
}


void ShadedMesh::SetData(Vector<MeshGenerators::SimpleVertex>& data) {
	m_mesh->SetData(data);
}

void ShadedMesh::AddTexture(const String& name, const String& file) {
	auto* oldTex = m_textures.Find(file);
	if (oldTex)
	{
		SetParam(name, *oldTex);
		return;
	}
	auto newTex = g_application->LoadTexture(file, true);
	m_textures.Add(file, newTex); // Cache texture
	SetParam(name, newTex);
}

void ShadedMesh::AddSkinTexture(const String& name, const String& file) {
	String key = "skin/" + file;
	auto* oldTex = m_textures.Find(key);
	if (oldTex)
	{
		SetParam(name, *oldTex);
		return;
	}

	auto newTex = g_application->LoadTexture(file);
	m_textures.Add(key, newTex); // Cache texture
	SetParam(name, newTex);
}

int ShadedMesh::AddSharedTexture(const String& name, const String& key)
{
	if (g_application->sharedTextures.Contains(key))
	{
		auto& t = g_application->sharedTextures.at(key);
		m_textures.Add(name, t->texture);
		return 0;
	}
	else {
		return 1;
	}

}

void ShadedMesh::SetBlendMode(const MaterialBlendMode& mode)
{
	m_material->blendMode = mode;
}

void ShadedMesh::SetPrimitiveType(const PrimitiveType& type)
{
	m_mesh->SetPrimitiveType(type);
}

void ShadedMesh::SetOpaque(bool opaque)
{
	m_material->opaque = opaque;
}

int lSetData(lua_State* L) {
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	Vector<MeshGenerators::SimpleVertex> newData;

	lua_pushvalue(L, 2); // Push array [-2]
	lua_pushnil(L); // Push slot for next key [-1]
	while (lua_next(L, -2) != 0) // key -> [-2], value -> [-1]
	{
		lua_pushvalue(L, -1); // Push sub array [-2]
		lua_pushnil(L); // Push slot for next key [-1]
		lua_next(L, -2); // key -> [-2], value -> [-1]
		float x, y, z, u, v;
		{
			lua_pushvalue(L, -1); // Push vertex array [-2]
			lua_pushnil(L); // Push slot for next key [-1]
			{
				lua_next(L, -2); // key -> [-2], value -> [-1]
				x = luaL_checknumber(L, -1);
				lua_pop(L, 1); // Remove value
				lua_next(L, -2); // key -> [-2], value -> [-1]
				y = luaL_checknumber(L, -1);
				lua_pop(L, 1); // Remove value
				if (lua_next(L, -2) != 0) {
					z = luaL_checknumber(L, -1);
					lua_pop(L, 2); // Remove value and key
				}
				else
				{
					// lua_next only pushes if there is more key, but consumes the key
					// so we don't need to pop anything on this side
					z = 0.0f;
				}
			}
		}
		lua_pop(L, 2); // Remove key and value
		lua_next(L, -2); // key -> [-2], value -> [-1]
		{

			lua_pushvalue(L, -1); // Push uv array [-2]
			lua_pushnil(L); // Push slot for next key [-1]
			lua_next(L, -2); // key -> [-2], value -> [-1]
			{
				u = luaL_checknumber(L, -1);
				lua_pop(L, 1); // Remove value
				lua_next(L, -2);
				v = luaL_checknumber(L, -1);
				lua_pop(L, 2); // Remove value and key
			}
		}
		lua_pop(L, 5); // Remove uv key, sub array k+v, main array k+v

		MeshGenerators::SimpleVertex newVert;
		newVert.pos = { x, y, z };
		newVert.tex = { u, v };
		newData.Add(newVert);
	}
	object->SetData(newData);
	return 0;
}

#define GET_TRACK_MESH(L, object) \
	ShadedMesh** userdata = static_cast<ShadedMesh**>(lua_touserdata(L, 1)); \
	if (!userdata) { luaL_error(L, "null userdata"); return 0; } \
	object = dynamic_cast<ShadedMeshOnTrack*>(*userdata); \
	if (object == nullptr) { luaL_error(L, "Object is an instance of ShadedMeshOnTrack"); return 0; }

int lUseGameMesh(lua_State* L)
{
	ShadedMeshOnTrack* object;
	GET_TRACK_MESH(L, object)
	object->lUseGameMesh(L);
	return 0;
}

int lSetPos(lua_State* L)
{
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	float x = luaL_checknumber(L, 2);
	float y = luaL_checknumber(L, 3);
	float z = 0.0f;
	if (lua_gettop(L) >= 4)
		z = luaL_checknumber(L, 4);

	object->SetPos(x, y, z);
	return 0;
}

int lGetPos(lua_State* L)
{
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	auto& v = object->GetPos();
	lua_pushnumber(L, v.x);
	lua_pushnumber(L, v.y);
	lua_pushnumber(L, v.z);
	return 3;
}

int lSetScale(lua_State* L)
{
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	float x = luaL_checknumber(L, 2);
	float y = luaL_checknumber(L, 3);
	float z = 1.0f;
	if (lua_gettop(L) >= 4)
		z = luaL_checknumber(L, 4);

	object->SetScale(x, y, z);
	return 0;
}

int lGetScale(lua_State* L)
{
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	auto& v = object->GetScale();
	lua_pushnumber(L, v.x);
	lua_pushnumber(L, v.y);
	lua_pushnumber(L, v.z);
	return 3;
}

int lSetRotation(lua_State* L)
{
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	float roll = luaL_checknumber(L, 2);

	float yaw = 0.0f;
	if (lua_gettop(L) >= 3)
		yaw = luaL_checknumber(L, 3);
	float pitch = 0.0f;
	if (lua_gettop(L) >= 4)
		pitch = luaL_checknumber(L, 4);

	object->SetRotation(pitch, yaw, roll);
	return 0;
}

int lGetRotation(lua_State* L)
{
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	auto& v = object->GetRotation();
	lua_pushnumber(L, v.z);
	lua_pushnumber(L, v.y);
	lua_pushnumber(L, v.x);
	return 3;
}

int lSetWireframe(lua_State* L)
{
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	bool b = lua_toboolean(L, 2);
	object->SetIsWireframe(b);
	return 0;
}

int lScaleToLength(lua_State* L)
{
	ShadedMeshOnTrack* object;
	GET_TRACK_MESH(L, object);

	float s = luaL_checknumber(L, 2) / object->GetLength();
	object->GetScale().y = s;
    return 0;
}


int lSetLength(lua_State* L)
{
	ShadedMeshOnTrack* object;
	GET_TRACK_MESH(L, object);
	float len = luaL_checknumber(L, 2);
	object->SetLength(len);
	return 0;
}

int lSetClipWithTrack(lua_State* L)
{
	ShadedMeshOnTrack* object;
	GET_TRACK_MESH(L, object);
	bool c = lua_toboolean(L, 2);
	object->SetClipping(c);
	return 0;
}

int lGetLength(lua_State* L)
{
	ShadedMeshOnTrack* object;
	GET_TRACK_MESH(L, object);
	lua_pushnumber(L, object->GetLength());
	return 1;
}


void ShadedMeshOnTrack::lUseGameMesh(lua_State* L) {
	const String s = luaL_checkstring(L, 2);
	Track& track = m_game->GetTrack();
	if (s == "button")
	{
		m_mesh = track.buttonMesh;
		m_length = track.buttonLength;
	}
	else if (s == "fxbutton")
	{
		m_mesh = track.fxbuttonMesh;
		m_length = track.fxbuttonLength;
	}
	else if (s == "track")
	{
		m_mesh = track.trackMesh;
		m_length = track.trackLength;
	}
	else
	{
		luaL_error(L, (String("Game mesh not found: ")+s).c_str());
	}
}

int lDraw(lua_State* L) {
	ShadedMesh** userdata = static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	if (userdata)
	{
		ShadedMesh* object = *userdata;

		if (object->IsWireframe())
			glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		if (ShadedMeshOnTrack* objOnTrack = dynamic_cast<ShadedMeshOnTrack*>(object))
		{
			objOnTrack->DrawOnTrack();
		}
		else
		{
			object->Draw();
		}
		if (object->IsWireframe())
			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
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

int lAddSharedTexture(lua_State* L) {
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	auto key = luaL_checkstring(L, 3);
	if (object->AddSharedTexture(luaL_checkstring(L, 2),key)) //Returns 1 on error
	{
		return luaL_error(L, "Could not find shared texture with key: '%s'", key);
	}

	return 0;
}

int lSetBlendMode(lua_State* L) {
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	MaterialBlendMode mode = (MaterialBlendMode)luaL_checkinteger(L, 2);
	object->SetBlendMode(mode);
	return 0;
}

int lSetPrimitiveType(lua_State* L) {
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	PrimitiveType type = (PrimitiveType)luaL_checkinteger(L, 2);
	object->SetPrimitiveType(type);
	return 0;
}

int lSetOpaque(lua_State* L) {
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	bool opaque = lua_toboolean(L, 2);
	object->SetOpaque(opaque);
	return 0;
}

int lSetParam(lua_State* L) {
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	if (lua_isinteger(L, 3)) {
		String name = luaL_checkstring(L, 2);
		int value = luaL_checkinteger(L, 3);
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

int lSetParamVec3(lua_State* L)
{
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	String name = luaL_checkstring(L, 2);
	float x, y, z;
	x = luaL_checknumber(L, 3);
	y = luaL_checknumber(L, 4);
	z = luaL_checknumber(L, 5);
	Vector3 vec = { x,y,z };
	object->SetParam(name, vec);

	return 0;
}

int lSetParamVec2(lua_State* L)
{
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	String name = luaL_checkstring(L, 2);
	if (lua_isinteger(L, 3)) {
		int x, y;
		x = luaL_checkinteger(L, 3);
		y = luaL_checkinteger(L, 4);
		Vector2i vec = { x,y };
		object->SetParam(name, vec);
	}
	else if (lua_isnumber(L, 3)) {
		float x, y;
		x = luaL_checknumber(L, 3);
		y = luaL_checknumber(L, 4);
		Vector2 vec = { x,y };
		object->SetParam(name, vec);
	}

	return 0;
}

int lSetParamVec4(lua_State* L)
{
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	String name = luaL_checkstring(L, 2);
	float x, y, z, w;
	x = luaL_checknumber(L, 3);
	y = luaL_checknumber(L, 4);
	z = luaL_checknumber(L, 5);
	w = luaL_checknumber(L, 6);
	Vector4 vec = { x,y,z,w };
	object->SetParam(name, vec);
	
	return 0;
}

int __index(lua_State* L) {
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	String fname = lua_tostring(L, 2);
	Map<String, lua_CFunction> fmap;
	Map<String, int> constMap;
	Map<String, double> doubleConstMap;
	fmap.Add("Draw", lDraw);
	fmap.Add("AddTexture", lAddTexture);
	fmap.Add("AddSkinTexture", lAddSkinTexture);
	fmap.Add("AddSharedTexture", lAddSharedTexture);
	fmap.Add("SetParam", lSetParam);
	fmap.Add("SetParamVec2", lSetParamVec2);
	fmap.Add("SetParamVec3", lSetParamVec3);
	fmap.Add("SetParamVec4", lSetParamVec4);
	fmap.Add("SetData", lSetData);
	fmap.Add("SetBlendMode", lSetBlendMode);
	fmap.Add("SetPrimitiveType", lSetPrimitiveType);
	fmap.Add("SetOpaque", lSetOpaque);
	fmap.Add("SetPosition", lSetPos);
	fmap.Add("GetPosition", lGetPos);
	fmap.Add("SetScale", lSetScale);
	fmap.Add("GetScale", lGetScale);
	fmap.Add("SetRotation", lSetRotation);
	fmap.Add("GetRotation", lGetRotation);
	fmap.Add("SetWireframe", lSetWireframe);

	constMap.Add("BLEND_ADD",  (int)MaterialBlendMode::Additive);
	constMap.Add("BLEND_MULT", (int)MaterialBlendMode::Multiply);
	constMap.Add("BLEND_NORM", (int)MaterialBlendMode::Normal);
	constMap.Add("PRIM_TRILIST", (int)PrimitiveType::TriangleList);
	constMap.Add("PRIM_TRIFAN", (int)PrimitiveType::TriangleFan);
	constMap.Add("PRIM_TRISTRIP", (int)PrimitiveType::TriangleStrip);
	constMap.Add("PRIM_LINELIST", (int)PrimitiveType::LineList);
	constMap.Add("PRIM_LINESTRIP", (int)PrimitiveType::LineStrip);
	constMap.Add("PRIM_POINTLIST", (int)PrimitiveType::PointList);

	if (auto* obj = dynamic_cast<ShadedMeshOnTrack*>(object))
	{
		fmap.Add("UseGameMesh", lUseGameMesh);
		fmap.Add("SetLength", lSetLength);
		fmap.Add("GetLength", lGetLength);
		fmap.Add("ScaleToLength", lScaleToLength);
		fmap.Add("SetClipWithTrack", lSetClipWithTrack);
		Track& track = obj->GetGame()->GetTrack();
		doubleConstMap.Add("BUTTON_TEXTURE_LENGTH", track.buttonLength);
		doubleConstMap.Add("FXBUTTON_TEXTURE_LENGTH", track.fxbuttonLength);
		doubleConstMap.Add("TRACK_LENGTH", track.trackLength);
		doubleConstMap.Add("TRACK_WIDTH", track.trackWidth);
	}
	

	auto function = fmap.find(fname);

	if (function != fmap.end())
	{
		lua_pushcfunction(L, function->second);
		return 1;
	}

	auto constValue = constMap.find(fname);

	if (constValue != constMap.end())
	{
		lua_pushinteger(L, constValue->second);
		return 1;
	}

	auto doubleConstValue = doubleConstMap.find(fname);
	if (doubleConstValue != doubleConstMap.end())
	{
		lua_pushinteger(L, doubleConstValue->second);
		return 1;
	}
		

	return luaL_error(L, *Utility::Sprintf("ShadedMesh has no: '%s'", *fname));
}

int __gc(lua_State* L) {
	ShadedMesh* object = *static_cast<ShadedMesh**>(lua_touserdata(L, 1));
	delete object;
	return 0;
}


int ShadedMesh::lNew(lua_State* L)
{
	ShadedMesh** place;
	if (lua_gettop(L) == 2)
	{
		place = (ShadedMesh**)lua_newuserdata(L, sizeof(ShadedMesh*));
		*place = new ShadedMesh(luaL_checkstring(L, 1), luaL_checkstring(L, 2));
	}
	else if (lua_gettop(L) == 1)
	{
		place = (ShadedMesh**)lua_newuserdata(L, sizeof(ShadedMesh*));
		*place = new ShadedMesh(luaL_checkstring(L, 1));
	}
	else {
		place = (ShadedMesh**)lua_newuserdata(L, sizeof(ShadedMesh*));
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

int ShadedMeshOnTrack::lNew(lua_State* L, class Game* game)
{
	// TODO We could maybe use templates to combine this better with above
	ShadedMeshOnTrack** place;
	if (lua_gettop(L) == 2)
	{
		place = (ShadedMeshOnTrack**)lua_newuserdata(L, sizeof(ShadedMeshOnTrack*));
		*place = new ShadedMeshOnTrack(game, luaL_checkstring(L, 1), luaL_checkstring(L, 2));
	}
	else if (lua_gettop(L) == 1)
	{
		place = (ShadedMeshOnTrack**)lua_newuserdata(L, sizeof(ShadedMeshOnTrack*));
		*place = new ShadedMeshOnTrack(game, luaL_checkstring(L, 1));
	}
	else {
		place = (ShadedMeshOnTrack**)lua_newuserdata(L, sizeof(ShadedMeshOnTrack*));
		*place = new ShadedMeshOnTrack(game);
	}

	lua_newtable(L);

	lua_pushcfunction(L, __index);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, __gc);
	lua_setfield(L, -2, "__gc");

	lua_setmetatable(L, -2);
	return 1;
}
