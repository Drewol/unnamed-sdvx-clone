#pragma once
#include <type_traits>
#include <typeinfo>

namespace Utility
{
	/*
		Template struct that contains type information such as name/type hash/etc.
		Information is aquired using RTTI
	*/
	template<typename T>
	struct TypeInfo
	{
		static const std::type_info& ti;
		static const String name;
		static const size_t hashCode;
	};

	template<typename T>
	const std::type_info& TypeInfo<T>::ti = typeid(T);
	template<typename T>
	const size_t TypeInfo<T>::hashCode = ti.hash_code();
	template<typename T>
	const String TypeInfo<T>::name = ti.name();
}
