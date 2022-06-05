#include "stdafx.h"
#include "BinaryStream.hpp"

//#define IMPLEMENT_STREAMABLE_TYPE(_type)\
//	BinaryStream& BinaryStream::operator<<(_type& obj) { SerializeObject<_type>(obj); return *this; }
//
//IMPLEMENT_STREAMABLE_TYPE(char);
//IMPLEMENT_STREAMABLE_TYPE(float);
//IMPLEMENT_STREAMABLE_TYPE(double);
//IMPLEMENT_STREAMABLE_TYPE(uint8);
//IMPLEMENT_STREAMABLE_TYPE(uint16);
//IMPLEMENT_STREAMABLE_TYPE(uint32);
//IMPLEMENT_STREAMABLE_TYPE(uint64);
//IMPLEMENT_STREAMABLE_TYPE(int8);
//IMPLEMENT_STREAMABLE_TYPE(int16);
//IMPLEMENT_STREAMABLE_TYPE(int32);
//IMPLEMENT_STREAMABLE_TYPE(int64);
//

bool BinaryStream::SerializeObject(String& obj)
{
	uint32 len;
	if (IsReading())
	{
		*this << len;
		obj.resize(len);
	}
	else
	{
		len = (uint32)obj.size();
		*this << len;
	}
	Serialize(obj.GetData(), len);
	return true;
}
bool BinaryStream::SerializeObject(WString& obj)
{
	uint32 len;
	if (IsReading())
	{
		*this << len;
		obj.resize(len);
	}
	else
	{
		len = (uint32)obj.size();
		*this << len;
	}
	Serialize(obj.GetData(), len * 2);
	return true;
}
