cmake_minimum_required(VERSION 3.13)

project(fastgrind VERSION 1.0 LANGUAGES CXX)

# -----------------------------------------------------------------------------
# Global configuration
# -----------------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(FASTGRIND_TOP_LEVEL ON)
else()
    set(FASTGRIND_TOP_LEVEL OFF)
endif()

option(FASTGRIND_BUILD_TESTS "Build fastgrind testcase targets" ${FASTGRIND_TOP_LEVEL})
option(FASTGRIND_INSTALL "Install fastgrind headers and CMake package files" ${FASTGRIND_TOP_LEVEL})

if(FASTGRIND_BUILD_TESTS AND NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

# Public include directory for all targets in this project
add_library(fastgrind_includes INTERFACE)
set_target_properties(fastgrind_includes PROPERTIES EXPORT_NAME includes)
target_include_directories(fastgrind_includes INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
add_library(fastgrind::includes ALIAS fastgrind_includes)

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
set_target_properties(fastgrind_wrap_flags PROPERTIES EXPORT_NAME wrap_flags)
target_link_options(fastgrind_wrap_flags INTERFACE ${WRAP_FLAGS})
add_library(fastgrind::wrap_flags ALIAS fastgrind_wrap_flags)

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
set_target_properties(fastgrind_instrument PROPERTIES EXPORT_NAME instrument)
target_compile_options(fastgrind_instrument INTERFACE ${fastgrind_instrument_compile_opts})
target_link_options(fastgrind_instrument INTERFACE -Wl,--export-dynamic)
add_library(fastgrind::instrument ALIAS fastgrind_instrument)

# -----------------------------------------------------------------------------
# Consumer-facing convenience targets
# -----------------------------------------------------------------------------
add_library(fastgrind_manual INTERFACE)
set_target_properties(fastgrind_manual PROPERTIES EXPORT_NAME manual)
target_link_libraries(fastgrind_manual INTERFACE fastgrind_includes fastgrind_wrap_flags)
add_library(fastgrind::manual ALIAS fastgrind_manual)

add_library(fastgrind_auto INTERFACE)
set_target_properties(fastgrind_auto PROPERTIES EXPORT_NAME auto)
target_link_libraries(fastgrind_auto INTERFACE fastgrind_manual fastgrind_instrument)
add_library(fastgrind::auto ALIAS fastgrind_auto)

# -----------------------------------------------------------------------------
# Subdirectories
# -----------------------------------------------------------------------------
if(FASTGRIND_BUILD_TESTS)
    add_subdirectory(testcase/glibc_je_tc_availabe)
    add_subdirectory(testcase/multi_pkg_compile)
    add_subdirectory(testcase/benchmark_box_grouping)
    add_subdirectory(testcase/cpp_feature_test)
endif()

# -----------------------------------------------------------------------------
# Install headers and package metadata
# -----------------------------------------------------------------------------
if(FASTGRIND_INSTALL)
    set(fastgrind_cmake_install_dir "${CMAKE_INSTALL_LIBDIR}/cmake/fastgrind")

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fastgrindConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/fastgrindConfig.cmake
        INSTALL_DESTINATION ${fastgrind_cmake_install_dir}
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/fastgrindConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(TARGETS
        fastgrind_includes
        fastgrind_wrap_flags
        fastgrind_instrument
        fastgrind_manual
        fastgrind_auto
        EXPORT fastgrindTargets
    )

    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(EXPORT fastgrindTargets
        FILE fastgrindTargets.cmake
        NAMESPACE fastgrind::
        DESTINATION ${fastgrind_cmake_install_dir}
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/fastgrindConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/fastgrindConfigVersion.cmake
        DESTINATION ${fastgrind_cmake_install_dir}
    )
endif()
