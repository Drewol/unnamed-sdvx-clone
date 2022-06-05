#include "stdafx.h"
#include "Math.hpp"
#include "Utility.hpp"

namespace Math
{
	const float pi = 3.14159265359f;
	const float e = 2.71828182846f;
	extern const float degToRad = (1.0f / 180.0f) * pi;
	extern const float radToDeg = (1.0f / pi) * 180.0f;

	float AngularDifference(float a, float b)
	{
		float sign = 1.0f;
		if (a > b)
		{
			Utility::Swap(a, b);
			sign = -1.0f;
		}
		b -= a;
		if (b > pi)
			b = b - pi * 2;
		return b * sign;
	}
}
