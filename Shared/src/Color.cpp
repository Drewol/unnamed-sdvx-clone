#include "stdafx.h"
#include "Color.hpp"
#include "Math.hpp"

Color::Color(const VectorMath::VectorBase<uint8, 4>& icolor)
{
	*this = Color(
		icolor.x / 255.0f,
		icolor.y / 255.0f,
		icolor.z / 255.0f,
		icolor.w / 255.0f
	);
}

Color::Color(float r, float g, float b) : Color(r, g, b, 1.0f)
{
}

Color::Color(float all) : Color(all, all, all, 1.0f)
{
}
bool Color::operator ==(const Color& other) const
{
	return this->x == other.x && this->y == other.y && this->z == other.y && this->w == other.w;
}
bool Color::operator !=(const Color& other) const
{
	return !(*this == other);
}
Colori Color::ToRGBA8() const
{
	return Colori(*this * 255.0f);
}

Color Color::WithAlpha(float a) const
{
	return Color(x, y, z, a);
}

Color Color::FromHSV(float hue, float saturation, float value)
{
	float chroma = value * saturation;
	float hue2 = fmodf(hue, 360.0f) / 60.0f;
	float hueMod = fmodf(hue2, 2) - 1;
	float m = value - chroma;
	float x = chroma * (1 - fabsf(hueMod));
	Color c;
	switch ((int)floorf(hue2)) {
	case 0:
		c = Color(chroma, x, 0.0f);
		break;
	case 1:
		c = Color(x, chroma, 0.0f);
		break;
	case 2:
		c = Color(0.0f, chroma, x);
		break;
	case 3:
		c = Color(0.0f, x, chroma);
		break;
	case 4:
		c = Color(x, 0.0f, chroma);
		break;
	case 5:
		c = Color(chroma, 0.0f, x);
		break;
	}
	c += Color(m, m, m, 0);
	return c;
}

// Color constants
const Color Color::White = Color(1, 1, 1, 1);
const Color Color::Black = Color(0, 0, 0, 1);
const Color Color::Red = Color(1, 0, 0, 1);
const Color Color::Green = Color(0, 1, 0, 1);
const Color Color::Blue = Color(0, 0, 1, 1);
const Color Color::Yellow = Color(1, 1, 0, 1);
const Color Color::Magenta = Color(1, 0, 1, 1);
const Color Color::Cyan = Color(0, 1, 1, 1);

Colori::Colori(uint8 r, uint8 g, uint8 b) : Colori(r, g, b, 255)
{

}



// Converted integer constants
const Colori Colori::Cyan = Color::Cyan;
const Colori Colori::Magenta = Color::Magenta;
const Colori Colori::Yellow = Color::Yellow;
const Colori Colori::Blue = Color::Blue;
const Colori Colori::Green = Color::Green;
const Colori Colori::Red = Color::Red;
const Colori Colori::Black = Color::Black;
const Colori Colori::White = Color::White;
