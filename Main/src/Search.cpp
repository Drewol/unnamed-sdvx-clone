#include "stdafx.h"
#include "Search.hpp"

String SearchParser::Parse(const String& input, const Vector<SearchKey> keys)
{
	String raw = input;
	SearchKey key = { "" , nullptr };
	char last = '\0';
	for (size_t i = 0; i < raw.length(); i++)
	{
		const String& keyName = key.first;
		if (last != '\0' && last != ' ') {
			last = raw[i];
			continue;
		}

		for (const SearchKey& ikey : keys)
		{
			if (raw[i] == ikey.first[0])
			{
				key = ikey;
				break;
			}
		}
		if (!keyName.empty())
		{
			bool found = false;
			for (size_t j = i; j < raw.length(); j++)
			{
				if (raw[j] == ':' || raw[j] == '=')
				{
					j++;
					// We have a match, so record until space
					size_t end = raw.find(' ', j);

					if (end == String::npos)
						end = raw.length();

					*key.second = raw.substr(j, end - j);
					raw.erase(i, end - i);

					// Restart from updated index
					found = true;
					i--;
					break;
				}

				if (j - i >= keyName.length() || raw[j] != keyName[j - i])
					break;
			}

			if (found) // don't update last if we changed the string
				continue;
		}
		last = raw[i];
	}
	return raw;
}
