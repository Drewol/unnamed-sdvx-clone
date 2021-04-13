#include "stdafx.h"
#include "I18N.hpp"

#include "Path.hpp"

I18N I18N::m_instance;

void I18N::Initialize(const String& dirPath)
{
	m_filesByLanguage.clear();
	m_files.clear();
	m_currFile = m_files.end();

	m_dirPath = Path::Absolute(dirPath);
}

void I18N::SetLanguage(const String& lang)
{
	if (auto* fileIt = m_filesByLanguage.Find(lang))
	{
		m_currFile = *fileIt;
		return;
	}

	const String filePath = m_dirPath + Path::sep + lang + ".po";

	POFile poFile(filePath);
	if (poFile.GetLanguage().empty())
	{
		return;
	}

	m_files.emplace_back(std::move(poFile));
	auto it = std::prev(m_files.end());

	m_filesByLanguage.emplace(poFile.GetLanguage(), it);
	m_currFile = it;
}

std::string_view I18N::GetText(const std::string_view& msg_id) const
{
	if (m_currFile == m_files.end())
	{
		return msg_id;
	}

	return m_currFile->GetText(msg_id);
}

void I18N::BindLua(lua_State* L)
{
	lua_pushcfunction(L, I18N::Lua_GetText);
	lua_setglobal(L, "_");
}

int I18N::Lua_GetText(lua_State* L)
{
	if (const char* msg_id = luaL_checkstring(L, 1))
	{
		lua_pushstring(L, I18N::Get().GetText(msg_id).data());
		return 1;
	}
	else
	{
		return 0;
	}
}
