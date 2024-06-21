cmake_minimum_required(VERSION 3.10)
project(SINGER)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

option(ENABLE_NATIVE "Enable the compiler to use architecture-specific functionality; speeds up numerical computations but makes the resulting binaries less portable" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR "In-source builds not allowed. Please make a build directory and run CMake from there.\n")
endif()

# Native build results in a solver that is a lot faster, but less portable.
if(${ENABLE_NATIVE})
  add_compile_options(-march=native)
endif()

add_subdirectory(SINGER/SINGER)
