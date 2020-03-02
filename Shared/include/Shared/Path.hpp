#pragma once
#include "Shared/String.hpp"
#include "Shared/Vector.hpp"

/* 
	File path API 
	Operates on string objects that contain path strings
	Valid formats are windows strings using \'s as directory seperators and X: as drive letters

	You should make sure that your path names are always normalized when using any of these functions, so no wrong direction seperators and double seperators
	It's a good idea to use Normalize when using various string sources or user input to create a path
*/
namespace Path
{
	// Working dir
	String GetCurrentPath();
	String GetExecutablePath();
	// The filename of the executable
	String GetModuleName();

	// Create a new directory
	// the directory must not already exist
	bool CreateDir(const String& path);
	// Creates a recursive directory tree
	bool CreateDirRecursive(String path);
	// Renames a file or folder
	// overwrite specifies if the target should be overwritten if it already exists
	bool Rename(const String& srcFile, const String& dstFile, bool overwrite = false);
	// Copies a single file
	// overwrite specifies if the target should be overwritten if it already exists
	bool Copy(const String& srcFile, const String& dstFile, bool overwrite = false);
	// Remove/Delete a file
	bool Delete(const String& path);
	// Remove/Delete a directory
	bool DeleteDir(const String& path);
	// Removes all files inside of a folder
	bool ClearDir(const String& path);
	// Copies a folder and all files in it
	// the target directory must not exist
	bool CopyDir(String srcFolder, String dstFolder);
	// Go to specified path using the system default file browser
	bool ShowInFileBrowser(const String& path);
	// Open external program with specified parameters (used to open charts in editor)
	bool Run(const String& programPath, const String& parameters);

	// Generatea a temporary file name that can be used for temporary files (undefined location)
	// This file is always a non-existing file
	String GetTemporaryPath();
	// Generatea a temporary file path with custom requirements such as the folder that should contain the file and a prefix for the file name
	// This file is always a non-existing file
	String GetTemporaryFileName(const String& path, const String& prefix = String());

	Vector<String> GetSubDirs(const String& path);

	// Check if the given path points to a directory
	bool IsDirectory(const String& path);
	// Check if a file/folder exists at given location
	bool FileExists(const String& path);
	// Converts a path to a standard format with all duplicate slashes removed and set to the platform default '\' on windows
	String Normalize(const String& path);
	// Return the absolute path variant of the given input path
	String Absolute(const String& path);
	// Check if the given path is absolute (false if relative)
	bool IsAbsolute(const String& path);
	// Removes the last directory or filename ('/' seperated)
	// returns the removed element in lastOut if set
	String RemoveLast(const String& path, String* lastOut = nullptr);
	// Removes a base path from the input path
	String RemoveBase(String path, String base);

	// Returns the extension found in given input path
	String GetExtension(const String& path);
	// Replace the extension in the input path(if any) and replaces it with a new one (or add one if none exists)
	// If the new extension is empty the '.' character that would normally come before the new extension is removed
	String ReplaceExtension(String path, String newExt);

	// Removes the first argument without spaces or within quotes from the input string and returns it
	String ExtractPathFromCmdLine(String& input);
	// Same as ExtractPathFromCmdLine but returns the result in an array and keeps the input intact
	Vector<String> SplitCommandLine(const String& input);
	Vector<String> SplitCommandLine(int argc, char** argv);
	
	// The character seperating directories/files 
	// '/' on linux
	// '\' on windows
	extern const char sep;
};