#pragma once
#include "Shared/BinaryStream.hpp"
#include "Shared/NonCopyable.hpp"
#include "Shared/Buffer.hpp"

class MemoryStreamBase : public BinaryStream
{
protected:
	Buffer* m_buffer = nullptr;
	size_t m_cursor = 0; // Write position
public:
	MemoryStreamBase() = default;
	MemoryStreamBase(Buffer& buffer, bool isReading);
	virtual void Seek(size_t pos);
	virtual size_t Tell() const;
	virtual size_t GetSize() const;
};

/* Stream that reads from a buffer */
class MemoryReader : public MemoryStreamBase
{
public:
	MemoryReader() = default;
	MemoryReader(Buffer& buffer);
	virtual size_t Serialize(void* data, size_t len);
};

/* Stream that writes to a buffer */
class MemoryWriter : public MemoryStreamBase
{
public:
	MemoryWriter() = default;
	MemoryWriter(Buffer& buffer);
	virtual size_t Serialize(void* data, size_t len);
};
