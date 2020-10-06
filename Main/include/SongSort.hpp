#pragma once
#include "stdafx.h"
#include "SongSelect.hpp"
#include <Beatmap/MapDatabase.hpp>

enum SortType
{
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

class SongSort
{
	public:
		SongSort(String name, bool dir) : m_name(name),m_dir(dir) {};
		virtual ~SongSort() = default;
		virtual void SortInplace(Vector<uint32>& vec, const Map<int32,
				SongSelectIndex>& collection) = 0;
		virtual SortType GetType() const = 0;
		String GetName() const { return m_name; }
	protected:
		String m_name;
		bool m_dir;
};

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
			return m_dir? SortType::TITLE_DESC : SortType::TITLE_ASC;
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
			return m_dir? SortType::SCORE_DESC : SortType::SCORE_ASC;
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
			return m_dir? SortType::DATE_DESC : SortType::DATE_ASC;
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
			return m_dir? SortType::ARTIST_DESC : SortType::ARTIST_ASC;
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
			return m_dir? SortType::EFFECTOR_DESC : SortType::EFFECTOR_ASC;
		};
	private:
		Map<uint32, uint64> m_dateMap;
};
