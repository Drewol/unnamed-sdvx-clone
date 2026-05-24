# Root CMake file
cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 17 CACHE STRING "v")
set(CMAKE_CXX_STANDARD_REQUIRED True)
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

option(BUILD_SHARED_LIBS "Build libraries as shared libraries" OFF)
option(USC_GNU_WERROR "Set Werror for gcc." OFF)
option(USE_SYSTEM_CPR "Use system CPR" OFF)
set(CPR_USE_SYSTEM_CURL ON CACHE BOOL "Use vcpkg/system curl for cpr" FORCE)

project(USC VERSION 0.6.0)
if(WIN32 AND ${CMAKE_VERSION} VERSION_GREATER "3.12")
    cmake_policy(SET CMP0079 NEW)
endif()
if (${CMAKE_VERSION} VERSION_GREATER "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.12)
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

# Set library paths for MacOS
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOSX TRUE)

    # Set homebrew's include dir
    execute_process(
        COMMAND brew --prefix
        OUTPUT_VARIABLE HOMEBREW_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
    )
    include_directories("${HOMEBREW_PREFIX}/include")

    # Libarchive is shipped as a keg so we must get its path manually
    execute_process(
        COMMAND brew --prefix libarchive
        OUTPUT_VARIABLE LIBARCHIVE_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
    )
    set(LibArchive_INCLUDE_DIR "${LIBARCHIVE_PREFIX}/include")
endif()

# Set path for discord-rpc dependency so that it doesn't download old version
if (${CMAKE_VERSION} VERSION_GREATER "4.0")
    set(DISCORD_RPC_THIRDPARTY_DIR
        ${PROJECT_SOURCE_DIR}/third_party/discord-rpc/thirdparty/
    )
    set(RAPIDJSONTEST
        ${PROJECT_SOURCE_DIR}/third_party/rapidjson
        CACHE PATH "Bundled rapidjson path for discord-rpc" FORCE
    )
    set(RAPIDJSON
        ${PROJECT_SOURCE_DIR}/third_party/rapidjson
        CACHE PATH "Bundled rapidjson path for discord-rpc" FORCE
    )
    file(
        MAKE_DIRECTORY
        ${DISCORD_RPC_THIRDPARTY_DIR}
    )
    if (WIN32)
        file(
            COPY
            ${PROJECT_SOURCE_DIR}/third_party/rapidjson
            DESTINATION ${DISCORD_RPC_THIRDPARTY_DIR}
        )
    else()
        file(
            CREATE_LINK
            ${PROJECT_SOURCE_DIR}/third_party/rapidjson
            ${DISCORD_RPC_THIRDPARTY_DIR}/rapidjson
            SYMBOLIC
        )
    endif()
endif()

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
    set_target_properties(rapidjson PROPERTIES FOLDER "Third Party")
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

install(TARGETS usc-game RUNTIME)
install(DIRECTORY bin/audio DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/unnamed-sdvx-clone)
install(DIRECTORY bin/fonts DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/unnamed-sdvx-clone)
install(DIRECTORY bin/LightPlugins DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/unnamed-sdvx-clone)
install(DIRECTORY bin/skins DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/unnamed-sdvx-clone)
