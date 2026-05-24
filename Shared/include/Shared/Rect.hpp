#pragma once
#include "VectorMath.hpp"

/*
	GUI space rectangle with bottom as y+height
*/

template<typename T>
class Vector;

namespace Shared
{
	template<typename T>
	class RectangleBase
	{
	public:
		typedef VectorMath::VectorBase<T, 2> VectorType;
		VectorType pos;
		VectorType size;
		// Conversion from other types
		template<typename Q>
		RectangleBase(const RectangleBase<Q>& other)
		{
			pos = (VectorType)other.pos;
			size = (VectorType)other.size;
		}
		// Give all 4 sides of the rectangle
		RectangleBase(T all = 0) : RectangleBase(all, all, all, all) {}
		RectangleBase(T left, T top, T right, T bottom)
		{
			pos = VectorType(left, top);
			size = VectorType(right - left, bottom - top);
		}
		RectangleBase(const VectorType& pos, const VectorType& size)
			: pos(pos), size(size)
		{
		}

		[[nodiscard]]
		T Left() const noexcept
		{
			return pos.x;
		}
		[[nodiscard]]
		T Top() const noexcept
		{
			return pos.y;
		}
		[[nodiscard]]
		T Bottom() const noexcept
		{
			return pos.y + size.y;
		}
		[[nodiscard]]
		T Right() const noexcept
		{
			return pos.x + size.x;
		}
		// Moves the edges of this rectangle inward or outward
		[[nodiscard]]
		RectangleBase Offset(float amount) const noexcept
		{
			RectangleBase newRect = *this;
			VectorType newSize = newRect.size + VectorType(amount);
			if (newSize.x < 0)
				newSize.x = 0;
			if (newSize.y < 0)
				newSize.y = 0;

			VectorType halfDelta = (newSize - newRect.size);
			newRect.pos -= halfDelta;
			newRect.size += halfDelta * 2;
			return newRect;
		}
		// Moves the upper and lower edges of this rectangle inward or outward
		[[nodiscard]]
		RectangleBase OffsetY(float amount) const noexcept
		{
			RectangleBase newRect = *this;
			VectorType newSize = newRect.size + VectorType(0, amount);
			if (newSize.y < 0)
				newSize.y = 0;

			VectorType halfDelta = (newSize - newRect.size);
			newRect.pos -= halfDelta;
			newRect.size += halfDelta * 2;
			return newRect;
		}
		// Moves the upper and lower edges of this rectangle inward or outward
		[[nodiscard]]
		RectangleBase OffsetX(float amount) const noexcept
		{
			RectangleBase newRect = *this;
			VectorType newSize = newRect.size + VectorType(amount, 0);
			if (newSize.x < 0)
				newSize.x = 0;

			VectorType halfDelta = (newSize - newRect.size);
			newRect.pos -= halfDelta;
			newRect.size += halfDelta * 2;
			return newRect;
		}

		[[nodiscard]]
		inline bool Contains(const RectangleBase& other) const noexcept
		{
			return Left() <= other.Left() && other.Right() <= Right() && Top() <= other.Top() && other.Bottom() <= Bottom();
		}

		[[nodiscard]]
		inline bool NotSmallerThan(const VectorType& other_size) const noexcept
		{
			return size.x >= other_size.x && size.y >= other_size.y;
		}

		// Clamp the parameter to this rectangle
		[[nodiscard]]
		RectangleBase Clamp(const RectangleBase& other) const noexcept
		{
			T top = Math::Max(other.Top(), Top());
			T bottom = Math::Min(other.Bottom(), Bottom());
			T left = Math::Max(other.Left(), Left());
			T right = Math::Min(other.Right(), Right());
			if (right < left)
				right = left;
			if (bottom < top)
				bottom = top;
			return RectangleBase(left, top, right, bottom);
		}
	};

	/*
		Same as a normal rectangle but this one has the top as y+height aka world space
	*/
	template<typename T>
	class RectangleBase3D : public RectangleBase<T>
	{
	public:
		typedef VectorMath::VectorBase<T, 2> VectorType;
		using RectangleBase<T>::RectangleBase;
		using RectangleBase<T>::RectangleBase::pos;
		using RectangleBase<T>::RectangleBase::size;

		RectangleBase3D() = default;
		// Conversion from other types
		template<typename Q>
		RectangleBase3D(const RectangleBase<Q>& other)
		{
			pos = (VectorType)other.pos;
			size = (VectorType)other.size;
		}
		RectangleBase3D(const RectangleBase<T>& other)
		{
			pos = other.pos;
			size = other.size;
		};
		RectangleBase3D(T left, T top, T right, T bottom)
		{
			pos = VectorType(left, bottom);
			size = VectorType(right - left, top - bottom);
		}
		[[nodiscard]]
		T Top() const noexcept
		{
			return pos.y + size.y;
		}
		[[nodiscard]]
		T Bottom() const noexcept
		{
			return pos.y;
		}
	};

	typedef RectangleBase<float> Rect;
	typedef RectangleBase<int32> Recti;
	typedef RectangleBase3D<float> Rect3D;
	typedef RectangleBase3D<int32> Recti3D;
}