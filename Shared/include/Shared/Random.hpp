#pragma once

/*
	Simple random number generation
*/
namespace Random
{
	[[nodiscard]]
	float Float();
	[[nodiscard]]
	float FloatRange(float min, float max);
	[[nodiscard]]
	int32 IntRange(int32 min, int32 max);
}