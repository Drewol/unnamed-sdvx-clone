#include "stdafx.h"
#include "Config.hpp"
#include "TextStream.hpp"
#include "Set.hpp"
#include "File.hpp"
#include "FileStream.hpp"
#include <sstream>

ConfigBase::~ConfigBase()
{
	for (auto e : m_entries)
	{
		delete e.second;
	}
}
bool ConfigBase::Load(const String& path, bool reset)
{
	File file;
	if (!file.OpenRead(path))
		return false;
	FileReader reader(file);
	return Load(reader, reset);
}
bool ConfigBase::Load(BinaryStream& stream, bool reset)
{
	// Clear and load defaults
	if (reset)
		Clear();

	Set<uint32> setKeys;
	for (auto e : m_entries)
	{
		setKeys.Add(e.first);
	}

	String line;
	while (TextStream::ReadLine(stream, line))
	{
		String k, v;
		if (line.Split("=", &k, &v))
		{
			k.Trim(' ');
			v.Trim(' ');
			std::stringstream s(v);

			auto it = m_keys.find(k);
			if (it != m_keys.end())
			{
				auto it1 = m_entries.find(it->second);
				if (it1 != m_entries.end())
				{
					m_entriesInFile.insert(it1->first);
					setKeys.erase(it1->first);
					m_entries[it1->first]->FromString(v);
				}
			}
		}
	}


	if (!setKeys.empty())
	{
		// Default setting missed in config file, flag as dirty
		m_dirty = true;
	}
	else
	{
		// Config is clean
		m_dirty = false;
	}
	return true;
}
bool ConfigBase::Save(const String& path,
	ConfigBase::KeyList* ignore,
	ConfigBase::KeyList* only)
{
	File file;
	if (!file.OpenWrite(path))
		return false;
	FileWriter reader(file);
	Save(reader, ignore, only);
	return true;
}
void ConfigBase::Save(BinaryStream& stream,
	ConfigBase::KeyList* ignore,
	ConfigBase::KeyList* only)
{
	for (auto& e : m_entries)
	{
		if (ignore && ignore->find(e.first) != ignore->end())
			continue;

		if (only && only->find(e.first) == only->end())
			continue;

		String key = m_reverseKeys[e.first];
		String line = key + " = " + e.second->ToString();
		TextStream::WriteLine(stream, line);
	}

	// Saved
	m_dirty = false;
}
void ConfigBase::Update(ConfigBase& other,
	ConfigBase::KeyList* ignore,
	ConfigBase::KeyList* only)
{
	for (auto& e : other.m_entries)
	{
		if (ignore && ignore->find(e.first) != ignore->end())
			continue;

		if (only && only->find(e.first) == only->end())
			continue;

		// TODO a better method of assignment than this?
		m_entries[e.first]->FromString(e.second->ToString());
	}

	m_dirty = true;
}
bool ConfigBase::IsDirty() const
{
	return m_dirty;
}
void ConfigBase::Clear()
{
	for (auto e : m_entries)
	{
		delete e.second;
	}
	m_entries.clear();
	InitDefaults();
}
ConfigBase::ConfigBase()
{
}
