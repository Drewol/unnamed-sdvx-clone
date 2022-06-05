#include "stdafx.h"
#include "File.hpp"
#include "Log.hpp"
#include "Buffer.hpp"

/*
	Unix implementation
*/
#ifdef __APPLE__
#include <errno.h>
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// for fstat
#include <sys/types.h>
#include <sys/stat.h>

class File_Impl
{
public:
	File_Impl(int h) : handle(h) {};
	~File_Impl()
	{
		close(handle);
	}
	int handle;
};

File::File()
{
}
File::~File()
{
	Close();
}
bool File::OpenRead(const String& path)
{
	Close();

	int handle = open(*path, O_RDONLY);
	if (handle == -1)
	{
		Logf("Failed to open file for reading %s: %d", Logger::Severity::Warning, *path, errno);
		return false;
	}

	m_impl = new File_Impl(handle);

	return true;
}
bool File::OpenWrite(const String& path, bool append /*= false*/, bool noLog /*= false*/)
{
	Close();

	int flags = O_WRONLY | O_CREAT;
	if (append)
		flags |= O_APPEND;
	int handle = open(*path, flags, S_IRUSR | S_IWUSR | S_IROTH);
	if (handle == -1)
	{
		if (!noLog)
			Logf("Failed to open file for writing %s: %d", Logger::Severity::Warning, *path, errno);
		return false;
	}

	m_impl = new File_Impl(handle);

	return true;
}
void File::Close()
{
	if (m_impl)
	{
		delete m_impl;
		m_impl = nullptr;
	}
}
void File::Seek(size_t pos)
{
	assert(m_impl);
	lseek(m_impl->handle, pos, SEEK_SET);
}
void File::Skip(int64 pos)
{
	assert(m_impl);
	lseek(m_impl->handle, pos, SEEK_CUR);
}
void File::SeekReverse(size_t pos)
{
	assert(m_impl);
	lseek(m_impl->handle, pos, SEEK_END);
}
size_t File::Tell() const
{
	assert(m_impl);
	return lseek(m_impl->handle, 0, SEEK_CUR);
}
size_t File::GetSize() const
{
	assert(m_impl);
	struct stat sb;
	fstat(m_impl->handle, &sb);
	return sb.st_size;
}
size_t File::Read(void* data, size_t len)
{
	assert(m_impl);
	return read(m_impl->handle, data, (uint32)len);
}
size_t File::Write(const void* data, size_t len)
{
	assert(m_impl);
	return write(m_impl->handle, data, (uint32)len);
}

uint64 File::GetLastWriteTime() const
{
	assert(m_impl);
	struct stat sb;
	fstat(m_impl->handle, &sb);

#ifdef __APPLE__
	return sb.st_mtimespec.tv_sec * (uint64)1000000000L + sb.st_mtimespec.tv_nsec;
#else
	return sb.st_mtim.tv_sec * (uint64)1000000000L + sb.st_mtim.tv_nsec;
#endif
}

uint64 File::GetLastWriteTime(const String& path)
{
	struct stat sb;
	if (stat(*path, &sb) != 0)
		return 0;

#ifdef __APPLE__
	return sb.st_mtimespec.tv_sec * (uint64)1000000000L + sb.st_mtimespec.tv_nsec;
#else
	return sb.st_mtim.tv_sec * (uint64)1000000000L + sb.st_mtim.tv_nsec;
#endif
}

uint64 File::FileTimeToUnixTimestamp(uint64 fileTime) {
	return fileTime / 1000000000;
}


bool LoadResourceInternal(const char* name, const char* type, Buffer& out)
{
	return false;
}
bool EmbeddedResource::LoadResource(const ::String& resourceName, Buffer& out, ResourceType resourceType)
{
	return false;
}
bool EmbeddedResource::LoadResource(uint32 resourceID, Buffer& out, ResourceType resourceType /*= RCData*/)
{
	return false;
}
