# project setup
cmake_minimum_required(VERSION 3.27)

project(nitek, VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-std=c++2a -Wall -Wextra -Werror -Wpedantic")
set(CMAKE_VERBOSE_MAKEFILE on)

add_subdirectory(source/utils)
add_subdirectory(source/nitek)

list(APPEND LIBS utils)
list(APPEND LIBS nitek)

include_directories(${PROJECT_SOURCE_DIR}/source)
include_directories(${PROJECT_SOURCE_DIR}/source/nitek)

# nitek main target
add_executable(
        nitek_main
        source/nitek/nitek_main.cpp
)
target_link_libraries(nitek_main PUBLIC ${LIBS})

file(GLOB TEST_SOURCES "tests/*.cpp")

# tests target with Catch2
# https://github.com/catchorg/Catch2/blob/devel/docs/cmake-integration.md#top
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/Catch2)
add_executable(
        tests
        ${TEST_SOURCES}
)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain PUBLIC ${LIBS})