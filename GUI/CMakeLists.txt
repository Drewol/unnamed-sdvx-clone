# GUI library

set(INCROOT ${CMAKE_CURRENT_SOURCE_DIR}/include/GUI/)
set(SRCROOT ${CMAKE_CURRENT_SOURCE_DIR}/src/)
set(PCHROOT ${CMAKE_CURRENT_SOURCE_DIR}/)

file(GLOB INC "${INCROOT}/*.hpp")
source_group("Public Headers" FILES ${INC})

file(GLOB SRC "${SRCROOT}/*.cpp" "${SRCROOT}/*.hpp")
source_group("Sources" FILES ${SRC})

# Find files used for project
set(GUI_SRC ${SRC} ${INC})

set(PCH_SRC ${PCHROOT}/stdafx.cpp)
set(PCH_INC ${PCHROOT}/stdafx.h)
set(PCH_FILES ${PCH_SRC} ${PCH_INC})
source_group("" FILES ${PCH_FILES})

# Compiler stuff
enable_precompiled_headers("${GUI_SRC}" ${PCH_SRC})

add_library(GUI ${GUI_SRC} ${PCH_FILES})
target_compile_features(GUI PUBLIC cxx_std_14)
target_include_directories(GUI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(GUI PRIVATE
    ${SRCROOT}
    ${INCROOT}
    ${PCHROOT}
)

# Dependencies
target_link_libraries(GUI Shared)
target_link_libraries(GUI nanovg)
target_link_libraries(GUI Graphics)
target_link_libraries(GUI lua)

if(WIN32)
    target_compile_options(Graphics PRIVATE /Zi)
endif()
