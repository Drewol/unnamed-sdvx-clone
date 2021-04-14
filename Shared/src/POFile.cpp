#include "stdafx.h"
#include "POFile.hpp"

#include "File.hpp"
#include "FileStream.hpp"
#include "TextStream.hpp"

#include "Timer.hpp"
#include "Log.hpp"
#include "Profiling.hpp"

POFile::POFile(const String& path)
{
	File file;
	if (!file.OpenRead(path))
	{
		return;
	}
	Parse(file);
}

POFile::POFile(File& file)
{
	Parse(file);
}

std::string_view POFile::GetText(const std::string_view& msg_id) const
{
	auto it = m_contents.find(msg_id);
	if (it != m_contents.end()) return it->second.m_value;
	else return msg_id;
}

static inline size_t LongestMatch(const std::string_view& x, const std::string_view& y)
{
	size_t i = 0;

	for (; i < x.size() && i < y.size(); ++i)
	{
		if (x[i] != y[i]) return i;
	}

	return i;
}

std::string_view POFile::GetText(const std::string_view& msg_id, const std::string_view& msg_ctxt) const
{
	auto range = m_contents.equal_range(msg_id);
	auto longest_match = range.first;
	size_t longest_match_size = 0;

	for (auto it = range.first; it != range.second; ++it)
	{
		const size_t curr_match_size = LongestMatch(msg_ctxt, it->second.m_context);
		if (curr_match_size > longest_match_size)
		{
			longest_match_size = curr_match_size;
			longest_match = it;
		}
	}

	if (longest_match != m_contents.end())
	{
		return longest_match->second.m_value;
	}
	else
	{
		return msg_id;
	}
}

void POFile::Parse(File& file)
{
	FileReader reader(file);
	Parse(reader);
}

static inline String unquote(String str)
{
	std::stringstream ss;

	bool in_quote = false;
	bool escaped = false;

	for (char c : str)
	{
		if (escaped)
		{
			switch (c)
			{
			case 'n': ss << '\n'; break;
			case 'r': ss << '\r'; break;
			case 't': ss << '\t'; break;
			case '\\':
			case '"':
			case '\'':
			default: ss << c; break;
			}

			escaped = false;
		}
		else if (in_quote)
		{
			switch (c)
			{
			case '"':
				in_quote = false;
				break;
			case '\\':
				escaped = true;
				break;
			default:
				ss << c;
				break;
			}
		}
		else
		{
			switch (c)
			{
			case '"':
				in_quote = true;
				break;
			case ' ':
			case '\n':
			case '\r':
			case '\t':
				break;
			default:
				in_quote = true;
				ss << c;
				break;
			}
		}
	}

	return ss.str();
}

void POFile::Parse(FileReader& reader)
{
	ProfilerScope $("Load POFile");

	String line;
	
	String curr_msgctxt;
	String curr_msgid;
	String curr_msgstr;

	String metadata;

	bool curr_id_seen = false;

	const static int CURR_ITEM_NONE = -1;
	const static int CURR_ITEM_MSGCTXT = 0;
	const static int CURR_ITEM_MSGID = 1;
	const static int CURR_ITEM_MSGSTR = 2;

	int curr_item = CURR_ITEM_NONE;

	auto push_curr_msg = [&]() {
		if (curr_id_seen)
		{
			if (curr_msgid.empty() && curr_msgctxt.empty())
			{
				metadata = curr_msgstr;
			}
			else
			{
				if (!curr_msgid.empty() && curr_msgstr.empty()) curr_msgstr = curr_msgid;
				m_contents.emplace(curr_msgid, Entry{ curr_msgctxt, curr_msgstr });
			}
		}

		curr_msgctxt.clear();
		curr_msgid.clear();
		curr_msgstr.clear();

		curr_id_seen = false;
	};
	
	while (TextStream::ReadLine(reader, line))
	{
		line.Trim();
		if (line.empty()) continue;

		if (line[0] == '#') continue;

		if (line.substr(0, 3).compare("msg") != 0)
		{
			if (line[0] == '"')
			{
				switch (curr_item)
				{
				case CURR_ITEM_MSGCTXT: curr_msgctxt += unquote(line); break;
				case CURR_ITEM_MSGID: curr_msgid += unquote(line); break;
				case CURR_ITEM_MSGSTR: curr_msgstr += unquote(line); break;
				default: break;
				}
			}
			else
			{
				curr_item = CURR_ITEM_NONE;
			}
			continue;
		}

		// msgid
		if (line.substr(3, 3).compare("id ") == 0)
		{
			push_curr_msg();
			curr_id_seen = true;
			curr_msgid = unquote(line.substr(6));
			curr_item = CURR_ITEM_MSGID;
		}

		// msgstr
		else if (line.substr(3, 4).compare("str ") == 0)
		{
			curr_msgstr += unquote(line.substr(7));
			curr_item = CURR_ITEM_MSGSTR;
		}

		// msgctxt
		else if (line.substr(3, 5).compare("ctxt ") == 0)
		{
			push_curr_msg();
			curr_msgctxt = unquote(line.substr(8));
			curr_item = CURR_ITEM_MSGCTXT;
		}
	}

	push_curr_msg();

	// Parse metadata
	for (String line : metadata.Explode("\n", false))
	{
		String key, value;
		line.Split(": ", &key, &value);

		if (key == "Language")
		{
			m_language = value;
		}
	}
}
