# All third party libraries
add_subdirectory(lua-5.3.5)

#cpr test programs
option(CPR_BUILD_TESTS "Set to ON to build cpr tests." OFF)
set(CPR_ENABLE_SSL ON)
add_subdirectory(cpr)
target_include_directories(cpr PUBLIC ${CPR_INCLUDE_DIRS})

#discord example program
option(BUILD_EXAMPLES "Build example apps" OFF)
add_subdirectory(discord-rpc)
target_include_directories(discord-rpc PUBLIC discord-rpc/include)

#nanovg
add_library(nanovg nanovg/src/nanovg.c)
target_include_directories(nanovg PUBLIC nanovg/src)
target_link_libraries(nanovg ${FREETYPE_LIBRARIES})
target_include_directories(nanovg PRIVATE ${FREETYPE_INCLUDE_DIRS})
target_compile_definitions(nanovg PRIVATE -DFONS_USE_FREETYPE)

#miniaudio
add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio)

#json for modern c++
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json)

#sqlite
add_library(sqlite3
    sqlite3/sqlite3.c
    sqlite3/sqlite3.h
    sqlite3/sqlite3ext.h
)
target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sqlite3)

#minimp3
add_library(minimp3
    minimp3/minimp3.c
    minimp3/minimp3.h
    minimp3/libc.h
)
target_include_directories(minimp3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/minimp3)

#soundtouch
add_library(soundtouch
    soundtouch/src/AAFilter.cpp
    soundtouch/src/AAFilter.h
    soundtouch/src/BPMDetect.cpp
    soundtouch/include/BPMDetect.h
    soundtouch/src/cpu_detect_x86.cpp
    soundtouch/src/cpu_detect.h
    soundtouch/src/FIFOSampleBuffer.cpp
    soundtouch/include/FIFOSampleBuffer.h
    soundtouch/include/FIFOSamplePipe.h
    soundtouch/src/FIRFilter.cpp
    soundtouch/src/FIRFilter.h
    soundtouch/src/InterpolateCubic.cpp
    soundtouch/src/InterpolateCubic.h
    soundtouch/src/InterpolateLinear.cpp
    soundtouch/src/InterpolateLinear.h
    soundtouch/src/InterpolateShannon.cpp
    soundtouch/src/InterpolateShannon.h
    soundtouch/src/mmx_optimized.cpp
    soundtouch/src/PeakFinder.cpp
    soundtouch/src/PeakFinder.h
    soundtouch/src/RateTransposer.cpp
    soundtouch/src/RateTransposer.cpp
    soundtouch/include/soundtouch_config.h
    soundtouch/src/SoundTouch.cpp
    soundtouch/include/SoundTouch.h
    soundtouch/include/STTypes.h
    soundtouch/src/sse_optimized.cpp
    soundtouch/src/TDStretch.cpp
    soundtouch/src/TDStretch.h
)
target_include_directories(soundtouch PUBLIC soundtouch/include)
target_include_directories(soundtouch PRIVATE soundtouch/src)

#GLEW
add_library(GLEW
    glew/include/GL/glew.h
    glew/include/GL/glxew.h
    glew/include/GL/wglew.h
    glew/src/glew.c
)
# GLEW is included statically and also doesn't need GLU(Which doesn't even exist on linux)
target_compile_definitions(GLEW PUBLIC -DGLEW_NO_GLU -DGLEW_STATIC)
target_include_directories(GLEW PUBLIC glew/include)

# Enable multiprocess compiling
if(MSVC)
    target_compile_options(cpr PRIVATE /MP)
    target_compile_options(discord-rpc PRIVATE /MP)
    target_compile_options(GLEW PRIVATE /MP)
    target_compile_options(lua PRIVATE /MP)
    target_compile_options(minimp3 PRIVATE /MP)
    target_compile_options(nanovg PRIVATE /MP)
    target_compile_options(soundtouch PRIVATE /MP)
    target_compile_options(sqlite3 PRIVATE /MP)
endif(MSVC)