#pragma once
#include "Shared/String.hpp"

namespace Debug
{
	struct StackFrame
	{
		String function;
		String file;
		uint32 line;
		void* address;
	};

	typedef Vector<StackFrame> StackTrace;

	// Checks if the debugger is attached
	[[nodiscard]]
	bool IsDebuggerAttached();

	// Gets a stack trace at the current function call
	[[nodiscard]]
	StackTrace GetStackTrace(uint32 offset = 0);

	// Translates addresses to function name if debug info is present
	[[nodiscard]]
	String GetFunctionNameFromAddress(void* address);
	[[nodiscard]]
	String GetLineInfoFromAddress(void* address, uint32& lineNumber);
}