#pragma once

#include "String.hpp"

/// Container for PO file (of `gettext`) contents.
/// For the main project, do not use this; use the I18N class instead.
class POFile
{
public:
	struct Entry
	{
		String m_context;
		String m_value;
	};

	POFile(const String& path);
	POFile(class File& file);

	std::string_view GetText(const std::string_view& msg_id) const;
	std::string_view GetText(const std::string_view& msg_id, const std::string_view& msg_ctxt) const;

	inline const String& GetLanguage() const { return m_language; }

private:
	void Parse(class File& file);
	void Parse(class FileReader& reader);

	String m_language;
	std::multimap<String, Entry, std::less<>> m_contents;
};