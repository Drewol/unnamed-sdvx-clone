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
class Path
{
public:
	// Working dir
	static String GetCurrentPath();
	static String GetExecutablePath();
	// The filename of the executable
	static String GetModuleName();

	// Create a new directory
	// the directory must not already exist
	static bool CreateDir(const String& path);
	// Creates a recursive directory tree
	static bool CreateDirRecursive(String path);
	// Renames a file or folder
	// overwrite specifies if the target should be overwritten if it already exists
	static bool Rename(const String& srcFile, const String& dstFile, bool overwrite = false);
	// Copies a single file
	// overwrite specifies if the target should be overwritten if it already exists
	static bool Copy(const String& srcFile, const String& dstFile, bool overwrite = false);
	// Remove/Delete a file
	static bool Delete(const String& path);
	// Remove/Delete a directory
	static bool DeleteDir(const String& path);
	// Removes all files inside of a folder
	static bool ClearDir(const String& path);
	// Copies a folder and all files in it
	// the target directory must not exist
	static bool CopyDir(String srcFolder, String dstFolder);
	// Go to specified path using the system default file browser
	static bool ShowInFileBrowser(const String& path);
	// Open external program with specified parameters (used to open charts in editor)
	static bool Run(const String& programPath, const String& parameters);

	// Generatea a temporary file name that can be used for temporary files (undefined location)
	// This file is always a non-existing file
	static String GetTemporaryPath();
	// Generatea a temporary file path with custom requirements such as the folder that should contain the file and a prefix for the file name
	// This file is always a non-existing file
	static String GetTemporaryFileName(const String& path, const String& prefix = String());

	static Vector<String> GetSubDirs(const String& path);

	// Check if the given path points to a directory
	static bool IsDirectory(const String& path);
	// Check if a file/folder exists at given location
	static bool FileExists(const String& path);
	// Converts a path to a standard format with all duplicate slashes removed and set to the platform default '\' on windows
	static String Normalize(const String& path);
	// Return the absolute path variant of the given input path
	static String Absolute(const String& path);
	// Check if the given path is absolute (false if relative)
	static bool IsAbsolute(const String& path);
	// Removes the last directory or filename ('/' seperated)
	// returns the removed element in lastOut if set
	static String RemoveLast(const String& path, String* lastOut = nullptr);
	// Removes a base path from the input path
	static String RemoveBase(String path, String base);

	// Returns the extension found in given input path
	static String GetExtension(const String& path);
	// Replace the extension in the input path(if any) and replaces it with a new one (or add one if none exists)
	// If the new extension is empty the '.' character that would normally come before the new extension is removed
	static String ReplaceExtension(String path, String newExt);

	// Removes the first argument without spaces or within quotes from the input string and returns it
	static String ExtractPathFromCmdLine(String& input);
	// Same as ExtractPathFromCmdLine but returns the result in an array and keeps the input intact
	static Vector<String> SplitCommandLine(const String& input);
	static Vector<String> SplitCommandLine(int argc, char** argv);

	// The character seperating directories/files 
	// '/' on linux
	// '\' on windows
	static char sep;

	// The path to the game directory
	// used to create absolute paths to game directories
	// If not set, it will default to the game binary directory
	static String gameDir;
};
