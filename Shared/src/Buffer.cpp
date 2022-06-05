#include "stdafx.h"
#include "Buffer.hpp"

Buffer::Buffer(const char* string)
{
	uint32 l = (uint32)strlen(string);
	for (uint32 i = 0; i < l; i++)
	{
		push_back((uint8)string[i]);
	}
}
Buffer::Buffer(Buffer&& rhs)
{
	((vector<uint8>*)this)->operator=((Buffer&&)rhs);
}
Buffer Buffer::Copy() const
{
	Buffer newBuffer;
	if (!empty())
	{
		newBuffer.resize(size());
		memcpy(newBuffer.data(), data(), size());
	}
	return (Buffer&&)newBuffer;
}
Buffer& Buffer::operator=(Buffer&& rhs)
{
	Vector<uint8>::operator=(rhs);
	return *this;
}

CopyableBuffer& CopyableBuffer::operator=(const CopyableBuffer& rhs)
{
	(Vector<uint8>&)* this = rhs;
	return *this;
}
CopyableBuffer::CopyableBuffer(const CopyableBuffer& rhs)
{
	(Vector<uint8>&)* this = rhs;
}
