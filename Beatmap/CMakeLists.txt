# Beatmap library

set(ENV{ASAN_OPTIONS}, "new_delete_type_mismatch=0")

set(INCROOT ${CMAKE_CURRENT_SOURCE_DIR}/include/Beatmap/)
set(SRCROOT ${CMAKE_CURRENT_SOURCE_DIR}/src/)
set(PCHROOT ${CMAKE_CURRENT_SOURCE_DIR}/)

file(GLOB INC "${INCROOT}/*.hpp")
source_group("Public Headers" FILES ${INC})

file(GLOB SRC "${SRCROOT}/*.cpp" "${SRCROOT}/*.hpp")
source_group("Sources" FILES ${SRC})

# Find files used for project
set(BEATMAP_SRC ${SRC} ${INC})

set(PCH_SRC ${PCHROOT}/stdafx.cpp)
set(PCH_INC ${PCHROOT}/stdafx.h)
set(PCH_FILES ${PCH_SRC} ${PCH_INC})
source_group("" FILES ${PCH_FILES})

# Compiler stuff
enable_precompiled_headers("${BEATMAP_SRC}" ${PCH_SRC})

add_library(Beatmap ${BEATMAP_SRC} ${PCH_FILES})
target_compile_features(Beatmap PUBLIC cxx_std_17)
target_include_directories(Beatmap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(Beatmap PRIVATE
    ${INCROOT}
    ${SRCROOT}
    ${PCHROOT}
)

# Dependencies

## Local Dependencies
target_link_libraries(Beatmap Shared)
target_link_libraries(Beatmap BackbeatIntegration)

## 3rd Party dependencies
target_link_libraries(Beatmap sqlite3)
target_link_libraries(Beatmap nlohmann_json)

target_link_libraries(Beatmap cc-common)

# Enable multiprocess compiling
if(MSVC)
    target_compile_options(Beatmap PRIVATE /MP)
endif(MSVC)
