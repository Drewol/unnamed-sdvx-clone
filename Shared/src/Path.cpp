#include "stdafx.h"
#include "Path.hpp"
#include "Files.hpp"

/*
	Common
*/
String Path::gameDir = "";

String Path::Absolute(const String& path)
{
	if (IsAbsolute(path))
		return path;

	String baseDir = !gameDir.empty() ? gameDir : RemoveLast(GetExecutablePath());
	return baseDir + sep + path;
}
String Path::RemoveLast(const String& path, String* lastOut /*= nullptr*/)
{
	String left;
	if (path.SplitLast(String() + sep, &left, lastOut))
		return left;
	return path;
}
String Path::RemoveBase(String path, String base)
{
	size_t targetOffset = 0;
	while (!base.empty())
	{
		size_t offset = base.find(sep);
		String segment = base;
		if (offset != -1)
		{
			// Cut off folder part
			segment = base.substr(0, offset);
			if (path.size() > (offset + 1))
				base = base.substr(offset + 1);
			else
				base.clear();
		}
		else
		{
			base.clear();
		}

		if (path.compare(targetOffset, segment.size(), segment) == 0)
		{
			if (path.size() > (segment.size() + targetOffset))
			{
				char followingChar = path[targetOffset + segment.size()];
				if (followingChar != sep && followingChar != '.')
					break;
				targetOffset += 1;
			}
			targetOffset += segment.size();
		}
		else
			break;
	}
	return path.substr(targetOffset);
}
String Path::GetExtension(const String& path)
{
	size_t dotPos = path.find_last_of(".");
	if (dotPos == std::string::npos)
		return String();
	size_t sepPos = path.find_last_of(Path::sep);
	if (sepPos == std::string::npos || dotPos > sepPos)
		return path.substr(dotPos + 1);
	return String();
}
String Path::ExtractPathFromCmdLine(String& input)
{
	String r;
	input.Trim();
	bool q = input[0] == '"';
	size_t i = q ? 1 : 0;
	while (i < input.size())
	{
		if (q)
		{
			if (input[i] == '"')
			{
				break;
			}
		}
		else
		{
			if (input[i] == ' ')
			{
				break;
			}
		}
		r += input[i];
		i++;
	}
	input.erase(input.begin(), input.begin() + r.size() + (q ? 2 : 0));
	input.Trim();
	return r;
}
Vector<String> Path::SplitCommandLine(const String& input)
{
	String buffer = input;
	Vector<String> res;
	while (!buffer.empty())
	{
		String a = ExtractPathFromCmdLine(buffer);
		a.Trim();
		if (!a.empty())
			res.push_back(a);
	}
	return res;
}

Vector<String> Path::SplitCommandLine(int argc, char** argv)
{
	Vector<String> res;
	for (int i = 0; i < argc; i++)
	{
		String a = argv[i];
		a.Trim();
		if (!a.empty())
			res.push_back(a);
	}
	return res;
}

String Path::GetModuleName()
{
	String moduleName = Path::GetExecutablePath();
	Path::RemoveLast(moduleName, &moduleName);
	return moduleName;
}
bool Path::CreateDirRecursive(String path)
{
	String path1;
	while (!path.empty())
	{
		String segment = path;
		size_t offset = path.find(Path::sep);
		if (offset != -1)
		{
			segment = path.substr(0, offset);
			if (path.size() > (offset + 1))
				path = path.substr(offset + 1);
			else
				path.clear();
		}
		else
		{
			path.clear();
		}

		if (!path1.empty())
			path1 += Path::sep;
		path1 += segment;

		// Create if not existing
		if (IsDirectory(path1) || path1.empty())
			continue;
		if (!CreateDir(path1))
			return false;
	}

	return true;
}
bool Path::ClearDir(const String& path)
{
	Vector<FileInfo> files = Files::ScanFiles(path);
	for (auto& file : files)
	{
		if (file.type == FileType::Folder)
		{
			if (!DeleteDir(file.fullPath))
				return false;
		}
		else
		{
			if (!Delete(file.fullPath))
				return false;
		}
	}
	return true;
}
bool Path::CopyDir(String srcFolder, String dstFolder)
{
	srcFolder = Absolute(srcFolder);

	// Remove trailing seperators
	dstFolder = Absolute(dstFolder);
	dstFolder.TrimBack(Path::sep);

	if (!CreateDir(dstFolder))
		return false;

	Vector<FileInfo> files = Files::ScanFiles(srcFolder);
	for (auto& file : files)
	{
		String commonPath = RemoveBase(file.fullPath, srcFolder);
		String dstPath = dstFolder + Path::sep + commonPath;
		if (file.type == FileType::Folder)
		{
			if (!CopyDir(file.fullPath, dstPath))
				return false;
		}
		else
		{
			if (!Copy(file.fullPath, dstPath))
				return false;
		}
	}
	return true;
}
