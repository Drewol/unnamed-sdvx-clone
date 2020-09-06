#include "stdafx.h"
#include "StringEncodingHeuristic.hpp"

#include "Shared/Log.hpp"

static inline CharClass GetAsciiCharClass(uint8_t ch)
{
	if (0x30 <= ch && ch <= 0x39 || 0x41 <= ch && ch <= 0x5A || 0x61 <= ch && ch <= 0x7A)
		return CharClass::ALNUM;
	else if (ch == 0x09 || ch == 0x0A || ch == 0x0D || 0x20 <= ch && ch <= 0x7E)
		return CharClass::ASCII_OTHER_CHARS;
	else
		return CharClass::INVALID;
}

bool TwoByteEncodingHeuristic::Consume(const uint8_t ch)
{
	if (!IsValid())
	{
		return false;
	}

	if (m_hi == 0)
	{
		if (RequiresSecondByte(ch))
		{
			m_hi = ch;
			return true;
		}

		return Process(ch);
	}

	uint16_t curr = (static_cast<uint16_t>(m_hi) << 8) | ch;
	m_hi = 0;

	return Process(curr);
}

// UTF-8

bool UTF8Heuristic::Consume(const uint8_t ch)
{
	if (m_remaining == 0)
	{
		if (ch < 0x80) return Process(ch);
		if ((ch >> 6) == 0b10)
		{
			MarkInvalid();
			return false;
		}
		if (ch < 0xE0)
		{
			m_remaining = 1;
			m_currChar = ch & 0x1F;
		}
		else if (ch < 0xF0)
		{
			m_remaining = 2;
			m_currChar = ch & 0x0F;
		}
		else if (ch < 0xF8)
		{
			m_remaining = 3;
			m_currChar = ch & 0x07;
		}
		else
		{
			MarkInvalid();
			return false;
		}
	}
	else
	{
		if ((ch >> 6) != 0b10)
		{
			MarkInvalid();
			return false;
		}

		m_currChar = ((m_currChar << 6) | (ch & 0x3F));
		if (--m_remaining == 0)
		{
			if (m_currChar < 0x10000) return Process(static_cast<uint16_t>(m_currChar));
			else if (m_currChar >= 0x110000)
			{
				MarkInvalid();
				return false;
			}
			else
			{
				Process(CharClass::OTHER_CHARS);
				return true;
			}
		}
	}

	return true;
}

CharClass UTF8Heuristic::GetCharClass(const uint16_t ch) const
{
	if (ch < 0x80)
		return GetAsciiCharClass(static_cast<uint8_t>(ch));

	if (0x3040 <= ch && ch <= 0x30FF)
		return CharClass::KANA;

	if (0x1100 <= ch && ch <= 0x11FF || 0xAC00 <= ch && ch <= 0xD7AF)
		return CharClass::HANGUL_UNICODE;

	if (0x4E00 <= ch && ch <= 0x9FFF)
		return CharClass::KANJI_UNICODE;

	// PUA
	if (0xE000 <= ch && ch <= 0xF8FF)
		return CharClass::PRIVATE_USE;

	// BOM
	if (ch == 0xFEFF)
		return CharClass::IGNORABLE;

	if (0xFF10 <= ch && ch <= 0xFF19 || 0xFF21 <= ch && ch <= 0xFF3A || 0xFF41 <= ch && ch <= 0xFF5A)
		return CharClass::ALNUM;

	// Half-width characters
	if (0xFF66 <= ch && ch <= 0xFF9D)
		return CharClass::KANA;

	if (0xFFA1 <= ch && ch <= 0xFFDC)
		return CharClass::HANGUL_UNICODE;

	return CharClass::OTHER_CHARS;
}

// CP850

CharClass CP850Heuristic::GetCharClass(const uint16_t ch) const
{
	if (ch < 0x80) return GetAsciiCharClass(static_cast<uint8_t>(ch));
	return CharClass::OTHER_CHARS;
}


// CP923
// Reference: https://charset.fandom.com/ko/wiki/ISO/IEC_8859-15

CharClass CP923Heuristic::GetCharClass(const uint16_t ch) const
{
	if (ch < 0x80) return GetAsciiCharClass(static_cast<uint8_t>(ch));

	if (ch < 0xA0) return CharClass::INVALID;
	return CharClass::OTHER_CHARS;
}

// CP932
// Reference 1: https://charset.fandom.com/ko/wiki/CP932
// Reference 2: https://www.sljfaq.org/afaq/encodings.html

bool CP932Heuristic::RequiresSecondByte(const uint8_t ch) const
{
	return 0x81 <= ch && ch <= 0x9F || 0xE0 <= ch && ch <= 0xFC;
}

CharClass CP932Heuristic::GetCharClass(const uint16_t ch) const
{
	// JIS X 0201
	if (ch <= 0x80) return GetAsciiCharClass(static_cast<uint8_t>(ch));

	if (0xA6 <= ch && ch <= 0xDD)
		return CharClass::KANA;
	if (0xA1 <= ch && ch <= 0xDF)
		return CharClass::OTHER_CHARS;

	// CP932 extension
	if (0x8740 <= ch && ch <= 0x879C)
		return CharClass::OTHER_CHARS;
	if (0xED40 <= ch && ch <= 0xEEFC || 0xFA40 <= ch && ch <= 0xFC4B)
		return CharClass::KANJI_CP932;
	if (0xF040 <= ch && ch <= 0xF9FC)
		return CharClass::PRIVATE_USE;

	// JIS X 0208
	if (0x84BF <= ch && ch <= 0x889E || 0x9873 <= ch && ch <= 0x989E || 0xEAA5 <= ch)
		return CharClass::INVALID;

	if (0x824F <= ch && ch <= 0x8258 || 0x8260 <= ch && ch <= 0x8279 || 0x8281 <= ch && ch <= 0x829A)
		return CharClass::ALNUM;
	if (0x829F <= ch && ch <= 0x82F1 || 0x8340 <= ch && ch <= 8396)
		return CharClass::KANA;
	if (0x889F <= ch && ch <= 0x9872)
		return CharClass::KANJI_LEVEL_1;
	if (0x989F <= ch && ch <= 0xEAA4)
		return CharClass::KANJI_LEVEL_2;

	return CharClass::OTHER_CHARS;
}

// CP949
// Reference: https://charset.fandom.com/ko/wiki/CP949

CharClass CP949Heuristic::GetEUCKRCharClass(const uint16_t ch) const
{
	// Complete Hangul
	if (0xB0A1 <= ch && ch <= 0xC8FE)
		return CharClass::HANGUL_KSX1001;

	// Incomplete Hangul
	if (0xA4A1 <= ch && ch <= 0xA4FE)
		return CharClass::HANGUL_KSX1001;

	// Full-width alnum
	if (0xA3B0 <= ch && ch <= 0xA3B9 || 0xA3C1 <= ch && ch <= 0xA3DA || 0xA3E1 <= ch && ch <= 0xA3FA)
		return CharClass::ALNUM;

	// Kanji (Every kanji will be regarded as rare even though some are not...)
	if (0xCAA1 <= ch && ch <= 0xFDFE)
		return CharClass::KANJI_KSX1001;

	// Kana
	if (0xAAA1 <= ch && ch <= 0xAAF3 || 0xABA1 <= ch && ch <= 0xABF6)
		return CharClass::KANA;

	// PUA
	if (0xC9A1 <= ch && ch <= 0xC9FE || 0xFEA1 <= ch && ch <= 0xFEFE)
		return CharClass::PRIVATE_USE;

	// Invalid region
	if (0xA2E9 <= ch && ch <= 0xA2FE || 0xA6E5 <= ch && ch <= 0xA6FE || 0xACF2 <= ch)
		return CharClass::INVALID;

	return CharClass::OTHER_CHARS;
}

bool CP949Heuristic::RequiresSecondByte(const uint8_t ch) const
{
	return 0x81 <= ch && ch <= 0xC6 || 0xA1 <= ch && ch <= 0xFE;
}

CharClass CP949Heuristic::GetCharClass(const uint16_t ch) const
{
	// KS X 1003
	if (ch < 0x80) return GetAsciiCharClass(static_cast<uint8_t>(ch));
	if (ch <= 0xFF) return CharClass::INVALID;

	const uint8_t hi = static_cast<uint8_t>(ch >> 8);
	const uint8_t lo = static_cast<uint8_t>(ch & 0xFF);

	// KS X 1001
	if (0xA1 <= hi && hi <= 0xFE && 0xA1 <= lo && lo <= 0xFE)
	{
		return GetEUCKRCharClass(ch);
	}

	// CP949 extension
	if (lo < 0x41 || 0x5A < lo && lo < 0x61 || 0x7A < lo && lo < 0x81 || lo == 0xFF)
	{
		return CharClass::INVALID;
	}
	else if (0x8141 <= ch && ch <= 0xC652)
	{
		return CharClass::HANGUL_CP949;
	}

	return CharClass::INVALID;
}

bool CP954Heuristic::RequiresSecondByte(const uint8_t ch) const
{
	return ch == 0x8E || 0xA1 <= ch && ch <= 0xFE;
}

CharClass CP954Heuristic::GetCharClass(const uint16_t ch) const
{
	if (ch < 0x80) return GetAsciiCharClass(static_cast<uint8_t>(ch));
	if (ch < 0xFF) return CharClass::INVALID;

	const uint8_t hi = static_cast<uint8_t>(ch >> 8);
	const uint8_t lo = static_cast<uint8_t>(ch & 0xFF);

	// JIS X 0201-jp
	if (hi == 0x8E)
	{
		if (0xA1 <= lo && lo <= 0xA5 || lo == 0xDE || lo == 0xDF) return CharClass::OTHER_CHARS;
		if (0xA6 <= lo && lo <= 0xDD) return CharClass::KANA;
		return CharClass::INVALID;
	}

	if (hi < 0xA1 || hi > 0xFE || lo < 0xA1 || lo > 0xFE)
		return CharClass::INVALID;

	if (0xA2AF <= ch && ch <= 0xA2B9 || 0xA2C2 <= ch && ch <= 0xA2C9 || 0xA2D1 <= ch && ch <= 0xA2DB || 0xA2EB <= ch && ch <= 0xA2F1 || 0xA2FA <= ch && ch <= 0xA2FD)
		return CharClass::INVALID;

	if (ch <= 0xA2FE)
		return CharClass::OTHER_CHARS;

	if (0xA3B0 <= ch && ch <= 0xA3B9 || 0xA3C1 <= ch && ch <= 0xA3DA || 0xA3E1 <= ch && ch <= 0xA3FA)
		return CharClass::ALNUM;

	if (0xA4A1 <= ch && ch <= 0xA4F3 || 0xA5A1 <= ch && ch <= 0xA5F6)
		return CharClass::KANA;

	if (0xA6A1 <= ch && ch <= 0xA6B8 || 0xA6C1 <= ch && ch <= 0xA6D8 || 0xA7A1 <= ch && ch <= 0xA7C1 || 0xA7D1 <= ch && ch <= 0xA7F1 || 0xA8A1 <= ch && ch <= 0xA8C0)
		return CharClass::OTHER_CHARS;

	if (0xB0A1 <= ch && ch <= 0xCFD3 || 0xD0A1 <= ch && ch <= 0xF4A6)
		return CharClass::KANJI_LEVEL_1;

	return CharClass::INVALID;
}

// For debugging
void StringEncodingHeuristic::DebugPrint() const
{
	Logf("Score: [%d] / Length: [%d] / Encoding: [%s]", Logger::Severity::Info,
		static_cast<int>(m_score), static_cast<int>(m_count), GetDisplayString(GetEncoding()));
}