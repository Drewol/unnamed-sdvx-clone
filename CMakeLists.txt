# Root CMake file
cmake_minimum_required(VERSION 3.12)
#set(VCPKG_CRT_LINKAGE static)
#set(VCPKG_LIBRARY_LINKAGE static)
#set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "Vcpkg target triplet (ex. x86-windows)")

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
    message("Found vcpkg root '$ENV{VCPKG_ROOT}'")
elseif(WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR "Could not find vcpkg root")
endif()


project(USC VERSION 0.5.0)
if(WIN32 AND ${CMAKE_VERSION} VERSION_GREATER "3.12")
    cmake_policy(SET CMP0079 NEW)
endif()
# Project configurations
set(CMAKE_CONFIGURATION_TYPES Debug Release)
set(CMAKE_DEBUG_POSTFIX _Debug)
set(CMAKE_RELEASE_POSTFIX _Release)
execute_process(COMMAND git log -1 --date=short --format="%cd_%h"
                OUTPUT_VARIABLE GIT_DATE_HASH
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE)


# Set output folders
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${PROJECT_SOURCE_DIR}/bin )
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${PROJECT_SOURCE_DIR}/bin )
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${PROJECT_SOURCE_DIR}/lib )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

set(CMAKE_MACOSX_RPATH 1)

# Set folder where to find FindXXX.cmake and
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/Modules/")

# Find external dependencies
find_package(Freetype REQUIRED)
find_package(ZLIB REQUIRED)
find_package(SDL2 REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(Vorbis REQUIRED)
find_package(OGG REQUIRED)
find_package(LibArchive REQUIRED)
find_package(Iconv REQUIRED)

# All projects use unicode define
# this is mainly for windows functions either being defined to call A or W prefixed functions
add_definitions(-DUNICODE -D_UNICODE)

#https://stackoverflow.com/questions/60041896/reuse-target-compile-options-from-variable-for-multiple-targets-cmake/60047012#60047012
add_library(cc-common INTERFACE)

if(WIN32)
    target_compile_options(cc-common INTERFACE /Zi)
endif()

OPTION(EMBEDDED "Enable embedded build" OFF)

if(EMBEDDED)
	message("Enabling embedded build")
    add_definitions(-DEMBEDDED)
endif()

OPTION(CRASHDUMP "Enable collecting crash dumps" ON)
if(CRASHDUMP)
    message("Enabling crash dumps")
    add_definitions(-DCRASHDUMP)
endif()

OPTION(ASAN "Build With ASAN" OFF)
if(ASAN)
    target_compile_options(cc-common INTERFACE
        -fsanitize=address
        -fno-omit-frame-pointer
    )
endif()

# Include macros
include(${PROJECT_SOURCE_DIR}/cmake/Macros.cmake)

# Sub-Project directories
add_subdirectory(third_party)
add_subdirectory(Shared)
add_subdirectory(Graphics)
add_subdirectory(Main)
add_subdirectory(Audio)
add_subdirectory(Beatmap)
add_subdirectory(GUI)

# Unit test projects
add_subdirectory(Tests)
add_subdirectory(Tests.Shared)
add_subdirectory(Tests.Game)

# Enabled project filters on windows
if(MSVC)
    #updater for windows
    add_subdirectory(updater)

    # Use filters in VS projects
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)

    # Set usc-game as default target in VS
    set_directory_properties(PROPERTY VS_STARTUP_PROJECT usc-game)

    # Put all third party libraries in a seperate folder in the VS solution
    set_target_properties(cpr PROPERTIES FOLDER "Third Party")
    set_target_properties(nanovg PROPERTIES FOLDER "Third Party")
    set_target_properties(sqlite3 PROPERTIES FOLDER "Third Party")
    set_target_properties(discord-rpc PROPERTIES FOLDER "Third Party")
    set_target_properties(minimp3 PROPERTIES FOLDER "Third Party")
    set_target_properties(soundtouch PROPERTIES FOLDER "Third Party")
    set_target_properties(lua PROPERTIES FOLDER "Third Party")
    set_target_properties(GLEW PROPERTIES FOLDER "Third Party")

    # My libraries in the libraries folder
    set_target_properties(Shared PROPERTIES FOLDER Libraries)
    set_target_properties(Graphics PROPERTIES FOLDER Libraries)
    set_target_properties(Audio PROPERTIES FOLDER Libraries)
    set_target_properties(Beatmap PROPERTIES FOLDER Libraries)
    set_target_properties(GUI PROPERTIES FOLDER Libraries)

    # Unit tests
    set_target_properties(Tests PROPERTIES FOLDER "Tests")
    set_target_properties(Tests.Shared PROPERTIES FOLDER "Tests")
    set_target_properties(Tests.Game PROPERTIES FOLDER "Tests")

endif(MSVC)
