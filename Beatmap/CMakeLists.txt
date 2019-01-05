# Beatmap library

# Find files used for project
file(GLOB Beatmap_src "src/*.cpp" "include/Beatmap/*.hpp")
file(GLOB C_src "src/*.c")


# Compiler stuff
enable_cpp14()
enable_precompiled_headers("${Beatmap_src}" src/stdafx.cpp)
precompiled_header_exclude("${C_src}")

include_directories(include include/Beatmap src)
add_library(Beatmap ${Beatmap_src} ${C_src})

# Public include paths for library
target_include_directories(Beatmap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Dependencies
target_link_libraries(Beatmap Shared)