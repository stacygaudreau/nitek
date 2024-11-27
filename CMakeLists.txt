# project setup
cmake_minimum_required(VERSION 3.27)

project(nitek, VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-std=c++2a -Wall -Wextra -Werror -Wpedantic")
set(CMAKE_VERBOSE_MAKEFILE on)

include(CTest)

add_subdirectory(source/llbase)
add_subdirectory(source/nitek)

list(APPEND LIBS llbase)
list(APPEND LIBS nitek)

include_directories(${PROJECT_SOURCE_DIR}/source)
include_directories(${PROJECT_SOURCE_DIR}/source/nitek)

# nitek main target
file(GLOB NITEK_SOURCES "source/nitek/*.cpp")
add_executable(
        nitek_main
        ${NITEK_SOURCES}
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
target_compile_definitions(tests PRIVATE IS_TEST_SUITE)

# add gtest source to targets when building for test
if (BUILD_TESTING)
    target_include_directories(llbase PUBLIC ${gtest_SOURCE_DIR}/include ${gtest_BINARY_DIR}/include)
    target_include_directories(nitek PUBLIC ${gtest_SOURCE_DIR}/include ${gtest_BINARY_DIR}/include)
endif()

include(GoogleTest)
gtest_discover_tests(tests)