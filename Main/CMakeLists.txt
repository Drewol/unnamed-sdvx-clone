# Main Project

# Find files used for project
file(GLOB Audio_include "Audio/*.hpp")
file(GLOB Audio_src "Audio/*.cpp")
file(GLOB GUI_include "GUI/*.hpp")
file(GLOB GUI_src "GUI/*.cpp" "Audio/minimp3.c")
file(GLOB Main_src "*.cpp" "*.hpp"  "Resource.rc" 
	${Audio_include} ${Audio_src} ${GUI_src} ${GUI_include})

# Project filters

source_group("Header Files\\Audio" FILES ${Audio_include})
source_group("Source Files\\Audio" FILES ${Audio_src})
source_group("Header Files\\GUI" FILES ${GUI_include})
source_group("Source Files\\GUI" FILES ${GUI_src})

# Compiler stuff
enable_cpp11()
enable_precompiled_headers("${Main_src}" stdafx.cpp)
precompiled_header_exclude("Resource.rc")

include_directories(Audio GUI .)
add_executable(Main ${Main_src} ${C_src})
set_output_postfixes(Main)

# Target subsystem on windows, set debugging folder
if(MSVC)
   set_target_properties(Main PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
   set_target_properties(Main PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
endif(MSVC)
	
# Dependencies
target_link_libraries(Main Shared)
target_link_libraries(Main Graphics)
target_link_libraries(Main Audio)
target_link_libraries(Main GUI)
target_link_libraries(Main Beatmap)
target_link_libraries(Main squirrel)

if(WIN32)
	# Bundled libraries
	target_link_libraries(Main zlib)
	target_link_libraries(Main freetype)
	target_link_libraries(Main SDL2)
	target_link_libraries(Main png)
	target_link_libraries(Main jpeg)
	target_include_directories(Main PUBLIC ${SDL2_SOURCE_DIR}/include/)
else()
	set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/third_party/)
	include(FindLibraries)
	target_link_libraries(Main ${SDL2_LIBRARY})
endif()
