#pragma once
#include <string>
#include <algorithm>
#include "Shared/Utility.hpp"
#include "Shared/Vector.hpp"

/*
	String class, extends std::string
*/
template<typename T>
class StringBase : public std::basic_string<T>
{
public:
	using std::basic_string<T>::basic_string;

	// These are for allowing function to be called on the base class when compiling on GCC
	using std::basic_string<T>::c_str;
	using std::basic_string<T>::substr;
	using std::basic_string<T>::begin;
	using std::basic_string<T>::end;
	using std::basic_string<T>::length;
	using std::basic_string<T>::back;
	using std::basic_string<T>::empty;
	using std::basic_string<T>::front;
	using std::basic_string<T>::find;
	using std::basic_string<T>::find_last_of;

	StringBase() = default;
	StringBase(const T* cs);
	StringBase(const std::basic_string<T>& ss);
	StringBase(const StringBase&) = default;
	StringBase& operator=(const StringBase&) = default;
	StringBase& operator=(const T* cs);
	StringBase& operator=(const std::basic_string<T>& ss);
	const T* operator*() const;
	void ToLower();
	void ToUpper();
	bool Split(const StringBase& delim, StringBase* l, StringBase* r) const;
	bool SplitLast(const StringBase& delim, StringBase* l, StringBase* r) const;
	Vector<StringBase> Explode(const StringBase& delim, bool keepEmpty = true) const;
	void TrimFront(T c);
	void TrimBack(T c);
	void Trim(T c = ' ');
	T* GetData();
	const T* GetData() const;
};

/* String class, extends std::string */
typedef StringBase<char> String;
typedef StringBase<wchar_t> WString;

namespace Utility
{
	template<typename I>
	I SprintfArgFilter(const I& in)
	{
		return in;
	}
	const char* SprintfArgFilter(const String& in);
	template<typename I>
	I WSprintfArgFilter(const I& in)
	{
		return in;
	}
	const wchar_t* WSprintfArgFilter(const WString& in);

	template<int N, typename... Args>
	void BufferSprintf(char (&buffer)[N], const char* fmt, Args... args)
	{
#ifdef _WIN32
		sprintf_s(buffer, N, fmt, SprintfArgFilter(args)...);
#else
		snprintf(buffer, N-1, fmt, SprintfArgFilter(args)...);
#endif
	}

	// Helper function that performs the c standard sprintf but returns a managed object instead
	// Max Output length = 8000
	template<typename... Args>
	String Sprintf(const char* fmt, Args... args)
	{
		static char buffer[8000];
		BufferSprintf(buffer, fmt, args...);

		return String(buffer);
	}

	// Helper function that performs the c standard sprintf but returns a managed object instead
	// Max Output length = 8000
	template<typename... Args>
	WString WSprintf(const wchar_t* fmt, Args... args)
	{
		static wchar_t buffer[8000];
#ifdef _WIN32
		swprintf(buffer, 8000-1, fmt, WSprintfArgFilter(args)...);
#else
		swprintf(buffer, 8000-1, fmt, WSprintfArgFilter(args)...);
#endif
		return WString(buffer);
	}

	// Unicode(wchar's on windows) to UTF8
	String ConvertToUTF8(const WString& unicodeString);
	// UTF8 to Unicode(wchar's on windows)
	WString ConvertToWString(const String& ansiString);
}

/* Template string function implementations */
template<typename T>
StringBase<T>::StringBase(const T* cs) : std::basic_string<T>(cs)
{

}
template<typename T>
StringBase<T>::StringBase(const std::basic_string<T>& ss)
{
	dynamic_cast<std::basic_string<T>&>(*this) = ss;
}
template<typename T>
StringBase<T>& StringBase<T>::operator=(const T* cs)
{
	dynamic_cast<std::basic_string<T>&>(*this) = cs;
	return *this;
}
template<typename T>
StringBase<T>& StringBase<T>::operator=(const std::basic_string<T>& ss)
{
	dynamic_cast<std::basic_string<T>&>(*this) = ss;
	return *this;
}
template<typename T>
const T* StringBase<T>::operator*() const
{
	return c_str();
}
template<typename T>
inline void StringBase<T>::ToLower()
{
	std::transform(begin(), end(), begin(), ::tolower);
}
template<typename T>
inline void StringBase<T>::ToUpper()
{
	std::transform(begin(), end(), begin(), ::toupper);
}
template<typename T>
bool StringBase<T>::Split(const StringBase& delim, StringBase* l, StringBase* r) const
{
	size_t f = find(delim);
	if(f == (size_t)-1)
		return false;
	StringBase selfCopy = *this;
	if(r)
	{
		*r = selfCopy.substr(f + delim.length());
	}
	if(l)
	{
		*l = selfCopy.substr(0, f);
	}

	return true;
}
template<typename T>
bool StringBase<T>::SplitLast(const StringBase& delim, StringBase* l, StringBase* r) const
{
	size_t f = find_last_of(delim);
	if(f == -1)
		return false;
	if(l)
	{
		*l = substr(0, f);
	}
	if(r)
	{
		*r = substr(f + delim.length());
	}

	return true;
}
template<typename T>
Vector<StringBase<T>> StringBase<T>::Explode(const StringBase& delim, bool keepEmpty /*=true*/) const
{
	String a, b;
	Vector<StringBase> res;
	if(!Split(delim, &a, &b))
	{
		res.Add(*this);
		return res;
	}

	do
	{
		if(keepEmpty || !a.empty())
			res.Add(a);

	} while(b.Split(delim, &a, &b));

	if (keepEmpty || !b.empty())
		res.Add(b);

	return res;
}
template<typename T>
void StringBase<T>::TrimFront(T c)
{
	while(length() > 0)
	{
		if(front() != c)
			break;
		this->erase(begin());
	}
}
template<typename T>
void StringBase<T>::TrimBack(T c)
{
	while(length() > 0)
	{
		if(back() != c)
			break;
		this->erase(--end());
	}
}
template<typename T>
void StringBase<T>::Trim(T c)
{
	TrimFront(c);
	TrimBack(c);
}
template<typename T>
T* StringBase<T>::GetData()
{
	if(empty())
		return nullptr;
	return &front();
}
template<typename T>
const T* StringBase<T>::GetData() const
{
	if(empty())
		return nullptr;
	return &front();
}
