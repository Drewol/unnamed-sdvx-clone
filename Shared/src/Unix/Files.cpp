#include "stdafx.h"
#include "Files.hpp"
#include "Path.hpp"
#include "Log.hpp"
#include "List.hpp"
#include "File.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

static Map<String, Vector<FileInfo>> _ScanFiles(const String& rootFolder, const Vector<String>& extFilters, bool recurse, bool* interrupt)
{
	// Found files will go in here. If there is no filter extensions or only "" then all files will have "" as their key
	Map<String, Vector<FileInfo>> ret;

	Vector<String> fixedExts;
	for (int i = 0; i < extFilters.size(); i++)
	{
		// Not a reference or const bc we need a copy so we can trim it
		String ext = extFilters[i];

		// Add empty vectors for collecting results
		ret[ext] = Vector<FileInfo>();

		ext.TrimFront('.');
		fixedExts.push_back(ext); // Remove possible leading dot
	}

	if (!Path::IsDirectory(rootFolder))
	{
		Logf("Can't run ScanFiles, \"%s\" is not a folder", Logger::Severity::Warning, rootFolder);
		return ret;
	}

	// List of paths to process, subfolders are getting added to this list
	List<String> folderQueue;

	// Add / to the end
	folderQueue.AddBack(rootFolder);

	// Either if we have no exts or no exts besides an empty string
	bool filterByExtension = extFilters.size() != 0 && !(extFilters.size() == 1 && fixedExts[0].empty());
	// Make sure the empty one is ready
	if (!filterByExtension)
		ret[""] = Vector<FileInfo>();

	while (!folderQueue.empty() && (!interrupt || !*interrupt))
	{
		String searchPath = folderQueue.front();
		folderQueue.pop_front();

		DIR* dir = opendir(*searchPath);
		if (dir == nullptr)
			continue;

		// Open first entry
		dirent* ent = readdir(dir);
		if (ent)
		{
			// Keep scanning files in this folder
			do
			{
				String filename = ent->d_name;

				/// TODO: Ask linux why
				if (filename == ".")
					continue;
				if (filename == "..")
					continue;

				FileInfo info;
				info.fullPath = Path::Normalize(searchPath + Path::sep + filename);
				info.lastWriteTime = File::GetLastWriteTime(info.fullPath); // linux doesn't provide this timestamp in the directory entry
				info.type = FileType::Regular;
				bool is_dir = (ent->d_type == DT_DIR);
				if (ent->d_type == DT_UNKNOWN || ent->d_type == DT_LNK)
				{
					struct stat buffer;
					stat(info.fullPath.c_str(), &buffer);
					is_dir = S_ISDIR(buffer.st_mode);
				}
				if (is_dir)
				{
					if (recurse)
					{
						// Visit sub-folder
						folderQueue.AddBack(info.fullPath);
					}
					else if (!filterByExtension)
					{
						info.type = FileType::Folder;
						ret[""].push_back(info);
					}
				}
				else
				{
					// Check file
					if (filterByExtension)
					{
						String ext = Path::GetExtension(info.fullPath);
						for (int i = 0; i < extFilters.size(); i++)
						{
							const String& extFilter = fixedExts[i];
							if (ext == extFilter)
							{
								const String& realExt = extFilters[i];
								ret[realExt].push_back(info);
								break;
							}
						}
					}
					else
					{
						ret[""].push_back(info);
					}
				}
			} while ((ent = readdir(dir)) && (!interrupt || !*interrupt));
		}

		closedir(dir);
	}

	return move(ret);
}

Map<String, Vector<FileInfo>> Files::ScanFiles(const String& folder, const Vector<String>& extFilters, bool* interrupt)
{
	return _ScanFiles(folder, extFilters, false, interrupt);
}
Map<String, Vector<FileInfo>> Files::ScanFilesRecursive(const String& folder, const Vector<String>& extFilters, bool* interrupt)
{
	return _ScanFiles(folder, extFilters, true, interrupt);
}

Vector<FileInfo> Files::ScanFiles(const String& folder, const String& extFilter, bool* interrupt)
{
	return _ScanFiles(folder, Vector<String>(1, extFilter), false, interrupt)[extFilter];
}
Vector<FileInfo> Files::ScanFilesRecursive(const String& folder, const String& extFilter, bool* interrupt)
{
	return _ScanFiles(folder, Vector<String>(1, extFilter), true, interrupt)[extFilter];
}
