#pragma once

namespace Utility
{
	// Utility function that clears out an object passed to it by using memset on the object's size
	template<typename T>
	void MemsetZero(T& obj)
	{
		memset(&obj, 0, sizeof(T));
	}

	// Dynamic cast function
	template<typename T, typename S>
	T* Cast(S* src)
	{
		return dynamic_cast<T*>(src);
	}

	// Dynamic cast function (using reference
	template<typename T, typename S>
	T& Cast(S& src)
	{
		return dynamic_cast<T&>(src);
	}

	// Reinterpret cast function
	template<typename T, typename S>
	T& Reinterpret(S& src)
	{
		return reinterpret_cast<T&>(src);
	}

	// Reinterpret input using an union
	template<typename T, typename S>
	T UnionCast(S src)
	{
		union
		{
			S a;
			T b;
		};
		a = src;
		return b;
	}

	// Swaps two references
	template<typename T>
	void Swap(T& a, T& b)
	{
		T tmp = a;
		a = b;
		b = tmp;
	}
}
