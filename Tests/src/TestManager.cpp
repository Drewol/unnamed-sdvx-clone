#include "stdafx.h"
#include "TestManager.hpp"
#include <signal.h>

TestManager::TestManager()
{
}
TestManager::~TestManager()
{
}
TestManager& TestManager::Get()
{
	static TestManager inst;
	return inst;
}

int32 TestManager::RunAll()
{
	if(!m_Begin())
		return -1;
	Logf("Running tests for %s", Logger::Info, m_moduleName);

	int32 failed = 0;
	for(int32 i = 0; i < m_tests.size(); i++)
	{
		if(m_RunTest(m_tests[i]) != 0)
			failed++;
	}

	m_End();

	return failed;
}

int32 TestManager::Run(const String& testID)
{
	size_t* testToRun = m_testsByName.Find(testID);
	if(!testToRun)
	{
		Logf("Test not found \"%s\"", Logger::Error, testID);
		return 1;
	}
	return Run(testToRun[0]);
}
int32 TestManager::Run(size_t testIndex)
{
	if(testIndex >= m_tests.size())
	{
		Logf("Invalid test index: %d", Logger::Error, testIndex);
		return 1;
	}
	if(!m_Begin())
		return -1;
	int32 r = m_RunTest(m_tests[testIndex]);
	m_End();
	return r;
}
Vector<String> TestManager::GetAvailableTests() const
{
	Vector<String> testNames;
	for(auto& t : m_tests)
	{
		testNames.Add(t->m_name);
	}
	return testNames;
}
size_t TestManager::GetNumTests() const
{
	return m_tests.size();
}

bool TestManager::m_Begin()
{
	auto SignalHandler = [](int sig)->void
	{
		throw TestFailure();
	};

	void(*oldSigHandler)(int) = 0;
	if(!Debug::IsDebuggerAttached())
	{
		m_oldSigHandler = signal(SIGSEGV, SignalHandler);
	}

	// Store module name
	m_moduleName = Path::GetModuleName();

	// Create intermediate folder
	m_testBasePath = Path::Absolute("TestFilesystem_" + m_moduleName);
	if(Path::FileExists(m_testBasePath))
		Path::DeleteDir(m_testBasePath);
	if(!Path::CreateDir(m_testBasePath))
	{
		Logf("Failed to create folder for intermediate test files: %s", Logger::Info, m_testBasePath);
		return false;
	}

	return true;
}

void TestManager::m_End()
{

	// Remove intermediate folder
	Path::DeleteDir(m_testBasePath);

	// Restore signal handler
	if(!Debug::IsDebuggerAttached())
		signal(SIGSEGV, m_oldSigHandler);
}

int32 TestManager::m_RunTest(TestEntry* test)
{
	Logger::Get().SetColor(Logger::White);
	Logger::Get().WriteHeader(Logger::Info);
	Logger::Get().Write(Utility::Sprintf("Running test [%s]: ", test->m_name));

	TestContext context(test->m_name, this);

	if(Debug::IsDebuggerAttached())
	{
		// Don't catch any exceptions while debugging
		test->m_function(context);
	}
	else
	{
		try
		{
			test->m_function(context);
		}
		catch(TestFailure tf)
		{

			Logger::Get().SetColor(Logger::Red);
			Logger::Get().Write("Failed\n");
			if(!tf.expression.empty())
				Logf("The test expression failed:\n\t%s", Logger::Error, tf.expression);
			Log("Stack Trace:", Logger::Error);

			size_t i = 0;
			for(; i < tf.trace.size(); i++)
			{
				// Skip until wanted function
				if(tf.trace[i].function.find("LocalTest") == 0)
					break;
			}
			if(i == tf.trace.size())
				i = 0; // Show everything if wanted symbol was not found

			for(; i < tf.trace.size(); i++)
			{
				Debug::StackFrame sf = tf.trace[i];
				Logf("%016X %s (%d:%s)", Logger::Error, sf.address, sf.function, sf.line, sf.file);
			}
			return -1;
		}
	}

	Logger::Get().SetColor(Logger::Green);
	Logger::Get().Write("Passed\n");

	return 0;
}

TestEntry::TestEntry(String name, TestFunction function) : m_name(name), m_function(function)
{
	// Make sure tests have unique namings
	assert(!TestManager::Get().m_testsByName.Contains(name));
	TestManager::Get().m_testsByName.Add(name, TestManager::Get().m_tests.size());
	TestManager::Get().m_tests.Add(this);
}
String TestContext::GenerateTestFilePath() const
{
	return m_testManager->m_testBasePath + Path::sep + "Test_" + m_name;
}
String TestContext::GetTestBasePath() const
{
	return m_testManager->m_testBasePath;
}
