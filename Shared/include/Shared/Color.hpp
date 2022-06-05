#pragma once
#include "Shared/VectorMath.hpp"

/*
	Integer color class (RGBA8)
*/
class Colori : public VectorMath::VectorBase<uint8, 4>
{
public:
	using VectorMath::VectorBase<uint8, 4>::VectorBase;
	Colori() = default;
	// Constructor with alpha=1
	Colori(uint8 r, uint8 g, uint8 b);

	static const Colori White;
	static const Colori Black;
	static const Colori Red;
	static const Colori Green;
	static const Colori Blue;
	static const Colori Yellow;
	static const Colori Magenta;
	static const Colori Cyan;
};

/*
	Floating point color class (RGBA32)
*/
class Color : public Vector4
{
public:
	using Vector4::Vector4;
	Color() = default;
	// Constructor with alpha=1
	Color(float all);
	// Constructor with alpha=1
	Color(float r, float g, float b);
	Color(const VectorMath::VectorBase<uint8, 4>& icolor);
	bool operator ==(const Color& other) const;
	bool operator !=(const Color& other) const;
	Colori ToRGBA8() const;
	// Returns the same color, but with a different alpha value
	Color WithAlpha(float a) const;
	// Color from hue, saturation and value
	static Color FromHSV(float hue, float saturation, float value);

	static const Color White;
	static const Color Black;
	static const Color Red;
	static const Color Green;
	static const Color Blue;
	static const Color Yellow;
	static const Color Magenta;
	static const Color Cyan;
};

