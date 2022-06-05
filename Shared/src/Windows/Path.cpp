#include "stdafx.h"
#include "Path.hpp"
#include "Log.hpp"
#include "Shellapi.h"

#include <algorithm>

/*
	Windows version
*/
#include <windows.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

char Path::sep = '\\';

bool Path::CreateDir(const String& path)
{
	WString wpath = Utility::ConvertToWString(path);
	return CreateDirectoryW(*wpath, nullptr) == TRUE;
}
bool Path::Delete(const String& path)
{
	WString wpath = Utility::ConvertToWString(path);
	return DeleteFileW(*wpath) == TRUE;
}
bool Path::DeleteDir(const String& path)
{
	if (!ClearDir(path))
		return false;
	WString wpath = Utility::ConvertToWString(path);
	return RemoveDirectoryW(*wpath) == TRUE;
}
bool Path::Rename(const String& srcFile, const String& dstFile, bool overwrite)
{
	WString wsrc = Utility::ConvertToWString(srcFile);
	WString wdst = Utility::ConvertToWString(dstFile);
	if (PathFileExistsW(*wdst) == TRUE)
	{
		if (!overwrite)
			return false;
		if (DeleteFileW(*wdst) == FALSE)
		{
			Logf("Failed to rename file, overwrite was true but the destination could not be removed", Logger::Severity::Warning);
			return false;
		}
	}
	return MoveFileW(*wsrc, *wdst) == TRUE;
}
bool Path::Copy(const String& srcFile, const String& dstFile, bool overwrite)
{
	WString wsrc = Utility::ConvertToWString(srcFile);
	WString wdst = Utility::ConvertToWString(dstFile);
	return CopyFileW(*wsrc, *wdst, overwrite) == TRUE;
}
String Path::GetCurrentPath()
{
	wchar_t currDir[MAX_PATH];
	GetCurrentDirectoryW(sizeof(currDir), currDir);
	return Utility::ConvertToUTF8(currDir);
}
String Path::GetExecutablePath()
{
	wchar_t filename[MAX_PATH];
	GetModuleFileNameW(GetModuleHandle(0), filename, sizeof(filename));
	return Utility::ConvertToUTF8(filename);
}
String Path::GetTemporaryPath()
{
	wchar_t path[MAX_PATH];
	::GetTempPathW(sizeof(path), path);
	return Utility::ConvertToUTF8(path);
}
String Path::GetTemporaryFileName(const String& path, const String& prefix)
{
	wchar_t out[MAX_PATH];
	WString wpath = Utility::ConvertToWString(path);
	WString wprefix = Utility::ConvertToWString(prefix);

	BOOL r = ::GetTempFileNameW(*wpath, *wprefix, 0, out);
	assert(r == TRUE);

	return Utility::ConvertToUTF8(out);
}
bool Path::IsDirectory(const String& path)
{
	WString wpath = Utility::ConvertToWString(path);
	DWORD attribs = GetFileAttributesW(*wpath);
	return (attribs != INVALID_FILE_ATTRIBUTES) && (attribs & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
}
bool Path::FileExists(const String& path)
{
	WString wpath = Utility::ConvertToWString(path);
	return PathFileExistsW(*wpath) == TRUE;
}
String Path::Normalize(const String& path)
{
	wchar_t out[MAX_PATH] = { 0 };
	WString wpath = Utility::ConvertToWString(path);
	// Convert a unix style path so we can correctly handle it
	std::replace(wpath.begin(), wpath.end(), L'/', static_cast<wchar_t>(sep));
	// Remove any relative path . or ..
	PathCanonicalizeW(out, *wpath);
	return Utility::ConvertToUTF8(out);
}
bool Path::IsAbsolute(const String& path)
{
	if (path.length() > 2 && path[1] == ':')
		return true;
	return false;
}

Vector<String> Path::GetSubDirs(const String& path)
{
	Vector<String> res;
	WString searchPathW = Utility::ConvertToWString(path + "\\*");
	WIN32_FIND_DATA findDataW;
	HANDLE searchHandle = FindFirstFile(*searchPathW, &findDataW);
	if (searchHandle == INVALID_HANDLE_VALUE)
		return res;

	do
	{
		String filename = Utility::ConvertToUTF8(findDataW.cFileName);
		if (filename == ".")
			continue;
		if (filename == "..")
			continue;
		if (findDataW.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			res.Add(filename);
	} while (FindNextFile(searchHandle, &findDataW));
	FindClose(searchHandle);

	return res;
}

bool Path::ShowInFileBrowser(const String& path)
{
	WString wpath = Utility::ConvertToWString(path);

	// Opens the directory, if a file path is sent then the file will be opened with the default program for that file type.
	// See also: https://stackoverflow.com/a/49694181
	const int res = static_cast<int>(reinterpret_cast<uintptr_t>(ShellExecuteW(NULL, L"open", *wpath, NULL, NULL, SW_SHOWDEFAULT)));

	if (res > 32)
	{
		return true;
	}
	else
	{
		switch (res)
		{
		case ERROR_FILE_NOT_FOUND:
			Logf("Failed to show file \"%s\" in the system default explorer: File not found.", Logger::Severity::Error, path);
			break;
		case ERROR_PATH_NOT_FOUND:
			Logf("Failed to show file \"%s\" in the system default explorer: Path not found.", Logger::Severity::Error, path);
			break;
		default:
			Logf("Failed to show file \"%s\" in the system default explorer: error %d", Logger::Severity::Error, path, res);
			break;
		}
		return false;
	}
}

bool Path::Run(const String& programPath, const String& parameters)
{
	STARTUPINFOW info = { sizeof(info) };
	PROCESS_INFORMATION processInfo;

	WString command = Utility::WSprintf(L"%S %S", *programPath, *parameters);

	if (!Path::FileExists(programPath))
	{
		Logf("Failed to open editor: invalid path \"%s\"", Logger::Severity::Error, programPath);
		return false;
	}

	if (CreateProcessW(NULL, command.GetData(), NULL, NULL, false, CREATE_NEW_CONSOLE, NULL, NULL, &info, &processInfo))
	{
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
	}
	else
	{
		Logf("Failed to open editor: error %d", Logger::Severity::Error, GetLastError());
		return false;
	}
	return true;
}
