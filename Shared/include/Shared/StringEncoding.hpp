#pragma once

/*
	An enum for common string encodings.
	
	When a new encoding must be supported, be sure to update these functions:
	- GetDisplayString
	- StringEncodingConverter::GetIConvArg
*/

enum class StringEncoding
{
	Unknown,
	UTF8, ASCII = UTF8,
	CP850, DOS_Latin1 = CP850,
	CP923, ISO8859_15 = CP923,
	CP932, ShiftJIS = CP932,
	CP949, EUC_KR = CP949,
	CP954, EUC_JP = CP954
};

constexpr const char* GetDisplayString(const StringEncoding encoding)
{
	switch (encoding)
	{
	case StringEncoding::UTF8: return "UTF-8";
	case StringEncoding::CP850: return "CP850 (aka DOS Latin 1)";
	case StringEncoding::CP923: return "CP923 (aka ISO 8859-15)";
	case StringEncoding::CP932: return "CP932 (or ShiftJIS)";
	case StringEncoding::CP949: return "CP949 (or EUC-KR)";
	case StringEncoding::CP954: return "CP954 (or EUC-JP)";
	case StringEncoding::Unknown: return "Unknown";
	default: return "[an unknown encoding]";
	}
}