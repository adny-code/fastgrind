# Minimal repo-local CMake integration using the exported fastgrind targets.
# This demo vendors the repository root and reuses fastgrind::auto together
# with fastgrind::instrument so every translation unit keeps the root flags.

cmake_minimum_required(VERSION 3.13)
project(auto_instrument_cmake_demo CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -O3 -g)
endif()

set(FASTGRIND_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
set(FASTGRIND_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FASTGRIND_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${FASTGRIND_ROOT_DIR} ${CMAKE_CURRENT_BINARY_DIR}/fastgrind)

add_library(pkgA STATIC pkgA/PackageA.cpp)
target_include_directories(pkgA PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/pkgA
)
target_link_libraries(pkgA PUBLIC fastgrind::includes fastgrind::instrument)

add_library(pkgB STATIC pkgB/PackageB.cpp)
target_include_directories(pkgB PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/pkgB
)
target_link_libraries(pkgB PUBLIC fastgrind::includes fastgrind::instrument pkgA)

add_library(pkgC STATIC pkgC/PackageC.cpp)
target_include_directories(pkgC PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/pkgC
)
target_link_libraries(pkgC PUBLIC fastgrind::includes fastgrind::instrument pkgB)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE fastgrind::auto pkgC pkgB pkgA pthread)

message(STATUS "Fastgrind root dir: ${FASTGRIND_ROOT_DIR}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Usage:
#   cd demo/auto_instrument/compile_with_cmake
#   cmake -S . -B build
#   cmake --build build -j
# Result: build/app
