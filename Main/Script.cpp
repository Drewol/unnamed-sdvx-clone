#include "Shared/Shared.hpp"
#include "Script.hpp"

#include "squirrel.h"

#include <iostream>
#include <fstream>

#define SQVM_INIT_STACK_SIZE (1024)

static int ReadFileAscii(SQUserPointer file)
{
	std::ifstream* fileStream = (std::ifstream *)file;

	char c;
	fileStream->read(&c, 1);

	if (fileStream->fail())
		return 0;

	return c;
}

static void CompilerErrorHandler(HSQUIRRELVM vm, const SQChar* desc, const SQChar* source,
	SQInteger line, SQInteger column)
{
}

class Script_Impl : public Script
{
private:
	HSQUIRRELVM m_sqvm;
	const String& m_scriptPath;

public:
	Script_Impl(const String& scriptPath)
		: m_scriptPath(scriptPath)
	{
	}

	~Script_Impl()
	{
		Close();
	}

	virtual bool AsyncLoad() override
	{
		m_sqvm = sq_open(SQVM_INIT_STACK_SIZE);
		sq_setcompilererrorhandler(m_sqvm, CompilerErrorHandler);

		std::ifstream fileStream (m_scriptPath, std::ios::binary);
		if (!fileStream.is_open())
		{
			// LOGGGG
			return false;
		}

		// all of this just to get a wchar_t*, help me
		// (yess I know I can disable SQUNICODE but why otherwise)

		const char* pathData = m_scriptPath.GetData();
		const size_t cSize = strlen(pathData) + 1;
		std::wstring wc(cSize, L'#');
		mbstowcs(&wc[0], pathData, cSize);

		SQRESULT cResult = sq_compile(m_sqvm, ReadFileAscii, &fileStream, &wc[0], 1);
		fileStream.close();

		if (cResult == SQ_ERROR)
		{
			// logging here would be handled by the compile error handler we set earlier
			return false;
		}

		return true;
	}

	virtual bool AsyncFinalize() override
	{
		return true;
	}

	void Close()
	{
		sq_close(m_sqvm);
	}
};

Script* Script::Create(const String& scriptPath)
{
	Script* impl = new Script_Impl(scriptPath);
	return nullptr;
}
