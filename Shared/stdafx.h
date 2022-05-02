// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//
#pragma once

#ifdef _WIN32
// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN
// Windows Header Files:
#include <windows.h>
#endif

#include "Shared/Types.hpp"

#include <vector>
#include <map>
#include <math.h>
#include <cmath>
#include <type_traits>