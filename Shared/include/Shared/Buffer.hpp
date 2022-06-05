#pragma once
#include "Shared/Vector.hpp"
#include "Shared/Unique.hpp"
#include <initializer_list>

/*
	Simple buffer class, extends a byte vector (uint8)
*/
class Buffer : public Vector<uint8>, Unique
{
public:
	using Vector<uint8>::Vector;
	Buffer() = default;
	Buffer(Buffer&& rhs);
	Buffer& operator=(Buffer&& rhs);
	// Creates a new buffer from a string, without the terminating null character
	explicit Buffer(const char* string);
	// Creates a copy of this buffer
	Buffer Copy() const;
};
class CopyableBuffer : public Buffer
{
public:
	using Buffer::Buffer;
	CopyableBuffer() = default;
	CopyableBuffer(const CopyableBuffer& rhs);
	CopyableBuffer& operator=(const CopyableBuffer& rhs);
};
