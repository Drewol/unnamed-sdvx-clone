#include "stdafx.h"
#include "Random.hpp"
#include <random>
#include <ctime>

namespace Random
{
	using std::mt19937;
	using std::uniform_int_distribution;
	using std::uniform_real_distribution;

	mt19937 gen((uint32)time(0));

	float Float()
	{
		uniform_real_distribution<float> floatDist;
		return floatDist(gen);
	}
	float FloatRange(float min, float max)
	{
		uniform_real_distribution<float> floatDist(min, max);
		return floatDist(gen);
	}
	int32 IntRange(int32 min, int32 max)
	{
		uniform_int_distribution<int32> intDist(min, max);
		return intDist(gen);
	}

}
