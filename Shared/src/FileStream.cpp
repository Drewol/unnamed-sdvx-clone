#include "stdafx.h"
#include "FileStream.hpp"
#include "CompressedFileStream.hpp"

FileStreamBase::FileStreamBase(File& file, bool isReading) : m_file(&file), BinaryStream(isReading)
{
}
void FileStreamBase::Seek(size_t pos)
{
	assert(m_file);
	m_file->Seek(pos);
}
size_t FileStreamBase::Tell() const
{
	assert(m_file);
	return m_file->Tell();
}
size_t FileStreamBase::GetSize() const
{
	assert(m_file);
	return m_file->GetSize();
}
File& FileStreamBase::GetFile()
{
	assert(m_file);
	return *m_file;
}

size_t FileReader::Serialize(void* data, size_t len)
{
	assert(m_file);
	return m_file->Read(data, len);
}

size_t FileWriter::Serialize(void* data, size_t len)
{
	assert(m_file);
	return m_file->Write(data, len);
}

CompressedFileStreamBase::CompressedFileStreamBase(File& file, bool isReading, int level)
	: FileStreamBase(file, isReading), m_level(level)
{
#ifdef ZLIB_FOUND
	m_z = nullptr;
	m_buffer = nullptr;
#endif
}

bool CompressedFileStreamBase::InitCompression()
{
#ifdef ZLIB_FOUND
	if (m_z == nullptr)
	{
		m_z = new z_stream();
		*m_z = { 0 };

		int ret;
		if (m_isReading)
			ret = inflateInit(m_z);
		else
			ret = deflateInit(m_z, m_level);

		if (ret != Z_OK)
		{
			delete m_z;
			m_z = nullptr;
			return false;
		}
		if (m_buffer == nullptr)
			m_buffer = new uint8[m_bufferLen];
	}
	return true;
#else
	return false;
#endif

}

bool CompressedFileStreamBase::StartCompression()
{
	m_useCompression = InitCompression();
	return m_useCompression;
}
CompressedFileStreamBase::~CompressedFileStreamBase()
{
#ifdef ZLIB_FOUND
	if (m_z)
	{
		if (m_isReading)
			inflateEnd(m_z);
		else
			deflateEnd(m_z);
		delete m_z;
		m_z = nullptr;
	}
	if (m_buffer)
	{
		delete[] m_buffer;
		m_buffer = nullptr;
	}
#endif
}

// Since we only support going from uncompressed -> compressed we don't
// have to worry about keeping track of how much data we actually used
// to decompress our output so far

size_t CompressedFileReader::Serialize(void* data, size_t len)
{
	if (len == 0)
		return 0;

	assert(m_file);
	if (!m_useCompression)
		return m_file->Read(data, len);

#ifdef ZLIB_FOUND
	assert(m_z);
	m_z->next_out = (uint8*)data;
	m_z->avail_out = len;
	do
	{
		// Move leftover input to the start of the buffer
		size_t extra = m_z->avail_in;
		assert(m_z->next_in + extra <= m_buffer + m_bufferLen);
		if (extra != 0)
			memmove(m_buffer, m_z->next_in, extra);
		m_z->next_in = m_buffer;

		// Read more data to decompress
		m_z->avail_in = extra + m_file->Read(m_buffer + extra, m_bufferLen - extra);

		int ret = inflate(m_z, Z_NO_FLUSH);
		assert(ret != Z_STREAM_ERROR);
		if (ret == Z_STREAM_END || ret == Z_BUF_ERROR)
			break;

		if (ret != Z_OK)
		{
			break;
		}

	} while (m_z->avail_out != 0);

	return len - m_z->avail_out;
#else
	return 0;
#endif
}

size_t CompressedFileWriter::Serialize(void* data, size_t len)
{
	if (len == 0)
		return 0;

	assert(m_file);
	if (!m_useCompression)
		return m_file->Write(data, len);

#ifdef ZLIB_FOUND
	bool ok = true;

	assert(m_z);
	m_z->next_in = (uint8*)data;
	m_z->avail_in = len;

	while(ok)
	{
		// Write to start of buffer every time
		m_z->next_out = m_buffer;
		m_z->avail_out = m_bufferLen;

		int ret = deflate(m_z, Z_NO_FLUSH);

		size_t clen = m_bufferLen- m_z->avail_out;
		for (size_t amt, pos = 0; ok && pos < clen; pos += amt)
		{
			amt = m_file->Write(m_buffer + pos, clen - pos);
			if (amt == 0)
				ok = false;
		}

		if (ret == Z_STREAM_END || ret == Z_BUF_ERROR)
			break;

		if (ret != Z_OK)
		{
			ok = false;
		}
	}

	if (!ok)
		return 0;

	return len - m_z->avail_in;
#else
	return 0;
#endif
}

bool CompressedFileWriter::FinishCompression()
{
	if (!m_useCompression)
		return true;

	bool ok = true;

#ifdef ZLIB_FOUND
	assert(m_z);
	assert(m_file);
	m_z->next_in = nullptr;
	m_z->avail_in = 0;
	while (ok)
	{
		m_z->next_out = m_buffer;
		m_z->avail_out = m_bufferLen;
		int ret = deflate(m_z, Z_FINISH);

		size_t clen = m_bufferLen - m_z->avail_out;
		for (size_t amt, pos = 0; ok && pos < clen; pos += amt)
		{
			amt = m_file->Write(m_buffer + pos, clen - pos);
			if (amt == 0)
				ok = false;
		}

		if (ret == Z_STREAM_END || ret == Z_BUF_ERROR)
			break;

		if (ret != Z_OK)
		{
			ok = false;
		}
	}

	m_useCompression = false;

	if (m_isReading)
		inflateEnd(m_z);
	else
		deflateEnd(m_z);
	delete m_z;
	m_z = nullptr;
#endif
	return ok;
}

CompressedFileWriter::~CompressedFileWriter()
{
	// You must call .FinishCompression() after compressing data
	assert(!m_useCompression);
}
