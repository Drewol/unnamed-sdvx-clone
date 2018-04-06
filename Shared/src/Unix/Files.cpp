#include "stdafx.h"
#include "Files.hpp"
#include "Path.hpp"
#include "Log.hpp"
#include "List.hpp"
#include "File.hpp"

#include <sys/types.h>
#include <dirent.h>

static Vector<FileInfo> _ScanFiles(String rootFolder, String extFilter, bool recurse, bool* interrupt)
{
	Vector<FileInfo> ret;
	if(!Path::IsDirectory(rootFolder))
	{
		Logf("Can't run ScanFiles, \"%s\" is not a folder", Logger::Warning, rootFolder);
		return ret;
	}

	// List of paths to process, subfolders are getting added to this list
	List<String> folderQueue;

	// Add / to the end
	folderQueue.AddBack(rootFolder);

	bool filterByExtension = !extFilter.empty();
	extFilter.TrimFront('.'); // Remove possible leading dot
							  // Recursive folder search
	while(!folderQueue.empty() && (!interrupt || !*interrupt))
	{
		String searchPath = folderQueue.front();
		folderQueue.pop_front();

		DIR* dir = opendir(*searchPath);
		if(dir == nullptr)
			continue;

		// Open first entry
		dirent* ent = readdir(dir);
		if(ent)
		{
			// Keep scanning files in this folder
			do
			{
                String filename = ent->d_name;

                /// TODO: Ask linux why
                if(filename == ".")
                    continue;
                if(filename == "..")
                    continue;

				FileInfo info;
                info.fullPath = Path::Normalize(searchPath + Path::sep + filename);
				info.lastWriteTime = File::GetLastWriteTime(info.fullPath); // linux doesn't provide this timestamp in the directory entry
				info.type = FileType::Regular;

				if(ent->d_type == DT_DIR)
				{
					if(recurse)
					{
						// Visit sub-folder
						folderQueue.AddBack(info.fullPath);
					}
					else if(!filterByExtension)
					{
                        info.type = FileType::Folder;
                        ret.Add(info);
					}
				}
				else
				{
					// Check file
					if(filterByExtension)
					{
						String ext = Path::GetExtension(info.fullPath);
						if(ext == extFilter)
						{
							ret.Add(info);
						}
					}
					else
					{
						ret.Add(info);
					}
				}
			} while((ent = readdir(dir)) && (!interrupt || !*interrupt));
		}

		closedir(dir);
	}

	return move(ret);
}

Vector<FileInfo> Files::ScanFiles(const String& folder, String extFilter, bool* interrupt)
{
	return _ScanFiles(folder, extFilter, false, interrupt);
}
Vector<FileInfo> Files::ScanFilesRecursive(const String& folder, String extFilter, bool* interrupt)
{
	return _ScanFiles(folder, extFilter, true, interrupt);
}
