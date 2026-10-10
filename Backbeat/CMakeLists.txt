set(INCROOT ${CMAKE_CURRENT_SOURCE_DIR}/include/Backbeat/)
set(SRCROOT ${CMAKE_CURRENT_SOURCE_DIR}/src/)

file(GLOB INC "${INCROOT}/*.hpp")
file(GLOB SRC "${SRCROOT}/*.cpp" "${SRCROOT}/*.hpp")

add_library(BackbeatIntegration ${SRC} ${INC})
target_compile_features(BackbeatIntegration PUBLIC cxx_std_17)
target_include_directories(BackbeatIntegration PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(BackbeatIntegration PUBLIC Shared Backbeat::Backbeat)
target_link_libraries(BackbeatIntegration PRIVATE cc-common)

if(MSVC)
	target_compile_options(BackbeatIntegration PRIVATE /MP)
endif()
