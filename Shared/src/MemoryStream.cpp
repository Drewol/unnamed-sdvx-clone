#include "stdafx.h"
#include "MemoryStream.hpp"
#include <algorithm>

MemoryStreamBase::MemoryStreamBase(Buffer& buffer, bool isReading) : BinaryStream(isReading), m_buffer(&buffer)
{
}
void MemoryStreamBase::Seek(size_t pos)
{
	assert(m_buffer);
	assert(pos <= m_buffer->size());
	m_cursor = pos;
}
size_t MemoryStreamBase::Tell() const
{
	assert(m_buffer);
	return m_cursor;
}
size_t MemoryStreamBase::GetSize() const
{
	assert(m_buffer);
	return m_buffer->size();
}

MemoryReader::MemoryReader(Buffer& buffer) : MemoryStreamBase(buffer, true)
{
}
size_t MemoryReader::Serialize(void* data, size_t len)
{
	assert(m_buffer);
	size_t endPos = m_cursor + len;
	if (endPos > m_buffer->size())
	{
		if (m_cursor >= m_buffer->size())
			return 0;
		len = m_buffer->size() - m_cursor;
	}
	if (len > 0)
	{
		memcpy(data, &(*m_buffer)[m_cursor], len);
		m_cursor += len;
	}
	return len;
}

MemoryWriter::MemoryWriter(Buffer& buffer) : MemoryStreamBase(buffer, false)
{
}
size_t MemoryWriter::Serialize(void* data, size_t len)
{
	if (len == 0)
		return 0;
	assert(m_buffer);
	size_t newSize = std::max(m_buffer->size(), m_cursor + len);
	// Reserve space for new data
	if (newSize > m_buffer->size())
	{
		m_buffer->resize(newSize);
	}
	memcpy(&(*m_buffer)[m_cursor], data, len);
	m_cursor += len;
	return len;
}
