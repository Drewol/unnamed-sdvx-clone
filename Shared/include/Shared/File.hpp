#pragma once
#include "Shared/Unique.hpp"
#include "Shared/String.hpp"
#include "Shared/Buffer.hpp"

class File : Unique
{
private:
	class File_Impl* m_impl = nullptr;
public:
	File();
	~File();

	bool OpenRead(const String& path);
	bool OpenWrite(const String& path, bool append = false, bool noLog = false);
	void Close();
	// Seek from file start
	void Seek(size_t pos);
	// Seek from current position
	void Skip(int64 pos);
	// Seek from the end of the file
	void SeekReverse(size_t pos);
	size_t Tell() const;
	size_t GetSize() const;
	size_t Read(void* data, size_t len);
	size_t Write(const void* data, size_t len);

	// Get the last write time of the file
	uint64 GetLastWriteTime() const;

	// Get the last write time of a file at a given path
	static uint64 GetLastWriteTime(const String& path);
	static uint64 FileTimeToUnixTimestamp(uint64 fileTime);
};

/* 
	Functions for resources compiled with the executable 
	WINDOWS ONLY
*/
namespace EmbeddedResource
{
	enum ResourceType
	{
		AniCursor = 21,
		AniIcon = 22,
		Bitmap = 2,
		Cursor = 1,
		Menu = 4,
		RCData = 10,
		String = 6,
	};
	bool LoadResource(const ::String& resourceName, Buffer& out, ResourceType resourceType = RCData);
	bool LoadResource(uint32 resourceID, Buffer& out, ResourceType resourceType = RCData);
}