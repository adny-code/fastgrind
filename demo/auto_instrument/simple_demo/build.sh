#!/usr/bin/env bash
# Simple example: compile simple_demo/main.cpp and enable allocation wrapping for fastGrind.
# Usage: run ./build.sh to generate build/app
# Overridable via environment variables: CXX, CXXSTD, CXXFLAGS

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

CXX=${CXX:-g++}
CXXSTD=${CXXSTD:-c++11}
CXXFLAGS=${CXXFLAGS:--O3 -g -Wall -Wextra -std=${CXXSTD} -DFASTGRIND_INSTRUMENT}
INCLUDE_FLAGS=( -I"${REPO_ROOT}/include" -I"${SCRIPT_DIR}" )
LINK_FLAGS=( -Wl,--export-dynamic )

WRAP_FLAGS=(
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

EXCLUDE_FILE_LISTS=(
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
    /usr/include/tcmalloc/
    /usr/include/gperftools/
    /usr/include/google/
    /usr/include/valgrind/
    /usr/include/cuda/
    /usr/local/cuda/include/
    /opt/local/include/
    ${REPO_ROOT}/third_party/
    ${REPO_ROOT}/extern/
    ${REPO_ROOT}/vendor/
    ${REPO_ROOT}/include/
)
EXCLUDE_FILE_LISTS=$(IFS=,; echo "${EXCLUDE_FILE_LISTS[*]}")

INSTRUMENT_FLAGS=(
  -finstrument-functions
  -finstrument-functions-exclude-file-list=${EXCLUDE_FILE_LISTS}
)

echo "[CLEAN] ${BUILD_DIR}"
rm -rf "${BUILD_DIR}" && mkdir -p "${BUILD_DIR}"

echo "[BUILD] main.cpp -> main.o"
${CXX} ${CXXFLAGS} "${INCLUDE_FLAGS[@]}" "${INSTRUMENT_FLAGS[@]}" -c "${SCRIPT_DIR}/main.cpp" -o "${BUILD_DIR}/main.o"

echo "[LINK ] app"
${CXX} -pthread -o "${BUILD_DIR}/app" "${BUILD_DIR}/main.o" "${WRAP_FLAGS[@]}" "${LINK_FLAGS[@]}"

echo "[DONE ] Executable: ${BUILD_DIR}/app"
echo "Run: ${BUILD_DIR}/app (a fastgrind.json will be generated on exit)"
