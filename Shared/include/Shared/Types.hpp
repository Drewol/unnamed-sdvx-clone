/*
	Provides typedefs for basic types such as int8, uint32, etc.
	Also includes most used c headers such as stdlib,cinsttypes,assert.
*/
#pragma once

// Include windows header on windows
#ifdef _WIN32

// Minimal windows headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Windows max macro nonsense
#ifdef max
#undef max
#undef min
#endif
#endif

// for int8_t,int16_t,etc.
#include <cinttypes>
// for assert()
#include <assert.h>
// for size_t
#include <stdlib.h>
// for memcpy()
#include <cstring>

// Convenient basic integer types without _t postfix
typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;
