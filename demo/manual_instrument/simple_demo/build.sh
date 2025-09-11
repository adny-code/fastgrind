#!/usr/bin/env bash
# Simple example: compile simple_demo/main.cpp and enable allocation wrapping for fastgrind.
# Usage: run ./build.sh to generate build/app
# Overridable via environment variables: CXX, CXXSTD, CXXFLAGS

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

CXX=${CXX:-g++}
CXXSTD=${CXXSTD:-c++11}
CXXFLAGS=${CXXFLAGS:--O3 -g -Wall -Wextra -std=${CXXSTD}}
INCLUDE_FLAGS=( -I"${REPO_ROOT}/include" -I"${SCRIPT_DIR}" )

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

echo "[CLEAN] ${BUILD_DIR}"
rm -rf "${BUILD_DIR}" && mkdir -p "${BUILD_DIR}"

echo "[BUILD] main.cpp -> main.o"
${CXX} ${CXXFLAGS} "${INCLUDE_FLAGS[@]}" -c "${SCRIPT_DIR}/main.cpp" -o "${BUILD_DIR}/main.o"

echo "[LINK ] app"
${CXX} -pthread -o "${BUILD_DIR}/app" "${BUILD_DIR}/main.o" "${WRAP_FLAGS[@]}"

echo "[DONE ] Executable: ${BUILD_DIR}/app"
echo "Run: ${BUILD_DIR}/app (a fastgrind.json will be generated on exit)"
