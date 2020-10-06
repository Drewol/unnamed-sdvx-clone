# Audio library

set(INCROOT ${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/)
set(SRCROOT ${CMAKE_CURRENT_SOURCE_DIR}/src/)
set(PCHROOT ${CMAKE_CURRENT_SOURCE_DIR}/)

file(GLOB INC "${INCROOT}/*.hpp")
source_group("Public Headers" FILES ${INC})

file(GLOB SRC "${SRCROOT}/*.cpp" "${SRCROOT}/*.hpp")
source_group("Sources" FILES ${SRC})

# Platform specific source files
if(WIN32)
    file(GLOB PLATFORM_SRC "${SRCROOT}/Windows/*.cpp")
else()
    file(GLOB PLATFORM_SRC "${SRCROOT}/Unix/*.cpp")
endif(WIN32)
source_group("Sources\\Platform" FILES ${PLATFORM_SRC})

set(AUDIO_SRC ${SRC} ${INC} ${PLATFORM_SRC})

set(PCH_SRC ${PCHROOT}/stdafx.cpp)
set(PCH_INC ${PCHROOT}/stdafx.h)
set(PCH_FILES ${PCH_SRC} ${PCH_INC})
source_group("" FILES ${PCH_FILES})

# Compiler stuff
enable_precompiled_headers("${AUDIO_SRC}" "${PCH_SRC}")

add_library(Audio ${AUDIO_SRC} ${PCH_FILES})
target_compile_features(Audio PUBLIC cxx_std_17)
target_include_directories(Audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(Audio PRIVATE
    ${SRCROOT}
    ${INCROOT}
    ${PCHROOT}
)

if(UNIX AND NOT APPLE)
    target_compile_options(Audio PUBLIC -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-parameter -Wno-unused-function -Wno-unused-value) 
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(Audio PUBLIC -Wno-maybe-uninitialized) 
    endif(CMAKE_BUILD_TYPE STREQUAL "Release")
endif(UNIX AND NOT APPLE)

# Dependencies
## Local Dependencies
target_link_libraries(Audio Shared)

## 3rd Party dependencies
### Find dependencies roots
target_include_directories(Audio PUBLIC ${SDL2_INCLUDE_DIR})
### Add dependencies to linkage
target_link_libraries(Audio soundtouch)
target_link_libraries(Audio minimp3)
target_link_libraries(Audio miniaudio)
target_link_libraries(Audio ${SDL2_LIBRARY})
target_link_libraries(Audio ${OGG_LIBRARIES})
target_link_libraries(Audio ${Vorbis_LIBRARIES})

if(WIN32)
    target_compile_options(Graphics PRIVATE /Zi)
endif()
