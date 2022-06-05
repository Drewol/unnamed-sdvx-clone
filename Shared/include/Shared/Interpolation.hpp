#pragma once

namespace Interpolation
{
	enum Predefined
	{
		Linear,
		EaseOutQuad,
		EaseInQuad,
		EaseInOutQuad,
		EaseInCubic,
		EaseOutCubic,
		EaseInOutCubic,
		EaseInExpo,
		EaseOutExpo,
		EaseInOutExpo
	};

	/*
		Cubic bezier spline
		check http://cubic-bezier.com/ or http://easings.net/ for curves/predefined values
	*/
	class CubicBezier
	{
	public:
		CubicBezier() = default;
		CubicBezier(const CubicBezier&) = default;
		CubicBezier(CubicBezier&&) = default;
		CubicBezier& operator=(const CubicBezier& other) = default;
		CubicBezier(Predefined predefined);
		CubicBezier(float a, float b, float c, float d);
		CubicBezier(double a, double b, double c, double d);
		float Sample(float in) const;
		float operator()(float in) const;

	private:
		void m_Set(float a, float b, float c, float d);
		void m_Set(double a, double b, double c, double d);
		float a, b, c, d;
	};

	// Typedef for use with predefined functions
	typedef CubicBezier TimeFunction;

	template<typename T>
	T Lerp(T a, T b, float f, TimeFunction timeFunction = Linear)
	{
		return a + (b - a) * timeFunction(f);
	}
	int32 Lerp(int32 a, int32 b, float f, TimeFunction timeFunction = Linear);
}
