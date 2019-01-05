# Graphics library

# Find files used for project
file(GLOB Graphics_src "src/*.cpp" "src/glew.c" "include/Graphics/*.hpp")

# Platform specific source files
if(WIN32)
	include_directories("src/Windows")
	file(GLOB Platform_src "src/Windows/*.cpp" "src/Windows/*.hpp")
	# Platform specific source group
	source_group("Source Files\\Windows" FILES ${Platform_src})
else(WIN32)
	include_directories("src/Linux")
	file(GLOB Platform_src "src/Linux/*.cpp")
endif(WIN32)

file(GLOB Graphics_src ${Graphics_src} ${Platform_src})

# Compiler stuff
enable_cpp14()
enable_precompiled_headers("${Graphics_src}" src/stdafx.cpp)
precompiled_header_exclude("src/glew.c")

# Look for static included GLEW before other directories
include_directories(BEFORE include include/Graphics src include/Graphics/glew)
add_library(Graphics ${Graphics_src})

# Public include paths for library
target_include_directories(Graphics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/include/Graphics/glew)
	
# GLEW is included statically and also doesn't need GLU(Which doesn't even exist on linux)
add_definitions(-DGLEW_NO_GLU -DGLEW_STATIC)

# Dependencies
target_link_libraries(Graphics Shared)
if(WIN32)
	target_link_libraries(Graphics zlib)
	target_link_libraries(Graphics freetype)
	target_link_libraries(Graphics SDL2)
	target_link_libraries(Graphics png)
	target_link_libraries(Graphics jpeg)
	target_include_directories(Graphics PUBLIC ${SDL2_SOURCE_DIR}/include/)
else(WIN32)

	set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/third_party)
	include(FindLibraries)
	target_link_libraries(Graphics ${FREETYPE_LIBRARIES})
	target_link_libraries(Graphics ${SDL2_LIBRARY})
	target_link_libraries(Graphics ${JPEG_LIBRARIES})
	target_link_libraries(Graphics ${PNG_LIBRARIES})
	target_link_libraries(Graphics pthread) 
	target_link_libraries(Graphics dl)

	if(APPLE)
		find_library(OPEN_GL OpenGL)
		target_link_libraries(Graphics ${OPEN_GL})
	else(APPLE)
		target_link_libraries(Graphics GL)
		find_package(X11 REQUIRED)
		include_directories(${X11_INCLUDE_DIR})
	endif(APPLE)

	include_directories(${FREETYPE_INCLUDE_DIRS})
	find_package(Freetype REQUIRED)
endif(WIN32)
