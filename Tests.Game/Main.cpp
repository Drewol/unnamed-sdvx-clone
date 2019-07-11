#include "stdafx.h"
#include <Tests/TestManager.hpp>
#include <typeinfo>

void ListTests()
{
	TestManager& testManager = TestManager::Get();
	Vector<String> testNames = testManager.GetAvailableTests();
	size_t numTests = testNames.size();
	Logf("Available Tests:", Logger::Info, typeid(testManager).name());
	for(size_t i = 0; i < numTests; i++)
	{
		Logf(" %s", Logger::Info, testNames[i]);
	}
}

int main(int argc, char** argv)
{
	Vector<String> cmdLine = Path::SplitCommandLine(argc, argv);
	if(cmdLine.empty())
	{
		Logf("No test to run specified, please use %s <test name>", Logger::Error, Path::GetModuleName());
		ListTests();
		return 1;
	}

	String execName = cmdLine.back();
	TestManager& testManager = TestManager::Get();
	Vector<String> testNames = testManager.GetAvailableTests();
	if(!testNames.Contains(execName))
	{
		Logf("Test \"%s\" not found", Logger::Error, execName);
		ListTests();
		return 1;
	}

	return testManager.Run(execName);
}