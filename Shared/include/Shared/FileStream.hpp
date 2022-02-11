#pragma once
#include "Shared/BinaryStream.hpp"
#include "Shared/Unique.hpp"
#include "Shared/File.hpp"

class FileStreamBase : public BinaryStream
{
protected:
	File* m_file = nullptr;
public:
	FileStreamBase()= default;
	FileStreamBase(File& file, bool isReading);
	virtual void Seek(size_t pos);
	virtual size_t Tell() const;
	virtual size_t GetSize() const;
	File& GetFile();
};

/* Stream that reads from a buffer */
class FileReader : public FileStreamBase
{
public:
	FileReader() = default;
	FileReader(File& file) : FileStreamBase(file, true) {};
	virtual size_t Serialize(void* data, size_t len);
};

/* Stream that writes to a buffer */
class FileWriter : public FileStreamBase
{
public:
	FileWriter() = default;
	FileWriter(File& file) : FileStreamBase(file, false) {};
	virtual size_t Serialize(void* data, size_t len);
};

