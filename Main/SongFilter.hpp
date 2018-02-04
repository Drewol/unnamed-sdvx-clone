#pragma once
#include "stdafx.h"
#include "SongSelect.hpp"
#include <Beatmap/MapDatabase.hpp>

class SongFilter
{
public:
	SongFilter() = default;
	~SongFilter() = default;

	virtual Map<int32, SongSelectIndex> GetFiltered(const Map<int32, SongSelectIndex>& source) { return source; }
	virtual String GetName() { return m_name; }
	virtual bool IsAll() { return true; }

private:
	String m_name = "All";

};

class LevelFilter : public SongFilter
{
public:
	LevelFilter(uint16 level) : m_level(level) {}
	virtual Map<int32, SongSelectIndex> GetFiltered(const Map<int32, SongSelectIndex>& source) override;
	virtual String GetName() override;
	virtual bool IsAll() override;

private:
	uint16 m_level;
};

class FolderFilter : public SongFilter
{
public:
	FolderFilter(String folder) : m_folder(folder) {}
	virtual Map<int32, SongSelectIndex> GetFiltered(const Map<int32, SongSelectIndex>& source);
	virtual String GetName() override;
	virtual bool IsAll() override;

private:
	String m_folder;
};