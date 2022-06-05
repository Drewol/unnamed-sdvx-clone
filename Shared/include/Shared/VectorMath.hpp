#pragma once
#include "Shared/Math.hpp"
#include <type_traits>

namespace VectorMath
{
	template<typename T, size_t num>
	class VectorBase {};

	/* Vector 4 */
	template<typename T>
	class VectorBase<T, 4>
	{
	public:
		T x, y, z, w;
		VectorBase() : x(0), y(0), z(0), w(0) {};
		explicit VectorBase(T c) : x(c), y(c), z(c), w(c) {};
		VectorBase(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {};
		template<typename T1>
		VectorBase(const VectorBase<T1, 4>& other)
		{
			x = (T)other.x;
			y = (T)other.y;
			z = (T)other.z;
			w = (T)other.w;
		}
		template<typename T1>
		VectorBase(const VectorBase<T1, 3>& other, T w = 0) : w(w)
		{
			x = (T)other.x;
			y = (T)other.y;
			z = (T)other.z;
		}
		template<typename T1>
		VectorBase(const VectorBase<T1, 2>& other, T z = 0, T w = 0) : z(z), w(w)
		{
			x = (T)other.x;
			y = (T)other.y;
		}
		template<typename T1 = T>
		VectorBase<T1, 3> xyz() const
		{
			return VectorBase<T1, 3>((T1)x, (T1)y, (T1)z);
		}
		template<typename T1 = T>
		VectorBase<T1, 2> xy() const
		{
			return VectorBase<T1, 2>((T1)x, (T1)y);
		}
		VectorBase operator+(const VectorBase& r) const
		{
			return VectorBase(x + r.x, y + r.y, z + r.z, w + r.w);
		}
		VectorBase operator-(const VectorBase& r) const
		{
			return VectorBase(x - r.x, y - r.y, z - r.z, w - r.w);
		}
		VectorBase operator*(const VectorBase& r) const
		{
			return VectorBase(x * r.x, y * r.y, z * r.z, w * r.w);
		}
		VectorBase operator/(const VectorBase& r) const
		{
			return VectorBase(x / r.x, y / r.y, z / r.z, w / r.w);
		}
		VectorBase operator*(T r) const
		{
			return VectorBase(x * r, y * r, z * r, w * r);
		}
		VectorBase operator/(T r) const
		{
			return VectorBase(x / r, y / r, z / r, w / r);
		}

		VectorBase& operator+=(const VectorBase& r)
		{
			x += r.x, y += r.y, z += r.z, w += r.w;
			return *this;
		}
		VectorBase& operator-=(const VectorBase& r)
		{
			x -= r.x, y -= r.y, z -= r.z, w -= r.w;
			return *this;
		}
		VectorBase& operator*=(const VectorBase& r)
		{
			x *= r.x, y *= r.y, z *= r.z, w *= r.w;
			return *this;
		}
		VectorBase& operator/=(const VectorBase& r)
		{
			x /= r.x, y /= r.y, z /= r.z, w /= r.w;
			return *this;
		}
		VectorBase& operator*=(T r)
		{
			x *= r, y *= r, z *= r, w *= r;
			return *this;
		}
		VectorBase& operator/=(T r)
		{
			x /= r, y /= r, z /= r, w /= r;
			return *this;
		}

		VectorBase operator-() const
		{
			return VectorBase(-x, -y, -z, -w);
		}

		T Length() const;
		T LengthSquared() const;
		VectorBase Normalized() const;
	};
	/* Vector 3 */
	template<typename T>
	class VectorBase<T, 3>
	{
	public:
		T x, y, z;
		VectorBase() : x(0), y(0), z(0) {};
		explicit VectorBase(float c) : x(c), y(c), z(c) {};
		VectorBase(T x, T y, T z) : x(x), y(y), z(z) {};
		template<typename T1>
		VectorBase(const VectorBase<T1, 3>& other)
		{
			x = (T)other.x;
			y = (T)other.y;
			z = (T)other.z;
		}
		template<typename T1>
		VectorBase(const VectorBase<T1, 2>& other, T z = 0) : z(z)
		{
			x = (T)other.x;
			y = (T)other.y;
		}
		template<typename T1 = T>
		VectorBase<T1, 2> xy() const
		{
			return VectorBase<T1, 2>((T1)x, (T1)y);
		}
		VectorBase operator+(const VectorBase& r) const
		{
			return VectorBase(x + r.x, y + r.y, z + r.z);
		}
		VectorBase operator-(const VectorBase& r) const
		{
			return VectorBase(x - r.x, y - r.y, z - r.z);
		}
		VectorBase operator*(const VectorBase& r) const
		{
			return VectorBase(x * r.x, y * r.y, z * r.z);
		}
		VectorBase operator/(const VectorBase& r) const
		{
			return VectorBase(x / r.x, y / r.y, z / r.z);
		}
		VectorBase operator*(T r) const
		{
			return VectorBase(x * r, y * r, z * r);
		}
		VectorBase operator/(T r) const
		{
			return VectorBase(x / r, y / r, z / r);
		}

		VectorBase& operator+=(const VectorBase& r)
		{
			x += r.x, y += r.y, z += r.z;
			return *this;
		}
		VectorBase& operator-=(const VectorBase& r)
		{
			x -= r.x, y -= r.y, z -= r.z;
			return *this;
		}
		VectorBase& operator*=(const VectorBase& r)
		{
			x *= r.x, y *= r.y, z *= r.z;
			return *this;
		}
		VectorBase& operator/=(const VectorBase& r)
		{
			x /= r.x, y /= r.y, z /= r.z;
			return *this;
		}
		VectorBase& operator*=(T r)
		{
			x *= r, y *= r, z *= r;
			return *this;
		}
		VectorBase& operator/=(T r)
		{
			x /= r, y /= r, z /= r;
			return *this;
		}

		VectorBase operator-() const
		{
			return VectorBase(-x, -y, -z);
		}

		T Length() const;
		T LengthSquared() const;
		VectorBase Normalized() const;
	};
	/* Vector 2 */
	template<typename T>
	class VectorBase<T, 2>
	{
	public:
		T x, y;
		VectorBase() : x(0), y(0) {};
		explicit VectorBase(T c) : x(c), y(c) {};
		VectorBase(T x, T y) : x(x), y(y) {};
		template<typename T1>
		VectorBase(const VectorBase<T1, 2>& other)
		{
			x = (T)other.x;
			y = (T)other.y;
		}
		template<typename T1>
		VectorBase operator+(const VectorBase<T1, 2>& r) const
		{
			return VectorBase(x + r.x, y + r.y);
		}
		VectorBase operator-(const VectorBase& r) const
		{
			return VectorBase(x - r.x, y - r.y);
		}
		VectorBase operator*(const VectorBase& r) const
		{
			return VectorBase(x * r.x, y * r.y);
		}
		VectorBase operator/(const VectorBase& r) const
		{
			return VectorBase(x / r.x, y / r.y);
		}
		VectorBase operator*(T r) const
		{
			return VectorBase(x * r, y * r);
		}
		VectorBase operator/(T r) const
		{
			return VectorBase(x / r, y / r);
		}

		VectorBase& operator+=(const VectorBase& r)
		{
			x += r.x, y += r.y;
			return *this;
		}
		VectorBase& operator-=(const VectorBase& r)
		{
			x -= r.x, y -= r.y;
			return *this;
		}
		VectorBase& operator*=(const VectorBase& r)
		{
			x *= r.x, y *= r.y;
			return *this;
		}
		VectorBase& operator/=(const VectorBase& r)
		{
			x /= r.x, y /= r.y;
			return *this;
		}
		VectorBase& operator*=(T r)
		{
			x *= r, y *= r;
			return *this;
		}
		VectorBase& operator/=(T r)
		{
			x /= r, y /= r;
			return *this;
		}

		VectorBase operator-() const
		{
			return VectorBase(-x, -y);
		}

		T Length() const;
		T LengthSquared() const;
		VectorBase Normalized() const;
	};

	// Dot product implementations
	template<typename T, size_t Num>
	static T Dot(const VectorBase<T, Num>& lhs, const VectorBase<T, Num>& rhs)
	{
		static_assert(sizeof(T) == 0, "Invalid vector types for dot product");
	}
	template<typename T>
	static T Dot(const VectorBase<T, 2>& lhs, const VectorBase<T, 2>& rhs)
	{
		return lhs.x * rhs.x + lhs.y * rhs.y;
	}
	template<typename T>
	static T Dot(const VectorBase<T, 3>& lhs, const VectorBase<T, 3>& rhs)
	{
		return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
	}
	template<typename T>
	static T Dot(const VectorBase<T, 4>& lhs, const VectorBase<T, 4>& rhs)
	{
		return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
	}

	// Cross product implementation
	template<typename T>
	static VectorBase<T, 3> Cross(const VectorBase<T, 3>& lhs, const VectorBase<T, 3>& rhs)
	{
		return VectorBase<T, 3>(lhs.y * rhs.z - lhs.z * rhs.y,
			lhs.z * rhs.x - lhs.x * rhs.z,
			lhs.x * rhs.y - lhs.y * rhs.x);
	}
	template<typename T>
	static float Cross2D(const VectorBase<T, 2>& lhs, const VectorBase<T, 2>& rhs)
	{
		return lhs.x * rhs.y - lhs.y * rhs.x;
	}

	// Vector length and squared length
	template<typename T>
	T VectorMath::VectorBase<T, 4>::Length() const
	{
		static_assert(std::is_floating_point<T>::value == true, "Length can only be called on floating point vectors");
		return (T)std::sqrt(LengthSquared());
	}
	template<typename T>
	T VectorMath::VectorBase<T, 4>::LengthSquared() const
	{
		return Dot(*this, *this);
	}
	template<typename T>
	T VectorMath::VectorBase<T, 3>::Length() const
	{
		static_assert(std::is_floating_point<T>::value == true, "Length can only be called on floating point vectors");
		return (T)std::sqrt(LengthSquared());
	}
	template<typename T>
	T VectorMath::VectorBase<T, 3>::LengthSquared() const
	{
		return Dot(*this, *this);
	}
	template<typename T>
	T VectorMath::VectorBase<T, 2>::Length() const
	{
		static_assert(std::is_floating_point<T>::value == true, "Length can only be called on floating point vectors");
		return (T)std::sqrt(LengthSquared());
	}
	template<typename T>
	T VectorMath::VectorBase<T, 2>::LengthSquared() const
	{
		return Dot(*this, *this);
	}

	// Vector normalization
	template<typename T, size_t Num>
	static VectorBase<T, Num> Normalize(const VectorBase<T, Num>& lhs)
	{
		static_assert(sizeof(T) == 0, "Invalid vector type for normalize");
	}
	template<typename T>
	static VectorBase<T, 4> Normalize(const VectorBase<T, 4>& lhs)
	{
		return lhs / lhs.Length();
	}
	template<typename T>
	static VectorBase<T, 3> Normalize(const VectorBase<T, 3>& lhs)
	{
		return lhs / lhs.Length();
	}
	template<typename T>
	static VectorBase<T, 2> Normalize(const VectorBase<T, 2>& lhs)
	{
		return lhs / lhs.Length();
	}
	// Member normalized function
	template<typename T>
	VectorBase<T, 4> VectorBase<T, 4>::Normalized() const
	{
		return Normalize(*this);
	}
	template<typename T>
	VectorBase<T, 3> VectorBase<T, 3>::Normalized() const
	{
		return Normalize(*this);
	}
	template<typename T>
	VectorBase<T, 2> VectorBase<T, 2>::Normalized() const
	{
		return Normalize(*this);
	}

	// Vector lerp
	template<typename T, size_t Num>
	VectorBase<T, Num> Lerp(const VectorBase<T, Num>& a, const VectorBase<T, Num>& b, float pos)
	{
		VectorBase<T, Num> d = b - a;
		return a + d * pos;
	}
}

typedef VectorMath::VectorBase<float, 2> Vector2;
typedef VectorMath::VectorBase<int32, 2> Vector2i;
typedef VectorMath::VectorBase<float, 3> Vector3;
typedef VectorMath::VectorBase<int32, 3> Vector3i;
typedef VectorMath::VectorBase<float, 4> Vector4;
typedef VectorMath::VectorBase<int32, 4> Vector4i;
