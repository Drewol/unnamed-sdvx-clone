# Main Project

# Find files used for project
# Find miniaudio root
get_property(NLOHMANN_JSON_ROOT GLOBAL PROPERTY NLOHMANN_JSON_ROOT)
file(GLOB Audio_include "Audio/*.hpp")
file(GLOB Audio_src "Audio/*.cpp")
file(GLOB GUI_include "GUI/*.hpp")
file(GLOB nuk_include "nuklear/*.h")
file(GLOB GUI_src "GUI/*.cpp" "Audio/minimp3.c")
file(GLOB Main_src "*.cpp" "*.hpp"  "Resource.rc" 
	${Audio_include} ${Audio_src} ${GUI_src} ${GUI_include} ${nuk_include} ${NLOHMANN_JSON_HPP})

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

include_directories(Audio GUI . ${NLOHMANN_JSON_ROOT})
add_executable(usc-game ${Main_src} ${C_src})

if(${CMAKE_VERSION} VERSION_GREATER "3.11")
	add_compile_definitions(GIT_COMMIT=${GIT_DATE_HASH})
endif()

set_output_postfixes(usc-game)

# Target subsystem on windows, set debugging folder
if(MSVC)
   set_target_properties(usc-game PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
   set_target_properties(usc-game PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
endif(MSVC)

# Defines
target_compile_definitions(usc-game PUBLIC VERSION_MAJOR=${PROJECT_VERSION_MAJOR})
target_compile_definitions(usc-game PUBLIC VERSION_MINOR=${PROJECT_VERSION_MINOR})
target_compile_definitions(usc-game PUBLIC VERSION_PATCH=${PROJECT_VERSION_PATCH})

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
#target_link_libraries(usc-game archive)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/third_party/)
include(FindLibraries)
target_link_libraries(usc-game ${SDL2_LIBRARY})
target_link_libraries(usc-game ${LibArchive_LIBRARIES})

