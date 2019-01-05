# Shared Library

# Find files used for project
file(GLOB Tests_src "src/*.cpp")
file(GLOB Tests_include "include/Tests/*.hpp")
source_group("Source Files" FILES ${Tests_src})
source_group("Header Files" FILES ${Tests_include})

# Platform specific source files
if(WIN32)
	file(GLOB Platform_src "src/Windows/*.cpp")
	source_group("Source Files\\Windows" FILES ${Platform_src})
else(WIN32)
	file(GLOB Platform_src "src/Linux/*.cpp")
endif(WIN32)

# Compiler stuff
enable_cpp14()
enable_precompiled_headers("${Tests_src}" src/stdafx.cpp)

include_directories(include include/Tests src)
add_library(Tests ${Tests_src} ${Tests_include} ${Platform_src})

if(MSVC)
	# SEH and Debug
	target_compile_options(Tests PUBLIC /EHa /Zi)
endif()

target_link_libraries(Tests Shared)

# Public include paths for library
target_include_directories(Tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
