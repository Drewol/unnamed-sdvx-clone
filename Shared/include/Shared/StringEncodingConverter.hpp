#pragma once

#include "Shared/StringEncoding.hpp"
#include "Shared/String.hpp"

// A wrapper for iconv
class StringEncodingConverter final
{
public:
	[[nodiscard]]
	static String ToUTF8(StringEncoding encoding, const char* str, const size_t str_len);
	[[nodiscard]]
	inline static String ToUTF8(StringEncoding encoding, const char* str) { return ToUTF8(encoding, str, strlen(str)); }
	[[nodiscard]]
	inline static String ToUTF8(StringEncoding encoding, const String& str) { return ToUTF8(encoding, str.c_str(), str.size()); }

	static String PathnameToUTF8(StringEncoding encoding, struct archive_entry* entry);

	[[nodiscard]]
	inline static constexpr bool NeedsConversion(StringEncoding encoding) noexcept;

private:
	[[nodiscard]]
	inline static constexpr const char* GetIConvArg(const StringEncoding encoding) noexcept;

	StringEncodingConverter() = delete;
	~StringEncodingConverter() = delete;

	constexpr static size_t BUFFER_SIZE = 64;
};

inline constexpr bool StringEncodingConverter::NeedsConversion(StringEncoding encoding) noexcept
{
	switch (encoding)
	{
	case StringEncoding::Unknown:
	case StringEncoding::UTF8:
		return false;
	default:
		return true;
	}
}

inline constexpr const char* StringEncodingConverter::GetIConvArg(const StringEncoding encoding) noexcept
{
	switch (encoding)
	{
	case StringEncoding::UTF8: return "utf8";
	case StringEncoding::CP850: return "cp850";
	case StringEncoding::CP923: return "cp923";
	case StringEncoding::CP932: return "cp932";
	case StringEncoding::CP949: return "cp949";
	case StringEncoding::CP954: return "EUC-JP";

	case StringEncoding::Unknown: default: return "";
	}
}