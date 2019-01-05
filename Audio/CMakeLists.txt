# Audio library

# Find files used for project
file(GLOB Audio_src "src/*.cpp" "include/Audio/*.hpp" "src/minimp3.c" "src/minimp3.h" "src/libc.h")

# small mp3 library
file(GLOB minimp3_src "src/minimp3/*.c" "src/minimp3/*.h")

# The soundtouch library used for pitch shifting
file(GLOB soundtouch_src "src/soundtouch/src/*.cpp" "src/soundtouch/src/*.h" "src/soundtouch/include/*.h")

# Platform specific source files
if(WIN32)
	include_directories("src/Windows")
	file(GLOB Platform_src "src/Windows/*.cpp" "src/Windows/*.hpp")
	# Platform specific source group
	source_group("Source Files\\Windows" FILES ${Platform_src})
	source_group("SoundTouch" FILES ${soundtouch_src})
	source_group("minimp3" FILES ${minimp3_src})
endif(WIN32)

file(GLOB Audio_src ${Audio_src} ${Platform_src})

# Compiler stuff
enable_cpp14()
enable_precompiled_headers("${Audio_src}" src/stdafx.cpp)
precompiled_header_exclude("${minimp3_src}" "${soundtouch_src}")

include_directories(include include/Audio src src/soundtouch/src src/soundtouch/include src/minimp3)
add_library(Audio ${Audio_src} ${minimp3_src} ${soundtouch_src})

# Public include paths for library
target_include_directories(Audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Dependencies
target_link_libraries(Audio Shared)
if(WIN32)
	target_link_libraries(Audio SDL2)
	target_link_libraries(Audio vorbis)
	target_link_libraries(Audio ogg)
	target_include_directories(Audio PUBLIC ${SDL2_SOURCE_DIR}/include/)
else(WIN32)
	set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/third_party)
	include(FindLibraries)
	target_link_libraries(Audio ${OGG_LIBRARIES})
	target_link_libraries(Audio ${VORBIS_LIBRARIES})
endif(WIN32)
