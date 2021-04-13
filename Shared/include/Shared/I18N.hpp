#pragma once

#include "POFile.hpp"
#include "Map.hpp"
#include "Vector.hpp"

/// Class for managing i18n stuff
class I18N final
{
public:
	static inline I18N& Get() { return m_instance; }

	void Initialize(const String& dirPath);
	void SetLanguage(const String& lang);

	std::string_view GetText(const std::string_view& msg_id) const;
	void BindLua(struct lua_State* L);

private:
	static int Lua_GetText(struct lua_State* L);

	static I18N m_instance;

	Vector<POFile> m_files;
	String m_dirPath;

	Map<String, decltype(m_files)::iterator> m_filesByLanguage;
	decltype(m_files)::iterator m_currFile;
};

inline std::string_view _(const std::string_view& msg_id)
{
	return I18N::Get().GetText(msg_id);
}
