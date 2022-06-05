#include "stdafx.h"
#include "Interpolation.hpp"
#include "VectorMath.hpp"

namespace Interpolation
{

	CubicBezier::CubicBezier(Predefined predefined)
	{
		switch (predefined)
		{
		case Linear:
			m_Set(0.0, 0.0, 1.0, 1.0);
			break;
		case EaseInQuad:
			m_Set(0.55, 0.085, 0.68, 0.53);
			break;
		case EaseOutQuad:
			m_Set(0.25, 0.46, 0.45, 0.94);
			break;
		case EaseInOutQuad:
			m_Set(0.455, 0.03, 0.515, 0.955);
			break;
		case EaseInCubic:
			m_Set(0.55, 0.055, 0.675, 0.19);
			break;
		case EaseOutCubic:
			m_Set(0.215, 0.61, 0.355, 1);
			break;
		case EaseInOutCubic:
			m_Set(0.645, 0.045, 0.355, 1);
			break;
		case EaseInExpo:
			m_Set(0.95, 0.05, 0.795, 0.035);
			break;
		case EaseOutExpo:
			m_Set(0.19, 1, 0.22, 1);
			break;
		case EaseInOutExpo:
			m_Set(1.0, 0.0, 0.0, 1.0);
			break;
		}
	}
	CubicBezier::CubicBezier(float a, float b, float c, float d)
	{
		m_Set(a, b, c, d);
	}
	CubicBezier::CubicBezier(double a, double b, double c, double d)
	{
		m_Set(a, b, c, d);
	}
	float CubicBezier::Sample(float in) const
	{
		assert(in >= 0);
		assert(in <= 1.0f);
		float inv = 1.0f - in;
		float inv2 = inv * inv;
		float inv3 = inv2 * inv;
		float in2 = in * in;
		float in3 = in2 * in;
		Vector2 r = Vector2(0, 0) * inv3 +
			Vector2(a, b) * 3 * inv2 * in +
			Vector2(c, d) * 3 * inv * in2 +
			Vector2(1, 1) * in3;
		return r.y;
	}
	float CubicBezier::operator()(float in) const
	{
		return Sample(in);
	}
	void CubicBezier::m_Set(float a, float b, float c, float d)
	{
		this->a = a;
		this->b = b;
		this->c = c;
		this->d = d;
	}
	void CubicBezier::m_Set(double a, double b, double c, double d)
	{
		this->a = (float)a;
		this->b = (float)b;
		this->c = (float)c;
		this->d = (float)d;
	}

	int32 Lerp(int32 a, int32 b, float f, TimeFunction timeFunction /*= TimeFunction::Linear*/)
	{
		return a + (int32)((float)(b - a) * timeFunction(f));
	}

}
