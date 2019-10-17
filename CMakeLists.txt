# Root CMake file
cmake_minimum_required(VERSION 3.8)
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

find_package(ZLIB REQUIRED)

project(FX)
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

# All projects use unicode define
#	this is mainly for windows functions either being defined to call A or W prefixed functions
add_definitions(-DUNICODE -D_UNICODE)

# Precompiled header macro
#	src 	= Path to source files
#	pchSrc 	= Path to precompiled header source file
macro(enable_precompiled_headers src pchSrc)
	if(MSVC)
		#message("Enabling precompiled header generated from source file ${pchSrc}")
		#message("Files using precompiled headers => ${src}")
		# Set precompiled header usage
		set_source_files_properties(${src} PROPERTIES COMPILE_FLAGS "/Yu")
		# Set precompiled header
		set_source_files_properties(${pchSrc} PROPERTIES COMPILE_FLAGS "/Yc")
	endif(MSVC)
endmacro(enable_precompiled_headers)

# Excludes a file from precompiled header usage
macro(precompiled_header_exclude exclude)
	if(MSVC)
		# Excluded files
		set_source_files_properties(${exclude} PROPERTIES COMPILE_FLAGS "")
	endif(MSVC)
endmacro(precompiled_header_exclude)

# Function to enable c++11 compilation on linux
macro(enable_cpp14)
	if(UNIX)
		# C++11 support enabled for linux compilers
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
	endif(UNIX)
endmacro(enable_cpp14)

# Set output binary postfixes so that they will be named <project>_<configuration>.exe/dll
macro(set_output_postfixes projectName)
	set_target_properties(${projectName} PROPERTIES 
		OUTPUT_NAME_DEBUG ${projectName}_Debug
		OUTPUT_NAME_RELEASE ${projectName})
endmacro(set_output_postfixes)

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
	
	# Put all third party libraries in a seperate folder in the VS solution
	#set_target_properties(jpeg PROPERTIES FOLDER "Third Party")
	#set_target_properties(png PROPERTIES FOLDER "Third Party")
	#set_target_properties(zlib PROPERTIES FOLDER "Third Party")
	#set_target_properties(ogg PROPERTIES FOLDER "Third Party")
	#set_target_properties(vorbis PROPERTIES FOLDER "Third Party")
	#set_target_properties(freetype PROPERTIES FOLDER "Third Party")
	#set_target_properties(archive PROPERTIES FOLDER "Third Party")
	#set_target_properties(archive_static PROPERTIES FOLDER "Third Party")
	#set_target_properties(SDL2 PROPERTIES FOLDER "Third Party")
	set_target_properties(cpr PROPERTIES FOLDER "Third Party")
	#set_target_properties(libcurl PROPERTIES FOLDER "Third Party/external")
    set_target_properties(nanovg PROPERTIES FOLDER "Third Party")
    #set_target_properties(nuklear PROPERTIES FOLDER "Third Party")
    set_target_properties(discord-rpc PROPERTIES FOLDER "Third Party")
    set_target_properties(lua PROPERTIES FOLDER "Third Party")
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
