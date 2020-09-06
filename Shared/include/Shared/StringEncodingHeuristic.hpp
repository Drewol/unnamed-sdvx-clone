#pragma once

#include <cstdint>
#include "Shared/StringEncoding.hpp"

/*
	Heuristics for Encoding Detection
	---------------------------------
	This file and StringEncodingHeuristics.cpp contains logic for simple encoding detection.
	Because of the specific domain of interest, other encoding detection libraries such as `uchardet` couldn't be used.
	* Almost all non UTF-8 strings are Shift_JIS encoded.
	* Often, chart titles and artist/effector names consist of various special characters.

	The heuristic is very simple.
	1. For each character, the character class is defined. (See `CharClass` enum below.)
	2. There is a score associated with each character class.
	3. An encoding which successfully decodes input with lowest total score is chosen.

	Sometimes the encoding is incorrectly chosen for strings encoded in other encoding (ex: CP850).
*/

// Heuristic values for various character classes
// Feel free to tweak these values
enum class CharClass
{
	/* Common */
	INVALID = -1,
	IGNORABLE = 0,	// Ignorable characters, such as BOM
	ALNUM = 10,	// Full-width or half-width 0-9, A-Z, a-z
	ASCII_OTHER_CHARS = 15,	// ASCII symbols
	OTHER_CHARS = 100,	// Characters which do not fall in any other classes
	PRIVATE_USE = 1000,	// Private-use characters

	/* Kana */
	KANA = 20,

	/* Hangul */
	HANGUL_UNICODE = 50, // Any Hangul
	HANGUL_KSX1001 = 25, // Hangul in KS X 1001
	HANGUL_CP949 = 100, // Hangul not in KS X 1001

	/* Kanji */
	KANJI_LEVEL_1 = 30, // Level 1 Kanji in JIS X 0208
	KANJI_UNICODE = 75, // Kanji in CJK Unified Ideographs block
	KANJI_KSX1001 = 90, // Kanji in KS X 1001
	KANJI_LEVEL_2 = 100, // Level 2+ Kanji in JIS X 0208
	KANJI_CP932 = 200, // Kanji in CP932
};

class UTF8Heuristic;
class CP850Heuristic;
class CP923Heuristic;
class CP932Heuristic;
class CP949Heuristic;

#pragma region Definitions

// Base heuristic
class StringEncodingHeuristic
{
protected:
	StringEncodingHeuristic() = default;
	StringEncodingHeuristic(const StringEncodingHeuristic&) = delete;

public:
	using Score = int32_t;

	virtual StringEncoding GetEncoding() const { return StringEncoding::Unknown; }

	inline Score GetScore() const { return m_score; }
	inline size_t GetCount() const { return m_count; }

	inline bool IsValid() const { return GetScore() >= 0; }

	virtual bool Consume(const uint8_t ch) = 0;
	virtual bool Finalize() = 0;

	// Operators can be confusing (lower score: better)
	inline bool Beats(const StringEncodingHeuristic& that) const { return !that.IsValid() || IsValid() && m_score < that.m_score; }

	void DebugPrint() const;

protected:
	virtual CharClass GetCharClass(const uint16_t ch) const = 0;
	inline bool Process(const uint16_t ch) { return Process(GetCharClass(ch)); }

	inline bool Process(CharClass charClass)
	{
		if (charClass == CharClass::INVALID)
		{
			MarkInvalid();
			return false;
		}
		else
		{
			m_score += static_cast<Score>(charClass);
			++m_count;

			return true;
		}
	}

	inline void MarkInvalid() { m_score = -1; }

	size_t m_count = 0;
	Score m_score = 0;
};

// A heuristic which denies every input
class NullHeuristic : public StringEncodingHeuristic
{
public:
	NullHeuristic() : StringEncodingHeuristic() { MarkInvalid(); }

	bool Consume(const uint8_t ch) override { return false; }
	bool Finalize() override { return false; }

protected:
	CharClass GetCharClass(const uint16_t ch) const { return CharClass::INVALID; }
};

// A heuristic for single-byte character encodings
class SingleByteEncodingHeuristic : public StringEncodingHeuristic
{
public:
	bool Consume(const uint8_t ch) override { return Process(ch); }
	bool Finalize() override { return IsValid(); }
};

// A heuristic for encodings with one or two bytes per character
class TwoByteEncodingHeuristic : public StringEncodingHeuristic
{
public:
	bool Consume(const uint8_t ch) override;
	bool Finalize() override { if (m_hi) MarkInvalid(); return IsValid(); }

protected:
	virtual bool RequiresSecondByte(const uint8_t ch) const = 0;

	uint8_t m_hi = 0;
};

template<typename Heuristics, typename... RestHeuristics>
class StringEncodingHeuristicCollection;

template<typename Heuristic>
class StringEncodingHeuristicCollection<Heuristic>
{
public:
	StringEncodingHeuristicCollection() = default;
	inline const StringEncodingHeuristic& GetBestHeuristic() const { return m_head; }
	inline void Consume(const char c) { if(m_head.IsValid()) m_head.Consume(c); }
	inline void Finalize() { m_head.Finalize(); }

	inline void DebugPrint() const { m_head.DebugPrint(); }

protected:
	Heuristic m_head;
};

template<typename Heuristic, typename... RestHeuristics>
class StringEncodingHeuristicCollection
{
public:
	StringEncodingHeuristicCollection() = default;
	inline const StringEncodingHeuristic& GetBestHeuristic() const
	{
		const StringEncodingHeuristic& head = m_head;
		const StringEncodingHeuristic& restBest = m_rest.GetBestHeuristic();

		return restBest.Beats(head) ? restBest : head;
	}
	inline void Consume(const char c) { if(m_head.IsValid()) m_head.Consume(c); m_rest.Consume(c); }
	inline void Finalize() { m_head.Finalize(); m_rest.Finalize(); }

	inline void DebugPrint() const { m_head.DebugPrint(); m_rest.DebugPrint(); }

protected:
	Heuristic m_head;
	StringEncodingHeuristicCollection<RestHeuristics...> m_rest;
};

template<typename HeuristicCollection, typename... RestHeuristicCollections>
class TieredStringEncodingHeuristic;

template<typename HeuristicCollection>
class TieredStringEncodingHeuristic<HeuristicCollection>
{
public:
	TieredStringEncodingHeuristic() = default;
	inline const StringEncodingHeuristic& GetBestHeuristic() const
	{
		return m_collection.GetBestHeuristic();
	}

	inline void Consume(const char ch) { m_collection.Consume(ch); }
	inline void Finalize() { m_collection.Finalize(); }

	inline void DebugPrint() const { m_collection.DebugPrint(); }

protected:
	HeuristicCollection m_collection;
};

template<typename HeuristicCollection, typename... RestHeuristicCollections>
class TieredStringEncodingHeuristic
{
public:
	TieredStringEncodingHeuristic() = default;
	inline const StringEncodingHeuristic& GetBestHeuristic() const
	{
		const StringEncodingHeuristic& heuristic = m_collection.GetBestHeuristic();
		if (heuristic.IsValid()) return heuristic;

		return m_rest.GetBestHeuristic();
	}

	inline void Consume(const char ch) { m_collection.Consume(ch); m_rest.Consume(ch); }
	inline void Finalize() { m_collection.Finalize(); m_rest.Finalize(); }
	
	inline void DebugPrint() const { m_collection.DebugPrint(); m_rest.DebugPrint(); }

protected:
	HeuristicCollection m_collection;
	TieredStringEncodingHeuristic<RestHeuristicCollections...> m_rest;
};

#pragma endregion

#pragma region Heuristics

class UTF8Heuristic : public StringEncodingHeuristic
{
public:
	StringEncoding GetEncoding() const override { return StringEncoding::UTF8; }

	bool Consume(const uint8_t ch) override;
	bool Finalize() override { if (m_remaining) MarkInvalid(); return IsValid();}

protected:
	CharClass GetCharClass(const uint16_t ch) const override;

	uint32_t m_currChar = 0;
	uint8_t m_remaining = 0;
};

class CP850Heuristic : public SingleByteEncodingHeuristic
{
public:
	StringEncoding GetEncoding() const override { return StringEncoding::CP850; }

protected:
	CharClass GetCharClass(const uint16_t ch) const override;
};

class CP923Heuristic : public SingleByteEncodingHeuristic
{
public:
	StringEncoding GetEncoding() const override { return StringEncoding::CP923; }

protected:
	CharClass GetCharClass(const uint16_t ch) const override;
};

class CP932Heuristic : public TwoByteEncodingHeuristic
{
public:
	StringEncoding GetEncoding() const override { return StringEncoding::CP932; }

protected:
	bool RequiresSecondByte(const uint8_t ch) const override;
	CharClass GetCharClass(const uint16_t ch) const override;
};

class CP949Heuristic : public TwoByteEncodingHeuristic
{
public:
	StringEncoding GetEncoding() const override { return StringEncoding::CP949; }

private:
	CharClass GetEUCKRCharClass(const uint16_t ch) const;

protected:
	bool RequiresSecondByte(const uint8_t ch) const override;
	CharClass GetCharClass(const uint16_t ch) const override;
};

// Umm... Let's ignore JIS X 0212 for now...
class CP954Heuristic : public TwoByteEncodingHeuristic
{
public:
	StringEncoding GetEncoding() const override { return StringEncoding::CP954; }

protected:
	bool RequiresSecondByte(const uint8_t ch) const override;
	CharClass GetCharClass(const uint16_t ch) const override;
};

#pragma endregion