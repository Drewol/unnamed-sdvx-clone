#pragma once

#include <math.h>
#include <cmath>

#include <type_traits>

namespace Math
{
	// Floating point PI constant
	constexpr float pi = 3.14159265359f;
	constexpr float e = 2.71828182846f;
	constexpr float degToRad = (1.0f / 180.0f) * pi;
	constexpr float radToDeg = (1.0f / pi) * 180.0f;

	// Templated min
	template<typename T>
	[[nodiscard]]
	static constexpr T Min(T a, T b) noexcept
	{
		if(a < b)
			return a;
		else
			return b;
	}

	// Templated max
	template<typename T>
	[[nodiscard]]
	static constexpr T Max(T a, T b) noexcept
	{
		if(a > b)
			return a;
		else
			return b;
	}

	template<typename T>
	[[nodiscard]]
	static constexpr T Clamp(T v, T min, T max) noexcept
	{
		if(v < min)
			return min;
		if(v > max)
			return max;
		return v;
	}

	template<typename T>
	[[nodiscard]]
	static constexpr std::enable_if_t<std::is_arithmetic<T>::value, T> Lerp(T a, T b, T t) noexcept
	{
		// Not the spec-confirming lerp, but good enough
		return t == 0 ? a : t == 1 ? b : a + t * (b - a);
	}

	// Templated Greatest common divisor
	template<typename T>
	[[nodiscard]]
	static constexpr T GCD(T a, T b) noexcept
	{
		return b == 0 ? a : gcd(b, a % b);
	}

	// Gets the sign of a value
	template<typename T>
	[[nodiscard]]
	T Sign(T val) noexcept
	{
		return static_cast<T>((T(0) < val) - (val < T(0)));
	}

	// Returns angular difference between 2 angles (radians)
	// closest path
	// Values must be in the range [0, 2pi]
	[[nodiscard]]
	float AngularDifference(float a, float b) noexcept;

	template<typename T>
	[[nodiscard]]
	T Floor(T t) noexcept
	{
		return std::floor(t);
	}

	template<typename T>
	[[nodiscard]]
	T Ceil(T t) noexcept
	{
		return std::ceil(t);
	}

	template<typename T>
	[[nodiscard]]
	T Round(T t) noexcept
	{
		return std::round(t);
	}

	template<typename T>
	[[nodiscard]]
	int RoundToInt(T t) noexcept
	{
		return static_cast<int>(t + 0.5);
	}

	template<typename T>
	[[nodiscard]]
	T BeatInMS(T bpm) noexcept
	{
		return static_cast<T>(60000) / bpm;
	}

	template<typename T>
	[[nodiscard]]
	T TickInMS(T bpm, T tpqn) noexcept
	{
		return BeatInMS(bpm) / tpqn;
	}

	template<typename T>
	[[nodiscard]]
	T TicksFromMS(T ms, T bpm, T tpqn) noexcept
	{
		return ms / TickInMS(bpm, tpqn);
	}

	template<typename T>
	[[nodiscard]]
	T MSFromTicks(T ticks, T bpm, T tpqn) noexcept
	{
		return TickInMS(bpm, tpqn) * ticks;
	}
}
