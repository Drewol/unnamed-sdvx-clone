#pragma once

#include <math.h>
#include <cmath>

#include <type_traits>

namespace Math
{
	// Floating point PI constant
	extern const float pi;
	extern const float e;
	extern const float degToRad;
	extern const float radToDeg;

	// Templated min
	template<typename T>
	static T Min(T a, T b)
	{
		if(a < b)
			return a;
		else
			return b;
	}

	// Templated max
	template<typename T>
	static T Max(T a, T b)
	{
		if(a > b)
			return a;
		else
			return b;
	}

	template<typename T>
	static T Clamp(T v, T min, T max)
	{
		if(v < min)
			return min;
		if(v > max)
			return max;
		return v;
	}

	template<typename T>
	static constexpr std::enable_if_t<std::is_arithmetic<T>::value, T> Lerp(T a, T b, T t) noexcept
	{
		// Not the spec-confirming lerp, but good enough
		return t == 0 ? a : t == 1 ? b : a + t * (b - a);
	}

	// Templated Greatest common divisor
	template<typename T>
	static T GCD(T a, T b)
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
	template<typename T>
	int RoundToInt(T t)
	{
		return static_cast<int>(t + 0.5);
	}
	template<typename T>
	T BeatInMS(T bpm)
	{
		return (T)60000 / bpm;
	}
	template<typename T>
	T TickInMS(T bpm, T tpqn)
	{
		return BeatInMS(bpm) / tpqn;
	}
	template<typename T>
	T TicksFromMS(T ms, T bpm, T tpqn)
	{
		return ms / TickInMS(bpm, tpqn);
	}
	template<typename T>
	T MSFromTicks(T ticks, T bpm, T tpqn)
	{
		return TickInMS(bpm, tpqn) * ticks;
	}
}
