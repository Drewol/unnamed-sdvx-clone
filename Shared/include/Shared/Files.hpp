#pragma once
#include "Shared/String.hpp"
#include "Shared/Vector.hpp"
#include "Vector.hpp"
#include "Map.hpp"

enum class FileType
{
	Regular = 0,
	Folder
};

/*
	Result of file finding operations
*/
struct FileInfo
{
	String fullPath;
	uint64 lastWriteTime;
	FileType type;

};

/*
	File enumeration functions
*/
class Files
{
public:
	// Finds files in a given folder
	// uses the given extension filters if specified (results will be returned in a map with given exts as keys)
	// Additional interruptible flag can contain a boolean which can interrupt the search when set to true
	static Map<String, Vector<FileInfo>> ScanFiles(const String& folder, const Vector<String>& extFilters, bool* interrupt = nullptr);

	// Finds files in a given folder, recursively
	// uses the given extension filters if specified (results will be returned in a map with given exts as keys)
	// Additional interruptible flag can contain a boolean which can interrupt the search when set to true
	static Map<String, Vector<FileInfo>> ScanFilesRecursive(const String& folder, const Vector<String>& extFilters, bool* interrupt = nullptr);

	// Finds files in a given folder
	// uses the given extension filter if specified
	// Additional interruptible flag can contain a boolean which can interrupt the search when set to true
	static Vector<FileInfo> ScanFiles(const String& folder, const String& extFilter = String(), bool* interrupt = nullptr);

	// Finds files in a given folder, recursively
	// uses the given extension filter if specified
	// Additional interruptible flag can contain a boolean which can interrupt the search when set to true
	static Vector<FileInfo> ScanFilesRecursive(const String& folder, const String& extFilter = String(), bool* interrupt = nullptr);
};
