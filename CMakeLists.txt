cmake_minimum_required(VERSION 3.13)

project(fastgrind VERSION 1.0 LANGUAGES CXX)

# -----------------------------------------------------------------------------
# Global configuration
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Install into /usr/local by default (CMake's default; keep explicit for clarity)
set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Install prefix")

include(GNUInstallDirs)


# Public include directory for all targets in this project
add_library(fastgrind_includes INTERFACE)
target_include_directories(fastgrind_includes INTERFACE ${CMAKE_SOURCE_DIR}/include)

# -----------------------------------------------------------------------------
# Linker wrap flags (centralized)
# Memory allocation function wrappers for instrumentation and interception
# -----------------------------------------------------------------------------
set(WRAP_FLAGS
    # Standard C memory allocation functions
    -Wl,--wrap=malloc
    -Wl,--wrap=calloc
    -Wl,--wrap=realloc
    -Wl,--wrap=free
    # C++ new/delete operators (mangled names)
    -Wl,--wrap=_Znwm
    -Wl,--wrap=_Znam
    -Wl,--wrap=_ZdlPv
    -Wl,--wrap=_ZdaPv
    -Wl,--wrap=_ZnwmRKSt9nothrow_t
    -Wl,--wrap=_ZnamRKSt9nothrow_t
    -Wl,--wrap=_ZdlPvRKSt9nothrow_t
    -Wl,--wrap=_ZdaPvRKSt9nothrow_t
    # System calls (commented out for stability)
    # -Wl,--wrap=sbrk
    # -Wl,--wrap=brk
    # -Wl,--wrap=mmap
    # -Wl,--wrap=munmap
    # -Wl,--wrap=mremap
    # POSIX and GNU extension memory functions
    -Wl,--wrap=valloc
    -Wl,--wrap=pvalloc
    -Wl,--wrap=memalign
    -Wl,--wrap=posix_memalign
    -Wl,--wrap=reallocarray
    -Wl,--wrap=_ZdaPvm
    -Wl,--wrap=_ZdlPvm
    -Wl,--wrap=aligned_alloc
    # C++17 aligned allocation operators
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
add_library(fastgrind_wrap_flags INTERFACE)
target_link_options(fastgrind_wrap_flags INTERFACE ${WRAP_FLAGS})

# -----------------------------------------------------------------------------
# Instrumentation exclusion list & helper interface
# -----------------------------------------------------------------------------
set(INSTRUMENT_EXCLUDE_FILES
    /usr/include/
    /usr/lib/
    /usr/local/
    /opt/local/
    fastgrind.h
    # other third-party headers can be added here
)
list(JOIN INSTRUMENT_EXCLUDE_FILES "," INSTRUMENT_EXCLUDE_FILES_JOINED)

# Define instrumentation compile options variable (shared usage)
set(fastgrind_base_compile_opts -O3)

set(fastgrind_instrument_compile_opts ${fastgrind_base_compile_opts} -DFASTGRIND_INSTRUMENT)
list(APPEND fastgrind_instrument_compile_opts -finstrument-functions -finstrument-functions-exclude-file-list=${INSTRUMENT_EXCLUDE_FILES_JOINED})

# Export all symbols for dynamic lookup
add_library(fastgrind_instrument INTERFACE)
target_compile_options(fastgrind_instrument INTERFACE ${fastgrind_instrument_compile_opts})
target_link_options(fastgrind_instrument INTERFACE -Wl,--export-dynamic)

# -----------------------------------------------------------------------------
# Subdirectories
# -----------------------------------------------------------------------------
add_subdirectory(testcase/glibc_je_tc_availabe)
add_subdirectory(testcase/multi_pkg_compile)
add_subdirectory(testcase/benchmark_box_grouping)
add_subdirectory(testcase/cpp_feature_test)

# -----------------------------------------------------------------------------
# Install headers
# -----------------------------------------------------------------------------
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

find_package(ftxui REQUIRED)
add_executable(fastgrind-report tools/fastgrind-report/main.cpp)
target_include_directories(fastgrind-report PRIVATE include)
target_compile_options(fastgrind-report PRIVATE -D__FAST_GRIND_STATUS=0 -g -O0)
target_link_options(fastgrind-report PRIVATE ${WRAP_FLAGS})
target_link_libraries(fastgrind-report
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE ftxui::component
)
