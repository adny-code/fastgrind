#!/usr/bin/env bash
# Simple build script: always rebuild pkgA / pkgB / pkgC static libs and final executable.
# No arguments accepted. Running this script performs a clean rebuild.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Repository root: compile_with_bash -> manual_instrument -> demo -> (repo root)
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

CXX=${CXX:-g++}
CXXSTD=${CXXSTD:-c++11}
CXXFLAGS=${CXXFLAGS:--O3 -g -Wall -Wextra -std=${CXXSTD} -DFASTGRIND_INSTRUMENT}
INCLUDE_FLAGS=( -I"$REPO_ROOT/include" -I"$SCRIPT_DIR" )
LINK_FLAGS=( -Wl,--export-dynamic )
AR=${AR:-ar}
ARFLAGS=${ARFLAGS:-rcs}

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
    fastgrind.h
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

echo "[CLEAN] Removing previous build directory"
rm -rf "$BUILD_DIR"

compile_pkgA() {
  echo "[BUILD] pkgA -> libpkgA.a"
  local src="$SCRIPT_DIR/pkgA/PackageA.cpp"
  local obj="$BUILD_DIR/pkgA/PackageA.o"
  mkdir -p "$(dirname "$obj")"
  $CXX ${CXXFLAGS} "${INCLUDE_FLAGS[@]}" -I"$SCRIPT_DIR/pkgA" -c "$src" -o "$obj" "${INSTRUMENT_FLAGS[@]}"
  $AR ${ARFLAGS} "$BUILD_DIR/libpkgA.a" "$obj"
}

compile_pkgB() {
  echo "[BUILD] pkgB -> libpkgB.a"
  local src="$SCRIPT_DIR/pkgB/PackageB.cpp"
  local obj="$BUILD_DIR/pkgB/PackageB.o"
  mkdir -p "$(dirname "$obj")"
  # Need both pkgA and pkgB header include paths
  $CXX ${CXXFLAGS} "${INCLUDE_FLAGS[@]}" -I"$SCRIPT_DIR/pkgA" -I"$SCRIPT_DIR/pkgB" -c "$src" -o "$obj" "${INSTRUMENT_FLAGS[@]}"
  $AR ${ARFLAGS} "$BUILD_DIR/libpkgB.a" "$obj"
}

compile_pkgC() {
  echo "[BUILD] pkgC -> libpkgC.a"
  local src="$SCRIPT_DIR/pkgC/PackageC.cpp"
  local obj="$BUILD_DIR/pkgC/PackageC.o"
  mkdir -p "$(dirname "$obj")"
  # Need parent directory (SCRIPT_DIR) to allow #include "pkgA/PackageA.h" form
  $CXX ${CXXFLAGS} "${INCLUDE_FLAGS[@]}" -I"$SCRIPT_DIR/pkgC" -c "$src" -o "$obj" "${INSTRUMENT_FLAGS[@]}"
  $AR ${ARFLAGS} "$BUILD_DIR/libpkgC.a" "$obj"
}

compile_main() {
  echo "[BUILD] main.o"
  local src="$SCRIPT_DIR/main.cpp"
  local obj="$BUILD_DIR/main.o"
  mkdir -p "$BUILD_DIR"
  $CXX ${CXXFLAGS} "${INCLUDE_FLAGS[@]}" -c "$src" -o "$obj" "${INSTRUMENT_FLAGS[@]}"
}

link_all() {
  echo "[LINK ] app"
  $CXX -pthread -o "$BUILD_DIR/app" \
    "$BUILD_DIR/main.o" \
    "$BUILD_DIR/libpkgC.a" \
    "$BUILD_DIR/libpkgB.a" \
    "$BUILD_DIR/libpkgA.a" \
    "${WRAP_FLAGS[@]}" "${LINK_FLAGS[@]}"
  echo "[DONE ] Executable: $BUILD_DIR/app"
}

mkdir -p "$BUILD_DIR"
compile_pkgA
compile_pkgB
compile_pkgC
compile_main
link_all

echo "[SUCCESS] Build completed. Run $BUILD_DIR/app"
