#pragma once
#include <Shared/Debug.hpp>
#include <Shared/Bindable.hpp>

// Test failure exception
class TestFailure
{
public:
	TestFailure(String expression = String()) : expression(expression)
	{
		trace = Debug::GetStackTrace();
	}
	String expression;
	Debug::StackTrace trace;
};

class TestEntry
{
public:
	typedef void(*TestFunction)(class TestContext& context);

	TestEntry(String name, TestFunction function);

private:
	String m_name;
	TestFunction m_function;
	friend class TestManager;
};

class TestManager
{
	TestManager();
public:
	~TestManager();
	static TestManager& Get();

	// Run all tests
	// returns the number of failed tests
	// 0 = all good
	int32 RunAll();

	// Run single individual tests
	int32 Run(const String& testID);
	int32 Run(size_t testIndex);

	// Query registered tests
	Vector<String> GetAvailableTests() const;
	size_t GetNumTests() const;

private:
	bool m_Begin();
	void m_End();
	int32 m_RunTest(TestEntry* test);
	Map<String, size_t> m_testsByName;
	Vector<TestEntry*> m_tests;
	String m_testBasePath;
	String m_moduleName;
	void (*m_oldSigHandler)(int);
	friend class TestEntry;
	friend class TestContext;
};

class TestContext
{
public:
	TestContext(String name, TestManager* mgr) : m_name(name), m_testManager(mgr) {};
	String GenerateTestFilePath() const;
	String GetTestBasePath() const;
	String GetName() const { return m_name; }

private:
	String m_name;
	TestManager* m_testManager;
};
