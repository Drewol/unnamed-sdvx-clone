#pragma once
#include <type_traits>
#include "Shared/Vector.hpp"
#include "Shared/Map.hpp"
#include "Shared/String.hpp"

/*
	Abstract binary stream base class
	a stream can either be operating as a reading stream or as a writing stream
	allows custom structures and String,Vector and Map types

	Example:
	{
		string a = "test";
		float b = 3.14f;
		int c = 20;
		Vector<string> d = {"a", "b", "c"};

		// Writes to a stream
		outStream << a;
		outStream << b;
		outStream << c;
		outStream << d;

		// Reads back the same data from a stream (note the << operator is still used for reading)
		inStream << a;
		inStream << b;
		inStream << c;
		inStream << d;
	}

*/
class BinaryStream
{
protected:
	BinaryStream(bool isReading = false)
	{
		m_isReading = isReading;
	}
public:
	// Reads or writes strings
	bool SerializeObject(String& obj);
	bool SerializeObject(WString& obj);

	// Vector/Map serialization helpers
	template<typename T>
	bool SerializeObject(Vector<T>& obj);
	template<typename K, typename V>
	bool SerializeObject(Map<K, V>& obj);

	// Writes or reads a pointer type by calling StaticSerialize on the type
	// prototype is:
	//	static bool StaticSerialize(BinaryStream& stream, T*& object);
	//
	// This function needs to assign the pointer if the stream is in reading mode
#define has_serialize_function_t (!std::is_trivial<T>::value && !std::is_void<decltype(T::StaticSerialize(*(BinaryStream*)0, *(T**)0))>::value)
	template <typename T>
	typename std::enable_if<has_serialize_function_t, bool>::type SerializeObject(T*& obj)
	{
		return T::StaticSerialize(*this, obj);
	}
	template <typename T>
	typename std::enable_if<has_serialize_function_t, bool>::type SerializeObject(T& obj)
	{
		T* tempObj = &obj;
		bool r = T::StaticSerialize(*this, tempObj);
		if (IsReading() && tempObj != &obj)
		{
			// If StaticSerialize allocated a new value, copy the data instead
			obj = *tempObj;
			delete tempObj;
		}
		return r;
	}

	// Reads or writes a struct or native type's data based on the stream's mode of operation
	template<typename T>
	typename std::enable_if<!std::is_pointer<T>::value && std::is_trivially_copyable<T>::value, bool>::type SerializeObject(T& obj)
	{
		return Serialize(&obj, sizeof(obj)) == sizeof(obj);
	}

	// Reads or writes data based on the stream's mode of operation
	virtual size_t Serialize(void* data, size_t len) = 0;
	// Seeks to a position in the stream
	virtual void Seek(size_t pos) = 0;
	// Seeks from the end, where 0 is the end of the stream
	virtual void SeekReverse(size_t pos)
	{
		Seek(GetSize() - pos);
		m_isOk = true;
	}
	// Seeks relative from current position
	virtual void Skip(size_t pos)
	{
		Seek(Tell() + pos);
		m_isOk = true;
	}
	// Tells the position of the stream
	virtual size_t Tell() const = 0;
	// Returns the current size of the stream
	//	either the max amount of readable data or the amount of currently written data
	virtual size_t GetSize() const = 0;

	// Stream operators
	// this template operator just routes everything to SerlializeObject
	template<typename T> BinaryStream& operator<<(T& obj)
	{
		m_isOk = SerializeObject(obj);
		return *this;
	}
	// Check if stream operator<< was successfull
	bool IsOk() const
	{
		return m_isOk;
	}

	bool IsReading() const 
	{
		return m_isReading;
	}
	bool IsWriting() const
	{
		return !m_isReading;
	}
protected:
	bool m_isReading;
	bool m_isOk = true;
};

template<typename T>
bool BinaryStream::SerializeObject(Vector<T>& obj)
{
	if(IsReading())
	{
		obj.clear();
		uint32 len;
		*this << len; 
		if (!m_isOk)
			return false;
		for(uint32 i = 0; i < len; i++)
		{
			T v;
			bool ok = SerializeObject(v);
			assert(ok);
			if (!ok)
				return false;
			obj.push_back(v);
		}
	}
	else
	{
		uint32 len = (uint32)obj.size();
		*this << len;
		if (!m_isOk)
			return false;
		for(uint32 i = 0; i < len; i++)
		{
			bool ok = SerializeObject(obj[i]);
			assert(ok);
			if (!ok)
				return false;
		}
	}
	return true;
}
template<typename K, typename V>
bool BinaryStream::SerializeObject(Map<K, V>& obj)
{
	if(IsReading())
	{
		obj.clear();
		uint32 len;
		*this << len;
		if (!m_isOk)
			return false;
		for(uint32 i = 0; i < len; i++)
		{
			K k;
			V v;
			bool ok = true;
			ok = ok && SerializeObject(k);
			ok = ok && SerializeObject(v);
			assert(ok);
			if (!ok)
				return false;
			obj.Add(k, v);
		}
	}
	else
	{
		uint32 len = (uint32)obj.size();
		*this << len;
		if (!m_isOk)
			return false;
		for(auto& p : obj)
		{
			bool ok = true;
			ok = ok && SerializeObject(const_cast<K&>(p.first));
			ok = ok && SerializeObject(p.second);
			assert(ok);
			if (!ok)
				return false;
		}
	}
	return true;
}
