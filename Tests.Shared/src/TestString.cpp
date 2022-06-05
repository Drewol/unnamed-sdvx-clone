#include <Shared/Shared.hpp>
#include <Shared/String.hpp>
#include <Tests/Tests.hpp>

std::vector<std::pair<const char*, const wchar_t*>> testCases({
	{"", L""},
	{"Hello, world!", L"Hello, world!"},

	// Borute in Katakana
	{"\xe3\x83\x9c\xe3\x83\xab\xe3\x83\x86", L"\x30DC\x30EB\x30C6"},
	// Sound Voltex in Hangul
	{"\xec\x82\xac\xec\x9a\xb4\xeb\x93\x9c \xeb\xb3\xbc\xed\x85\x8d\xec\x8a\xa4", L"\xC0AC\xC6B4\xB4DC\x0020\xBCFC\xD14D\xC2A4"},

	// Some SDVX song names
	{"\xce\xa3mbry\xc3\x98", L"\x03A3\x006D\x0062\x0072\x0079\x00D8"},
	{"Lachryma\xe3\x80\x8aRe:Queen'M\xe3\x80\x8b", L"Lachryma\x300ARe:Queen'M\x300B"},
	{"HE4VEN \xef\xbd\x9e\xe5\xa4\xa9\xe5\x9b\xbd\xe3\x81\xb8\xe3\x82\x88\xe3\x81\x86\xe3\x81\x93\xe3\x81\x9d\xef\xbd\x9e", L"HE4VEN \xFF5E\x5929\x56FD\x3078\x3088\x3046\x3053\x305D\xFF5E"},

	// Some emojis
	{"\xF0\x9F\x91\x8C\xF0\x9F\x94\xA5", L"\xD83D\xDC4C\xD83D\xDD25"},
	});

Test("String.ConvertToWString")
{
	for (auto& test : testCases)
	{
		TestEnsure(Utility::ConvertToWString(test.first) == test.second);
	}
}

Test("String.ConvertToUTF8")
{
	for (auto& test : testCases)
	{
		TestEnsure(Utility::ConvertToUTF8(test.second) == test.first);
	}
}

Test("String.ConvertToRoundtrip")
{
	for (auto& test : testCases)
	{
		TestEnsure(Utility::ConvertToUTF8(Utility::ConvertToWString(test.first)) == test.first);
		TestEnsure(Utility::ConvertToWString(Utility::ConvertToUTF8(test.second)) == test.second);
	}
}
