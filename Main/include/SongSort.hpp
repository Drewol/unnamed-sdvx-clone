#pragma once
#include "SongSelect.hpp"
#include "ChallengeSelect.hpp"
#include <Beatmap/MapDatabase.hpp>

enum SortType
{
	NO_SORT,
	TITLE_ASC,
	TITLE_DESC,
	SCORE_DESC,
	SCORE_ASC,
	DATE_DESC,
	DATE_ASC,
	ARTIST_ASC,
	ARTIST_DESC,
	EFFECTOR_ASC,
	EFFECTOR_DESC,
	SORT_COUNT,
};

template<class ItemIndex>
class ItemSort
{
public:
	ItemSort(String name, bool dir) : m_name(name), m_dir(dir) {};
	virtual ~ItemSort() = default;

	virtual SortType GetType() const { return NO_SORT; };
	String GetName() const { return m_name; }
	virtual void SortInplace(Vector<uint32>& vec, const Map<int32,
		ItemIndex>& collection) {};
protected:
	String m_name;
	bool m_dir;
};

using SongSort = ItemSort<SongSelectIndex>;

class TitleSort : public SongSort
{
public:
	TitleSort(String name, bool dir) : SongSort(name, dir) {};
	void SortInplace(Vector<uint32>& vec, const Map<int32,
		SongSelectIndex>& collection) override;
	virtual bool CompareSongs(const SongSelectIndex& song_a,
		const SongSelectIndex& song_b);
	SortType GetType() const override
	{
		return m_dir ? SortType::TITLE_DESC : SortType::TITLE_ASC;
	};
};

class ScoreSort : public TitleSort
{
public:
	ScoreSort(String name, bool dir) : TitleSort(name, dir) {};
	void SortInplace(Vector<uint32>& vec, const Map<int32,
		SongSelectIndex>& collection) override;
	SortType GetType() const override
	{
		return m_dir ? SortType::SCORE_DESC : SortType::SCORE_ASC;
	};
private:
	Map<uint32, uint32> m_scoreMap;
};

class DateSort : public TitleSort
{
public:
	DateSort(String name, bool dir) : TitleSort(name, dir) {};
	void SortInplace(Vector<uint32>& vec, const Map<int32,
		SongSelectIndex>& collection) override;
	SortType GetType() const override
	{
		return m_dir ? SortType::DATE_DESC : SortType::DATE_ASC;
	};
private:
	Map<uint32, uint64> m_dateMap;
};

class ArtistSort : public TitleSort
{
public:
	ArtistSort(String name, bool dir) : TitleSort(name, dir) {};
	void SortInplace(Vector<uint32>& vec, const Map<int32,
		SongSelectIndex>& collection) override;
	SortType GetType() const override
	{
		return m_dir ? SortType::ARTIST_DESC : SortType::ARTIST_ASC;
	};
private:
	Map<uint32, uint64> m_dateMap;
};

class EffectorSort : public TitleSort
{
public:
	EffectorSort(String name, bool dir) : TitleSort(name, dir) {};
	void SortInplace(Vector<uint32>& vec, const Map<int32,
		SongSelectIndex>& collection) override;
	SortType GetType() const override
	{
		return m_dir ? SortType::EFFECTOR_DESC : SortType::EFFECTOR_ASC;
	};
};

class ClearMarkSort : public TitleSort
{
public:
	ClearMarkSort(String name, bool dir) : TitleSort(name, dir) {};
	void SortInplace(Vector<uint32>& vec, const Map<int32,
		SongSelectIndex>& collection) override;
	SortType GetType() const override
	{
		return m_dir ? SortType::EFFECTOR_DESC : SortType::EFFECTOR_ASC;
	};
private:
	Map<uint32, uint64> m_clearMap;
};

using ChallengeSort = ItemSort<ChallengeSelectIndex>;

class ChallengeTitleSort : public ChallengeSort
{
public:
	ChallengeTitleSort(String name, bool dir) : ChallengeSort(name, dir) {};
	void SortInplace(Vector<uint32>& vec, const Map<int32,
		ChallengeSelectIndex>& collection) override;
	virtual bool CompareChallenges(const ChallengeSelectIndex& song_a,
		const ChallengeSelectIndex& song_b);
	SortType GetType() const override
	{
		return m_dir ? SortType::TITLE_DESC : SortType::TITLE_ASC;
	};
};

class ChallengeDateSort : public ChallengeTitleSort
{
public:
	ChallengeDateSort(String name, bool dir) : ChallengeTitleSort(name, dir) {};
	void SortInplace(Vector<uint32>& vec, const Map<int32,
		ChallengeSelectIndex>& collection) override;
	SortType GetType() const override
	{
		return m_dir ? SortType::DATE_DESC : SortType::DATE_ASC;
	};
};

class ChallengeScoreSort : public ChallengeTitleSort
{
public:
	ChallengeScoreSort(String name, bool dir) : ChallengeTitleSort(name, dir) {};
	void SortInplace(Vector<uint32>& vec, const Map<int32,
		ChallengeSelectIndex>& collection) override;
	SortType GetType() const override
	{
		return m_dir ? SortType::DATE_DESC : SortType::DATE_ASC;
	};
};

class ChallengeClearMarkSort : public ChallengeTitleSort
{
public:
	ChallengeClearMarkSort(String name, bool dir) : ChallengeTitleSort(name, dir) {};
	void SortInplace(Vector<uint32>& vec, const Map<int32,
		ChallengeSelectIndex>& collection) override;
	SortType GetType() const override
	{
		return m_dir ? SortType::DATE_DESC : SortType::DATE_ASC;
	};
};
