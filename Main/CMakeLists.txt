# Main Project

# Find files used for project
file(GLOB Audio_include "Audio/*.hpp")
file(GLOB Audio_src "Audio/*.cpp")
file(GLOB GUI_include "GUI/*.hpp")
file(GLOB nuk_include "nuklear/*.h")
file(GLOB GUI_src "GUI/*.cpp" "Audio/minimp3.c")
file(GLOB Main_src "*.cpp" "*.hpp"  "Resource.rc" 
	${Audio_include} ${Audio_src} ${GUI_src} ${GUI_include} ${nuk_include})

# Project filters

source_group("Header Files\\Audio" FILES ${Audio_include})
source_group("Source Files\\Audio" FILES ${Audio_src})
source_group("Header Files\\nuklear" FILES ${nuk_include})
source_group("Header Files\\GUI" FILES ${GUI_include})
source_group("Source Files\\GUI" FILES ${GUI_src})

# Compiler stuff
enable_cpp14()
enable_precompiled_headers("${Main_src}" stdafx.cpp)
precompiled_header_exclude("Resource.rc")

include_directories(Audio GUI .)
add_executable(usc-game ${Main_src} ${C_src})
set_output_postfixes(usc-game)

# Target subsystem on windows, set debugging folder
if(MSVC)
   set_target_properties(usc-game PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
   set_target_properties(usc-game PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
endif(MSVC)
	
# Dependencies
target_link_libraries(usc-game Shared)
target_link_libraries(usc-game Graphics)
target_link_libraries(usc-game Audio)
target_link_libraries(usc-game GUI)
target_link_libraries(usc-game Beatmap)
target_link_libraries(usc-game nanovg)
#target_link_libraries(Main nuklear)
target_link_libraries(usc-game lua)
target_link_libraries(usc-game discord-rpc)
target_link_libraries(usc-game cpr)
target_link_libraries(usc-game jansson)

if(WIN32)
	# Bundled libraries
	target_link_libraries(usc-game zlib)
	target_link_libraries(usc-game freetype)
	target_link_libraries(usc-game SDL2)
	target_link_libraries(usc-game png)
	target_link_libraries(usc-game jpeg)
	target_include_directories(usc-game PUBLIC ${SDL2_SOURCE_DIR}/include/)
else()
	set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/third_party/)
	include(FindLibraries)
	target_link_libraries(usc-game ${SDL2_LIBRARY})
endif()
