cmake_minimum_required(VERSION 3.30)
project(CopyProgramm)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(BOOST_INCLUDE_LIBRARIES interprocess)
set(BOOST_ENABLE_CMAKE ON)
FetchContent_Declare(
  boost
  URL https://github.com/boostorg/boost/releases/download/boost-1.86.0/boost-1.86.0-cmake.tar.gz
  URL_MD5 22b3b73a6d2aae063f27593f63503002
)
FetchContent_MakeAvailable(boost)

add_compile_options("$<$<CXX_COMPILER_ID:GNU,Clang>:-O3;-flto>")

add_subdirectory(src)

option(BUILD_TESTS "Build tests" OFF)

if (BUILD_TESTS)
    
    FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1 
    )
    FetchContent_MakeAvailable(catch2)

    enable_testing()
    add_subdirectory(tests)
endif ()