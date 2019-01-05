# GUI library

# Find files used for project
file(GLOB GUI_src "src/*.cpp" "include/GUI/*.hpp" "include/GUI/*.h")

# Compiler stuff
enable_cpp14()
enable_precompiled_headers("${GUI_src}" src/stdafx.cpp)

include_directories(include/GUI)
add_library(GUI ${GUI_src})
set_output_postfixes(GUI)

target_include_directories(GUI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Dependencies
target_link_libraries(GUI Shared)
target_link_libraries(GUI nanovg)
target_link_libraries(GUI Graphics)
target_link_libraries(GUI lua)
