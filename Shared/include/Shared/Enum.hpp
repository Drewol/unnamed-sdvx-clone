/*
	Some macro functionality that allows conversion from an enum to string values and the other way around
*/
#pragma once

/*
	Parses comma separated list into enum string<=>value mapping
*/
template<typename EnumType>
class EnumStringMap
{
	Map<EnumType, String> names;
	Map<String, EnumType> rev;

public:
	EnumStringMap(const char* enumInit)
	{
		String src = enumInit;
		size_t split = 0;
		size_t idLast = 0;
		size_t badVal = -1;
		EnumType e;
		for (uint32_t i = 0; split != badVal && !src.empty(); i++)
		{
			split = src.find(',');
			String seg = (split == badVal) ? src : src.substr(0, split);
			size_t assignment = seg.find("=");
			if (assignment != badVal)
			{
				String valueStr = seg.substr(assignment + 1);
				seg = seg.substr(0, assignment);
				size_t charValue = valueStr.find('\'');
				if (charValue != badVal) // Probably a char value
					idLast = (size_t)valueStr[charValue + 1];
				else // Hex or decimal value
					idLast = strtol(*valueStr, NULL, 0);
			}
			e = (EnumType)idLast++;
			seg.Trim();
			names.Add(e, seg);
			rev.Add(seg, e);
			src = (split == badVal) ? src : src.substr(split + 1);
		}
	}

	inline auto begin() { return names.begin(); }
	inline auto begin() const { return names.begin(); }
	inline auto end() { return names.end(); }
	inline auto end() const { return names.end(); }

	const String& ToString(EnumType e) const
	{
		static String dummy = "<invalid>";
		auto it = names.find(e);
		return it == names.end() ? dummy : it->second;
	}

	EnumType FromString(const String& str) const
	{
		auto it = rev.find(str);
		return it == rev.end() ? (EnumType)-1 : it->second;
	}
};

/*
Template class with helper functions to convert bitflag enums to string and parse them from a string
*/
template<typename EnumType>
class BitflagEnumConversion
{
public:
	static String ToString(EnumStringMap<EnumType>& stringMap, EnumType e)
	{
		String result;
		uint32 mask = 1;
		for (uint32 i = 0; i < 32; i++)
		{
			if ((uint32)e & mask)
			{
				if (!result.empty())
					result += " | ";
				result += stringMap.ToString((EnumType)mask);
			}
			mask <<= 1;
		}
		return result;
	}
	static EnumType FromString(EnumStringMap<EnumType>& stringMap, String str)
	{
		uint32 result = 0;
		size_t next = 0;
		size_t badVal = -1;
		while (next != badVal)
		{
			next = str.find('|');
			String current = str;
			if (next != badVal)
			{
				current = str.substr(0, next);
				str = str.substr(next + 1);
			}
			current.Trim();

			uint32 currentElem = (uint32)stringMap.FromString(current);
			result = result | currentElem;
		}
		return (EnumType)result;
	}
};

// Convers a macro argument list to a string
#define Enum_ArgsToString(_v, ...) #_v","#__VA_ARGS__

// Declares bitwise oprations on the given enum type
#define DeclareBitwiseEnumOps(_n)\
_n operator|(_n l, _n r);\
_n operator&(_n l, _n r);\
_n operator^(_n l, _n r);\
_n& operator|=(_n& l, _n r);\
_n& operator&=(_n& l, _n r);\
_n& operator^=(_n& l, _n r);

// Should be used when using DefineBitflagEnum(EnumType)
// Implements bitwise oprations declared with DeclareBitwiseEnumOps(EnumType)
#define ImplementBitflagEnum(_n)\
_n operator|(_n l, _n r) { return (_n)((uint32)(l) | (uint32)(r));}\
_n operator&(_n l, _n r) { return (_n)((uint32)(l) & (uint32)(r));}\
_n operator^(_n l, _n r) { return (_n)((uint32)(l) ^ (uint32)(r));}\
_n& operator|=(_n& l, _n r) { l = (_n)((uint32)(l) | (uint32)(r)); return l;}\
_n& operator&=(_n& l, _n r) { l = (_n)((uint32)(l) & (uint32)(r)); return l;}\
_n& operator^=(_n& l, _n r) { l = (_n)((uint32)(l) ^ (uint32)(r)); return l;}

/*
	Define that creates a class named Enum_<enum name> that allows from string and to string conversion
	Example usage:

	Enum(TestEnum,
		None,
		A = 0x1,
		B = 0x2,
		C = 0x4,
		D = 0x8);

	void main()
	{
		String str = Enum_TestEnum::ToString(TestEnum::A);
		TestEnum e = Enum_TestEnum::FromString("B");
	}
*/
#define DefineEnum(_n, ...) enum class _n : uint32 { __VA_ARGS__, _Length };\
struct Enum_##_n\
{\
	typedef _n EnumType;\
	static EnumStringMap<EnumType>& GetMap()\
	{\
		static EnumStringMap<EnumType> m(Enum_ArgsToString(__VA_ARGS__));\
		return m;\
	}\
	static const String& ToString(EnumType e)\
	{\
		return GetMap().ToString(e);\
	}\
	static EnumType FromString(const String& str)\
	{\
		return GetMap().FromString(str);\
	}\
};

/*
	Works in the same way as the Enum() macro and additionally implements bitwise operations for the given enum type
		and allows string<=>value conversions for multiple flags at the same time
*/
#define DefineBitflagEnum(_n, ...) enum class _n : uint32 { __VA_ARGS__, _Length };\
DeclareBitwiseEnumOps(_n)\
struct Enum_##_n\
{\
	typedef _n EnumType;\
	static EnumStringMap<EnumType>& GetMap()\
	{\
		static EnumStringMap<EnumType> m(Enum_ArgsToString(__VA_ARGS__));\
		return m;\
	}\
	static String ToString(EnumType e)\
	{\
		return BitflagEnumConversion<EnumType>::ToString(GetMap(), e);\
	}\
	static EnumType FromString(const String& str)\
	{\
		return BitflagEnumConversion<EnumType>::FromString(GetMap(), str);\
	}\
	bool HasFlag(EnumType in, EnumType flag)\
	{\
		return ((in) & (flag)) == (flag);\
	}\
};
