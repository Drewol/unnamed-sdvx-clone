#include "stdafx.h"
#include "Debug.hpp"
#include "Log.hpp"
#include "Path.hpp"

#pragma warning(push)
#pragma warning(disable:4091)
#include <windows.h>
#include "Dbghelp.h"
#pragma warning(pop)
#pragma comment(lib, "dbghelp.lib")

namespace Debug
{
	static HANDLE processHandle = GetCurrentProcess();

	// Internal debug state object
	class Debug
	{
	public:
		Debug();
		~Debug();
		static Debug& Main();
	};

	Debug::Debug()
	{
		BOOL ret = SymInitialize(processHandle, nullptr, TRUE);
		if (ret == FALSE)
		{
			Logf("SymInitialize failed", Logger::Severity::Warning);
		}

		String executablePath = Path::GetExecutablePath();

		char symFileOut[256];
		char dbgFileOut[256];
		ret = SymGetSymbolFile(processHandle, nullptr, *executablePath, sfPdb, symFileOut, sizeof(symFileOut), dbgFileOut, sizeof(dbgFileOut));
	}
	Debug::~Debug()
	{
		SymCleanup(processHandle);
	}
	Debug& Debug::Main()
	{
		static Debug inst;
		return inst;
	}

	bool IsDebuggerAttached()
	{
		return ::IsDebuggerPresent() == TRUE;
	}

	Vector<StackFrame> GetStackTrace(uint32_t offset)
	{
		Debug::Main();
		Vector<StackFrame> ret;

		static void* stack[128];
		uint32_t frameCount = CaptureStackBackTrace(0, 100, stack, nullptr);

		const uint32_t maxSymbolNameLength = 256;
		const uint32_t symbolInfoStructSize = sizeof(SYMBOL_INFO) + maxSymbolNameLength;
		static char buffer[symbolInfoStructSize];
		PSYMBOL_INFO symInfo = (PSYMBOL_INFO)buffer;
		symInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
		symInfo->MaxNameLen = maxSymbolNameLength;
		static IMAGEHLP_LINE64 lineInfo;
		lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

		for (uint32_t i = offset + 1; i < frameCount; i++)
		{
			StackFrame frame;
			DWORD64 addr = (DWORD64)stack[i];
			BOOL br = SymFromAddr(processHandle, addr, nullptr, symInfo);
			DWORD displ;
			frame.address = (void*)addr;
			if (SymGetLineFromAddr64(processHandle, addr, &displ, &lineInfo))
			{
				frame.line = (uint32)lineInfo.LineNumber;
				frame.file = lineInfo.FileName;
				frame.function = String(symInfo->Name, symInfo->Name + symInfo->NameLen);
				ret.Add(frame);
			}
			else
			{
				frame.line = 0;
				frame.function = String(symInfo->Name, symInfo->Name + symInfo->NameLen);
				ret.Add(frame);
			}
		}
		return ret;
	}

	String GetFunctionNameFromAddress(void* address)
	{
		static const uint32_t maxSymbolNameLength = 256;
		static const uint32_t symbolInfoStructSize = sizeof(SYMBOL_INFO) + maxSymbolNameLength;
		static char buffer[symbolInfoStructSize];
		PSYMBOL_INFO symInfo = (PSYMBOL_INFO)buffer;
		symInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
		symInfo->MaxNameLen = maxSymbolNameLength;
		BOOL br = SymFromAddr(processHandle, (uint64_t)address, nullptr, symInfo);
		return symInfo->Name;
	}
	String GetLineInfoFromAddress(void* address, uint32& lineNumber)
	{
		static IMAGEHLP_LINE64 lineInfo;
		lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
		DWORD displ;
		SymGetLineFromAddr64(processHandle, (uint64_t)address, &displ, &lineInfo);
		lineNumber = lineInfo.LineNumber;
		return lineInfo.FileName;
	}
}
