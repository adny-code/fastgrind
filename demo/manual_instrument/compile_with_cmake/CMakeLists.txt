# Minimal CMake build replicating ../compile_with_bash/build.sh logic
# Builds three static libraries (pkgA / pkgB / pkgC) and a final executable "app".
# It applies the same allocator wrap linker flags so the interception in memProbe.h works.
# Keep things intentionally simple and self‑contained.

cmake_minimum_required(VERSION 3.10)
project(manual_instrument_cmake_demo CXX)

# Default to Release if user did not specify a build type.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Use C++11 to match the bash script default.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Basic warning/optimization flags (user can still pass their own via CXXFLAGS when invoking cmake).
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -O3 -g)
endif()

# Path to repository root include directory (memProbe.h lives there).
set(REPO_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../include)

# Expose current source dir so we can #include "pkgA/PackageA.h" etc.
include_directories(
    ${REPO_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ---- Libraries ----
# Each package is built as a static library; dependency chain: pkgC -> pkgB -> pkgA.
add_library(pkgA STATIC pkgA/PackageA.cpp)
# Public include dirs allow dependents to include headers through the pkgX/ path.
target_include_directories(pkgA PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/pkgA)

add_library(pkgB STATIC pkgB/PackageB.cpp)
target_include_directories(pkgB PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/pkgB)
# pkgB needs headers from pkgA.
target_link_libraries(pkgB PUBLIC pkgA)

add_library(pkgC STATIC pkgC/PackageC.cpp)
target_include_directories(pkgC PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/pkgC)
# pkgC uses pkgB (which pulls in pkgA transitively).
target_link_libraries(pkgC PUBLIC pkgB)

# ---- Executable ----
add_executable(app main.cpp)
# Link order matches bash script: object -> C -> B -> A (transitive would already work, but keep explicit).
target_link_libraries(app PRIVATE pkgC pkgB pkgA pthread)

# ---- Allocation wrap flags (same list as build.sh) ----
set(WRAP_FLAGS
  -Wl,--wrap=malloc
  -Wl,--wrap=calloc
  -Wl,--wrap=realloc
  -Wl,--wrap=free
  -Wl,--wrap=_Znwm
  -Wl,--wrap=_Znam
  -Wl,--wrap=_ZdlPv
  -Wl,--wrap=_ZdaPv
  -Wl,--wrap=_ZnwmRKSt9nothrow_t
  -Wl,--wrap=_ZnamRKSt9nothrow_t
  -Wl,--wrap=_ZdlPvRKSt9nothrow_t
  -Wl,--wrap=_ZdaPvRKSt9nothrow_t
  -Wl,--wrap=valloc
  -Wl,--wrap=pvalloc
  -Wl,--wrap=memalign
  -Wl,--wrap=posix_memalign
  -Wl,--wrap=reallocarray
  -Wl,--wrap=_ZdaPvm
  -Wl,--wrap=_ZdlPvm
  -Wl,--wrap=aligned_alloc
  -Wl,--wrap=_ZnwmSt11align_val_t
  -Wl,--wrap=_ZnamSt11align_val_t
  -Wl,--wrap=_ZdlPvSt11align_val_t
  -Wl,--wrap=_ZdaPvSt11align_val_t
  -Wl,--wrap=_ZdlPvmSt11align_val_t
  -Wl,--wrap=_ZdaPvmSt11align_val_t
  -Wl,--wrap=_ZdlPvmRKSt9nothrow_t
  -Wl,--wrap=_ZdaPvmRKSt9nothrow_t
  -Wl,--wrap=_ZnwmSt11align_val_tRKSt9nothrow_t
  -Wl,--wrap=_ZnamSt11align_val_tRKSt9nothrow_t
  -Wl,--wrap=_ZdlPvSt11align_val_tRKSt9nothrow_t
  -Wl,--wrap=_ZdaPvSt11align_val_tRKSt9nothrow_t
  -Wl,--wrap=_ZdlPvmSt11align_val_tRKSt9nothrow_t
  -Wl,--wrap=_ZdaPvmSt11align_val_tRKSt9nothrow_t
)

# Apply linker flags only to the final executable.
target_link_options(app PRIVATE ${WRAP_FLAGS})

# Print a short summary at configure time (helps quick validation).
message(STATUS "Repo include dir: ${REPO_INCLUDE_DIR}")
message(STATUS "Wrap symbols count: ${WRAP_SYMBOLS_LENGTH}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Usage:
#   cd demo/manual_instrument/compile_with_cmake
#   cmake -S . -B build
#   cmake --build build -j
# Result: build/app
