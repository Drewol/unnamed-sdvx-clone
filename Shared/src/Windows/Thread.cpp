#pragma once
#include "stdafx.h"
#include "Thread.hpp"
#include <windows.h>

size_t Thread::SetAffinityMask(size_t affinityMask)
{
	HANDLE h = (HANDLE)native_handle();
	size_t res = (uint32)SetThreadAffinityMask(h, affinityMask);
	return res;
}

size_t Thread::SetCurrentThreadAffinityMask(size_t affinityMask)
{
	HANDLE h = (HANDLE)GetCurrentThread();
	size_t res = (uint32)SetThreadAffinityMask(h, affinityMask);
	return res;
}
