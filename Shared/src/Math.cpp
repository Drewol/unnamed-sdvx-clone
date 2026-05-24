#include "stdafx.h"
#include "Math.hpp"
#include "Utility.hpp"

namespace Math
{
	float AngularDifference(float a, float b) noexcept
	{
		float sign = 1.0f;
		if(a > b)
		{
			Utility::Swap(a, b);
			sign = -1.0f;
		}
		b -= a;
		if(b > pi)
			b = b - pi * 2;
		return b * sign;
	}
}