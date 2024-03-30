cmake_minimum_required(VERSION 3.27)
project(nitek, VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 20)

# tests with Catch2
# https://github.com/catchorg/Catch2/blob/devel/docs/cmake-integration.md#top

add_subdirectory(lib/Catch2)

enable_testing()

add_executable(
        nitek
        source/main.cpp
)

add_executable(
        tests
        tests/test_main.cpp
)

target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)

include(CTest)

