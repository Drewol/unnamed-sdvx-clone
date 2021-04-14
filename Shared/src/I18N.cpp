#include "stdafx.h"
#include "I18N.hpp"

#include "Path.hpp"

I18N I18N::m_instance;

const std::map<String, const char*> I18N::SUPPORTED_LANGUAGE_NAMES = {
	{"en", "English"},
	{"ko", "Korean"},
};

static constexpr char ToLower(char ch)
{
	return 'A' <= ch && ch <= 'Z' ? ch + ('a' - 'A') : ch;
}

static constexpr char ToUpper(char ch)
{
	return 'a' <= ch && ch <= 'z' ? ch - ('a' - 'A') : ch;
}

static std::map<std::string_view, String> LANG_ALIAS_MAP = {
	{"english", "en"},
	{"japanese", "ja"},
	{"korean", "ko"},
	{"swedish", "sv"},
};
static std::map<std::string_view, String> COUNTRY_ALIAS_MAP = {
	{"japan", "jp"},
	{"korea", "kr"},
	{"south korea", "kr"},
	{"sweden", "SE"},
	{"united states", "us"},
};

String I18N::GetSystemLanguage()
{
	String currLocale = setlocale(LC_CTYPE, nullptr);

	{
		String localeTemp, encoding;
		if (currLocale.SplitLast(".", &localeTemp, &encoding))
		{
			currLocale = localeTemp;
		}
	}

	String langCode = "";
	String countryCode = "";

	if (!currLocale.Split("_", &langCode, &countryCode) && !currLocale.Split("-", &langCode, &countryCode))
	{
		langCode = currLocale;
		countryCode = "";
	}

	std::transform(langCode.begin(), langCode.end(), langCode.begin(), ToLower);
	std::transform(countryCode.begin(), countryCode.end(), countryCode.begin(), ToLower);

	auto langIt = LANG_ALIAS_MAP.find(langCode);
	if (langIt != LANG_ALIAS_MAP.end())
	{
		langCode = langIt->second;
	}
	auto countryIt = LANG_ALIAS_MAP.find(countryCode);
	if (countryIt != LANG_ALIAS_MAP.end())
	{
		countryCode = countryIt->second;
	}

	std::transform(countryCode.begin(), countryCode.end(), countryCode.begin(), ToUpper);

	if (countryCode.empty()) return langCode;
	else return langCode + "-" + countryCode;
}

void I18N::Initialize(const String& dirPath)
{
	m_filesByLanguage.clear();
	m_files.clear();
	m_currFile = 0;

	m_dirPath = Path::Absolute(dirPath);
}

void I18N::SetLanguage(const String& lang)
{
	if (auto* fileInd = m_filesByLanguage.Find(lang))
	{
		m_currFile = *fileInd;
		return;
	}

	POFile poFile(GetFilePath(lang));
	if (poFile.GetLanguage().empty())
	{
		return;
	}

	const size_t ind = m_files.size();
	m_files.emplace_back(std::move(poFile));

	m_filesByLanguage.emplace(poFile.GetLanguage(), ind);
	m_currFile = ind;
}

std::string_view I18N::GetText(const std::string_view& msg_id) const
{
	if (m_currFile >= m_files.size())
	{
		return msg_id;
	}

	return m_files[m_currFile].GetText(msg_id);
}

std::string_view I18N::GetText(const std::string_view& msg_ctxt, const std::string_view& msg_id) const
{
	if (m_currFile >= m_files.size())
	{
		return msg_id;
	}

	return m_files[m_currFile].GetText(msg_id, msg_ctxt);
}

void I18N::BindLua(lua_State* L)
{
	lua_pushcfunction(L, I18N::Lua_GetText);
	lua_setglobal(L, "_");
}

int I18N::GetLanguageIndexFromNamesMap(const String& lang)
{
	auto it = SUPPORTED_LANGUAGE_NAMES.find(lang);
	if (it != SUPPORTED_LANGUAGE_NAMES.end())
	{
		return (int) std::distance(SUPPORTED_LANGUAGE_NAMES.begin(), it);
	}

	String l, c;
	if (lang.Split("-", &l, &c))
	{
		it = SUPPORTED_LANGUAGE_NAMES.find(l);
		if (it != SUPPORTED_LANGUAGE_NAMES.end())
		{
			return (int) std::distance(SUPPORTED_LANGUAGE_NAMES.begin(), it);
		}
	}
	else
	{
		l = lang;
	}

	it = SUPPORTED_LANGUAGE_NAMES.lower_bound(l);
	if (it->first.substr(0, l.size()) == l)
	{
		return (int)std::distance(SUPPORTED_LANGUAGE_NAMES.begin(), it);
	}

	return -1;
}

int I18N::Lua_GetText(lua_State* L)
{
	switch (lua_gettop(L))
	{
	case 1:
	{
		const char* msg_id = luaL_checkstring(L, 1);
		if (msg_id)
		{
			lua_pushstring(L, I18N::Get().GetText(msg_id).data());
			return 1;
		}
	}
		break;
	case 2:
	{
		const char* msg_id = luaL_checkstring(L, 1);
		const char* msg_ctxt = luaL_checkstring(L, 1);
		if (msg_id && msg_ctxt)
		{
			lua_pushstring(L, I18N::Get().GetText(msg_id, msg_ctxt).data());
			return 1;
		}
	}
		break;
	default:
		return luaL_error(L, "expecting 1 or 2 string arguments");
		break;
	}

	return luaL_error(L, "unknown error");
}

String I18N::GetFilePath(const String& lang) const
{
	String filePath = m_dirPath + Path::sep + lang + ".po";
	if (Path::FileExists(filePath) == false)
	{
		String x, y;
		if (lang.Split("-", &x, &y))
		{
			filePath = m_dirPath + Path::sep + x + ".po";
		}
	}

	return filePath;
}
