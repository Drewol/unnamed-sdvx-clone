# Shared Library

# Find files used for project
file(GLOB Shared_src "src/*.cpp")
file(GLOB Shared_include "include/Shared/*.hpp")
source_group("Source Files" FILES ${Shared_src})
source_group("Header Files" FILES ${Shared_include})

# Platform specific source files
if(WIN32)
	file(GLOB Platform_src "src/Windows/*.cpp")
	source_group("Source Files\\Windows" FILES ${Platform_src})
else(WIN32)
	file(GLOB Unix_src "src/Unix/*.cpp")

	if(APPLE)
		file(GLOB Platform_src ${Unix_src} "src/MacOS/*.cpp")
	else(APPLE)
		file(GLOB Platform_src ${Unix_src} "src/Linux/*.cpp")
	endif(APPLE)
endif(WIN32)

# Compiler stuff
enable_cpp14()
enable_precompiled_headers("${Shared_src}" src/stdafx.cpp)

include_directories(include include/Shared src)
add_library(Shared ${Shared_src} ${Shared_include} ${Platform_src})

# Public include paths for library
target_include_directories(Shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)