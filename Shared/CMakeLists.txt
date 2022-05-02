# Shared Library

set(INCROOT ${CMAKE_CURRENT_SOURCE_DIR}/include/Shared/)
set(SRCROOT ${CMAKE_CURRENT_SOURCE_DIR}/src/)
set(PCHROOT ${CMAKE_CURRENT_SOURCE_DIR}/)

file(GLOB INC "${INCROOT}/*.hpp")
source_group("Public Headers" FILES ${INC})

file(GLOB SRC "${SRCROOT}/*.cpp" "${SRCROOT}/*.hpp")
source_group("Sources" FILES ${SRC})

# Platform specific source files
if(WIN32)
    file(GLOB PLATFORM_SRC "${SRCROOT}/Windows/*.cpp")
else(WIN32)
    file(GLOB UNIX_SRC "${SRCROOT}/Unix/*.cpp")
    if(APPLE)
        file(GLOB PLATFORM_SRC ${UNIX_SRC} "${SRCROOT}/MacOS/*.cpp")
    else(APPLE)
        file(GLOB PLATFORM_SRC ${UNIX_SRC} "${SRCROOT}/Linux/*.cpp")
    endif(APPLE)
endif(WIN32)
source_group("Sources\\Platform" FILES ${PLATFORM_SRC})


# Find files used for project
set(SHARED_SRC ${SRC} ${INC} ${PLATFORM_SRC})

set(PCH_SRC ${PCHROOT}/stdafx.cpp)
set(PCH_INC ${PCHROOT}/stdafx.h)
set(PCH_FILES ${PCH_SRC} ${PCH_INC})
source_group("" FILES ${PCH_FILES})

# Compiler stuff
enable_precompiled_headers("${SHARED_SRC}" ${PCH_SRC})

add_library(Shared ${SHARED_SRC} ${PCH_FILES})
target_compile_features(Shared PUBLIC cxx_std_17)
target_include_directories(Shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(Shared PRIVATE
    ${SRCROOT}
    ${INCROOT}
    ${PCHROOT}
)

target_link_libraries(Shared lua)

target_link_libraries(Shared ${Iconv_LIBRARIES})
target_include_directories(Shared SYSTEM PRIVATE ${Iconv_INCLUDE_DIRS})

target_link_libraries(Shared ${LibArchive_LIBRARIES})
target_include_directories(Shared SYSTEM PRIVATE ${LibArchive_INCLUDE_DIRS})

target_link_libraries(Shared cc-common)

find_package(ZLIB REQUIRED)
if(ZLIB_FOUND)
    target_compile_definitions(Shared PRIVATE ZLIB_FOUND=${ZLIB_FOUND})
    target_include_directories(Shared SYSTEM PRIVATE ${ZLIB_INCLUDE_DIRS})
endif()

# Enable multiprocess compiling
if(MSVC)
    target_compile_options(Shared PRIVATE /MP)
endif(MSVC)
