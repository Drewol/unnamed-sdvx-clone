# Beatmap library

# Import 3rd Party dependencies
get_property(SQLITE3_ROOT GLOBAL PROPERTY SQLITE3_ROOT)

# Find files used for project
file(GLOB Beatmap_src "src/*.cpp" "include/Beatmap/*.hpp")

# Compiler stuff
enable_cpp14()
enable_precompiled_headers("${Beatmap_src}" src/stdafx.cpp)

include_directories(include include/Beatmap src)
add_library(Beatmap ${Beatmap_src})

# Public include paths for library
target_include_directories(Beatmap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(Beatmap PUBLIC SYSTEM ${SQLITE3_ROOT})

# Dependencies
target_link_libraries(Beatmap Shared)
target_link_libraries(Beatmap sqlite3)
