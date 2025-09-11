cmake_minimum_required(VERSION 3.13)

project(mmPool VERSION 1.0 LANGUAGES CXX)

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
# -----------------------------------------------------------------------------
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
    # -Wl,--wrap=sbrk
    # -Wl,--wrap=brk
    # -Wl,--wrap=mmap
    # -Wl,--wrap=munmap
    # -Wl,--wrap=mremap
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

add_library(fastgrind_wrap_flags INTERFACE)
target_link_options(fastgrind_wrap_flags INTERFACE ${WRAP_FLAGS})

# -----------------------------------------------------------------------------
# Instrumentation exclusion list & helper interface
# -----------------------------------------------------------------------------
set(INSTRUMENT_EXCLUDE_FILES
    # Must be excluded
    /usr/include/c++/
    /usr/include/x86_64-linux-gnu/c++/
    /usr/lib/gcc/
    /usr/include/x86_64-linux-gnu/
    /usr/include/linux/
    /usr/include/asm
    /usr/include/asm-generic
    /usr/include/sys/
    /usr/include/bits/
    /usr/include/gnu/
    /usr/include/glib-2.0/
    /usr/lib/x86_64-linux-gnu/glib-2.0/include/
    /usr/include/c++/v1
    /usr/lib/clang/
    /usr/local/include/
    # Third-Party, should be excluded
    fastGrind.h
    /usr/include/boost/
    /usr/include/eigen3/
    /usr/include/openssl/
    /usr/include/libunwind/
    /usr/include/jemalloc/
    /usr/include/gperftools/
    /usr/include/google/
    /usr/include/valgrind/
    /usr/include/cuda/
    /usr/local/cuda/include/
    /opt/local/include/
    ${CMAKE_SOURCE_DIR}/third_party/
    ${CMAKE_SOURCE_DIR}/extern/
    ${CMAKE_SOURCE_DIR}/vendor/
    ${CMAKE_SOURCE_DIR}/testCase/multi_pkg_compile/pkgB/
)
list(JOIN INSTRUMENT_EXCLUDE_FILES "," INSTRUMENT_EXCLUDE_FILES_JOINED)

# Define instrumentation compile options variable (shared usage)
set(fastgrind_base_compile_opts -O3)

set(fastgrind_instrument_compile_opts ${fastgrind_base_compile_opts} -DFASTGRIND_INSTRUMENT)
list(APPEND fastgrind_instrument_compile_opts -finstrument-functions -finstrument-functions-exclude-file-list=${INSTRUMENT_EXCLUDE_FILES_JOINED})

add_library(fastgrind_instrument INTERFACE)
target_compile_options(fastgrind_instrument INTERFACE ${fastgrind_instrument_compile_opts})
target_link_options(fastgrind_instrument INTERFACE -Wl,--export-dynamic)

# -----------------------------------------------------------------------------
# Subdirectories
# -----------------------------------------------------------------------------
add_subdirectory(testCase/glibc_je_tc_availabe)
add_subdirectory(testCase/multi_pkg_compile)
add_subdirectory(testCase/benchmark_fastgrind)
add_subdirectory(testCase/benchmark_raw)
add_subdirectory(testCase/benchmark_valgrind)

# -----------------------------------------------------------------------------
# Install headers
# -----------------------------------------------------------------------------
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
