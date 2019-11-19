#pragma once
#include <math.h>
#include <cmath>

namespace Math
{
	// Floating point PI constant
	extern const float pi;
	extern const float e;
	extern const float degToRad;
	extern const float radToDeg;

	// Templated min
	template<typename T>
	T Min(T a, T b)
	{
		if(a < b)
			return a;
		else
			return b;
	}

	// Templated max
	template<typename T>
	T Max(T a, T b)
	{
		if(a > b)
			return a;
		else
			return b;
	}

	template<typename T>
	T Clamp(T v, T min, T max)
	{
		if(v < min)
			return min;
		if(v > max)
			return max;
		return v;
	}

	// Templated Greatest common divisor
	template<typename T>
	T GCD(T a, T b)
	{
		return b == 0 ? a : gcd(b, a % b);
	}

	// Gets the sign of a value
	template <typename T> T Sign(T val) 
	{
		return (T)((T(0) < val) - (val < T(0)));
	}

	// Returns angular difference between 2 angles (radians)
	// closest path
	// Values must be in the range [0, 2pi]
	float AngularDifference(float a, float b);

	template<typename T>
	T Floor(T t)
	{
		return std::floor(t);
	}
	template<typename T>
	T Ceil(T t)
	{
		return std::ceil(t);
	}
	template<typename T>
	T Round(T t)
	{
		return std::round(t);
	}
}
