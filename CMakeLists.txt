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


# test target with Gtest
file(GLOB TEST_SOURCES "tests/*.cpp")
# FetchContent to load and build googletest
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG main
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# declare and link tests target
enable_testing()
add_executable(
        tests
        ${TEST_SOURCES}
)
target_link_libraries(tests PRIVATE gtest_main PUBLIC ${LIBS})

include(GoogleTest)
gtest_discover_tests(tests)