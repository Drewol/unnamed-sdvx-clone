#pragma once
#include "SongSelect.hpp"
#include "ChallengeSelect.hpp"
#include <Beatmap/MapDatabase.hpp>

enum FilterType
{
	All,
	Folder,
	Level,
	Collection
};

template<class ItemIndex>
class Filter
{
public:
	Filter() = default;
	virtual ~Filter() = default;
	virtual String GetName() const { return m_name; }
	virtual bool IsAll() const { return true; }
	virtual FilterType GetType() const { return FilterType::All; }
	virtual Map<int32, ItemIndex> GetFiltered(const Map<int32, ItemIndex>& source) { return source; }
private:
	String m_name = "All";
};

using SongFilter = Filter<SongSelectIndex>;

class LevelFilter : public SongFilter
{
public:
	~LevelFilter() = default;
	LevelFilter(uint16 level) : m_level(level) {}
	Map<int32, SongSelectIndex> GetFiltered(const Map<int32, SongSelectIndex>& source) override;
	String GetName() const override;
	bool IsAll() const override;
	FilterType GetType() const override { return FilterType::Level; }


private:
	uint16 m_level;
};

class FolderFilter : public SongFilter
{
public:
	FolderFilter(String folder, MapDatabase* database) : m_folder(folder), m_mapDatabase(database) {}
	~FolderFilter() = default;
	Map<int32, SongSelectIndex> GetFiltered(const Map<int32, SongSelectIndex>& source) override;
	String GetName() const override;
	bool IsAll() const override;
	FilterType GetType() const override { return FilterType::Folder; }


private:
	String m_folder;
	MapDatabase* m_mapDatabase;

};

class CollectionFilter : public SongFilter
{
public:
	CollectionFilter(String collection, MapDatabase* database) : m_collection(collection), m_mapDatabase(database) {}
	~CollectionFilter() = default;

	Map<int32, SongSelectIndex> GetFiltered(const Map<int32, SongSelectIndex>& source) override;
	String GetName() const override;
	bool IsAll() const override;
	FilterType GetType() const override { return FilterType::Collection; }


private:
	String m_collection;
	MapDatabase* m_mapDatabase;

};

using ChallengeFilter = Filter<ChallengeSelectIndex>;

class ChallengeLevelFilter : public ChallengeFilter
{
public:
	~ChallengeLevelFilter() = default;
	ChallengeLevelFilter(uint16 level) : m_level(level) {}
	Map<int32, ChallengeSelectIndex> GetFiltered(const Map<int32, ChallengeSelectIndex>& source) override;
	String GetName() const override;
	bool IsAll() const override;
	FilterType GetType() const override { return FilterType::Level; }
private:
	uint16 m_level;
};
