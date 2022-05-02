#pragma once
#include "Shared/FileStream.hpp"
#ifdef ZLIB_FOUND
#include "zlib.h"
#endif

class CompressedFileStreamBase : public FileStreamBase
{
public:
	CompressedFileStreamBase(File& file, bool isReading, int level);
	~CompressedFileStreamBase();
	// Initialize zlib metadata but do not start using it yet
	bool InitCompression();
	// Initailize zlib and start using it for the rest of the stream
	bool StartCompression();
	bool IsUsingCompression() { return m_useCompression;  }

	virtual void Seek(size_t pos) override
	{
		assert(!m_useCompression);
		return FileStreamBase::Seek(pos);
	}
	virtual size_t Tell() const override
	{
		assert(!m_useCompression);
		return FileStreamBase::Tell();
	}
	virtual size_t GetSize() const override
	{
		assert(!m_useCompression);
		return FileStreamBase::Tell();
	}
protected:
	bool m_useCompression = false;
	int m_level;
#ifdef ZLIB_FOUND
	z_stream* m_z;
	uint8* m_buffer;
	size_t m_bufferLen = 512;
#endif
};

/* Stream that reads from a buffer */
class CompressedFileReader : public CompressedFileStreamBase
{
public:
	CompressedFileReader() = default;
	CompressedFileReader(File& file, int level = -1)
		: CompressedFileStreamBase(file, true, level) {};
	virtual size_t Serialize(void* data, size_t len);
};

/* Stream that writes to a buffer */
class CompressedFileWriter : public CompressedFileStreamBase
{
public:
	CompressedFileWriter() = default;
	~CompressedFileWriter();
	CompressedFileWriter(File& file, int level = -1)
		: CompressedFileStreamBase(file, false, level) {};
	virtual size_t Serialize(void* data, size_t len);
	// This must be called at the end of compressing
	bool FinishCompression();
};