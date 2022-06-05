#include "stdafx.h"
#include "ConfigEntry.hpp"

String IntConfigEntry::ToString() const
{
	return Utility::Sprintf("%d", data);
}
void IntConfigEntry::FromString(const String& str)
{
	data = atoi(*str);
}

String BoolConfigEntry::ToString() const
{
	return data ? "True" : "False";
}
void BoolConfigEntry::FromString(const String& str)
{
	data = (str == "True");
}

String StringConfigEntry::ToString() const
{
	return "\"" + data + "\"";
}
void StringConfigEntry::FromString(const String& str)
{
	data = str;
	data.Trim('"');
}

String FloatConfigEntry::ToString() const
{
	return Utility::Sprintf("%f", data);
}
void FloatConfigEntry::FromString(const String& str)
{
	data = (float)atof(*str);
}

String ColorConfigEntry::ToString() const
{
	Colori ci = data.ToRGBA8();
	return Utility::Sprintf("%02X%02X%02X%02X", ci.x, ci.y, ci.z, ci.w);
}
void ColorConfigEntry::FromString(const String& str)
{
	int r, g, b, a;
	sscanf(str.substr(0, 2).c_str(), "%X", &r);
	sscanf(str.substr(2, 2).c_str(), "%X", &g);
	sscanf(str.substr(4, 2).c_str(), "%X", &b);
	sscanf(str.substr(6, 2).c_str(), "%X", &a);
	data = Colori(r, g, b, a);
}
