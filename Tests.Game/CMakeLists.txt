# Game/Graphics Test Project

# Find files used for project
file(GLOB Main_src "*.cpp" "*.hpp")

# Compiler stuff
enable_cpp14()

include_directories(.)
add_executable(Tests.Game ${Main_src})
set_output_postfixes(Tests.Game)
enable_precompiled_headers("${Main_src}" stdafx.cpp)

if(MSVC)
	# SEH and Debug
	target_compile_options(Tests.Game PUBLIC /EHa /Zi)
else()
	target_compile_options(Tests.Game PUBLIC -rdynamic)
endif()	

# Dependencies
target_link_libraries(Tests.Game Shared)
target_link_libraries(Tests.Game Graphics)
target_link_libraries(Tests.Game Audio)
target_link_libraries(Tests.Game Beatmap)
target_link_libraries(Tests.Game GUI)
target_link_libraries(Tests.Game Tests)

if(WIN32)
else()
	set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/third_party)
	include(FindLibraries)
endif()
