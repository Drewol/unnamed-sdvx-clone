#include <iostream>
#include <stdexcept>
#include <string>

#include <Windows.h>

#include "archive.h"
#include "archive_entry.h"

#include "Downloader.hpp"
#include "Extractor.hpp"

void StartUSC();

int main(int argc, const char* argv[])
{
	std::ios_base::sync_with_stdio(false);
	std::string archiveUrl = "https://www.drewol.me/Downloads/Game.zip";

	if (argc > 1)
	{
		std::cerr << "Waiting for the game to close..." << std::endl;
		DWORD uscPid = std::stol(argv[1]);
		HANDLE uscHandle = OpenProcess(SYNCHRONIZE, false, uscPid);
		WaitForSingleObject(uscHandle, INFINITE);
	}

	if (argc > 2)
	{
		archiveUrl = argv[2];
	}

	try
	{
		Downloader downloader;
		downloader.Download(archiveUrl);

		Extractor extractor;
		extractor.Extract(downloader.GetContent());
	}
	catch (std::runtime_error err)
	{
		std::cerr << "Updated failed due to an error.\n";
		std::cerr << err.what() << std::endl;

		std::cout << "Press ENTER to exit." << std::endl;
		std::cin.get();
		return 1;
	}

	std::cout << "Update completed. USC will restart after a moment..." << std::endl;

	StartUSC();
	Sleep(500);

	return 0;
}

void StartUSC()
{
	char currDir[MAX_PATH];
	GetCurrentDirectoryA(sizeof(currDir), currDir);
	std::string cd(currDir);
	std::string usc_path = cd + "\\usc-game.exe";

	STARTUPINFOA info = {sizeof(info)};
	PROCESS_INFORMATION processInfo;
	CreateProcessA(NULL, &usc_path.front(),
		NULL,
		NULL,
		FALSE,
		DETACHED_PROCESS,
		NULL,
		NULL,
		&info,
		&processInfo);
	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);
}