# USC updater for Windows

set(SRCROOT ${CMAKE_CURRENT_SOURCE_DIR}/src/)

file(GLOB SRC "${SRCROOT}/*.cpp" "${SRCROOT}/*.hpp")
source_group("Source" FILES ${SRC})

set(UPDATER_SRC ${SRC})

add_executable(updater ${UPDATER_SRC})

target_compile_features(updater PUBLIC cxx_std_17)

target_link_libraries(updater cpr)
target_link_libraries(updater ${LibArchive_LIBRARIES})
target_include_directories(updater SYSTEM PRIVATE ${LibArchive_INCLUDE_DIRS})
