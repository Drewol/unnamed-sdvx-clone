#include "stdafx.h"
#include "Debug.hpp"
#include "Log.hpp"
#include "Path.hpp"

namespace Debug
{
	bool IsDebuggerAttached()
	{
		return false;
	}
	Vector<StackFrame> GetStackTrace(uint32_t offset)
	{
		Vector<StackFrame> ret;
		return ret;
	}
}