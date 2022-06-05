#pragma once
#include "Rect.hpp"
#include "Math.hpp"
#include "VectorMath.hpp"
#include <limits>

namespace Shared
{
	/*
		Margin template class that is similar to the Rectangle class.
		Defines offsets for left,right,top,bottom
	*/
	template<typename T>
	class MarginBase
	{
	public:
		MarginBase(T all = 0)
			: left(all), right(all), top(all), bottom(all) {};
		MarginBase(T x, T y)
			: left(x), right(x), top(y), bottom(y) {};
		MarginBase(T left, T top, T right, T bottom)
			: left(left), top(top), right(right), bottom(bottom) {};

		// Apply this margin to a rectable
		RectangleBase<T> Apply(const RectangleBase<T>& in) const
		{
			RectangleBase<T> ret = in;
			ret.pos += VectorMath::VectorBase<T, 2>(left, top);
			ret.size -= VectorMath::VectorBase<T, 2>(left + right, top + bottom);
			return ret;
		}

		// Adds all individual margins together
		MarginBase operator+(const MarginBase& other) const
		{
			return MarginBase(
				left + other.left,
				top + other.top,
				right + other.right,
				bottom + other.bottom);
		}
		MarginBase& operator+=(const MarginBase& other)
		{
			return *this = (*this) + other;
		}
		MarginBase operator*(const T scale) const
		{
			return MarginBase(left * scale, top * scale, right * scale, bottom * scale);
		}

		// Invert the margin
		MarginBase operator-() const
		{
			return MarginBase(
				-left,
				-top,
				-right,
				-bottom);
		}

		// Total width of the margin (left + right)
		T Width() const
		{
			return left + right;
		}
		// Total height of the margin (top + bottom)
		T Height() const
		{
			return top + bottom;
		}

		// Size (Width, Height)
		VectorMath::VectorBase<T, 2> GetSize() const
		{
			return VectorMath::VectorBase<T, 2>(Width(), Height());
		}

		void ClampMin(T min = 0)
		{
			left = Math::Max(left, min);
			top = Math::Max(top, min);
			right = Math::Max(right, min);
			bottom = Math::Max(bottom, min);
		}
		void Clamp(T min, T max)
		{
			left = Math::Clamp(left, min, max);
			top = Math::Clamp(top, min, max);
			right = Math::Clamp(right, min, max);
			bottom = Math::Clamp(bottom, min, max);
		}

		T left, top, right, bottom;
	};

	typedef MarginBase<float> Margin;
	typedef MarginBase<int32> Margini;
}
