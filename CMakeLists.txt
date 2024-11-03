cmake_minimum_required(VERSION 3.30)
project(CopyProgramm)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.86 REQUIRED)

add_compile_options("$<$<CXX_COMPILER_ID:GNU,Clang>:-O3;-flto>")

add_subdirectory(src)

option(BUILD_TESTS "Build tests" OFF)

if (BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1 
    )
    FetchContent_MakeAvailable(catch2)

    enable_testing()
    add_subdirectory(tests)
endif ()