#pragma once

#include "POFile.hpp"
#include "Map.hpp"
#include "Vector.hpp"

/// Class for managing i18n stuff
class I18N final
{
public:
	static inline I18N& Get() { return m_instance; }

	static String GetSystemLanguage();

	void Initialize(const String& dirPath);
	void SetLanguage(const String& lang);

	std::string_view GetText(const std::string_view& msg_id) const;
	std::string_view GetText(const std::string_view& msg_id, const std::string_view& msg_ctxt) const;

	void BindLua(struct lua_State* L);

	const static std::map<String, const char*> SUPPORTED_LANGUAGE_NAMES;
	static int GetLanguageIndexFromNamesMap(const String& lang);

private:
	static int Lua_GetText(struct lua_State* L);

	static I18N m_instance;

	String GetFilePath(const String& lang) const;

	Vector<POFile> m_files;
	String m_dirPath;

	Map<String, size_t> m_filesByLanguage;
	size_t m_currFile;
};

inline std::string_view _(const std::string_view& msg_id)
{
	return I18N::Get().GetText(msg_id);
}

inline std::string_view _(const std::string_view& msg_id, const std::string_view& msg_ctxt)
{
	return I18N::Get().GetText(msg_id, msg_ctxt);
}