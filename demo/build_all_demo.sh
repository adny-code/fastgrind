#!/usr/bin/env bash
# Build all demos under demo/auto_instrument and demo/manual_instrument
# It detects build method per subfolder in this priority:
#   1) build.sh  2) CMakeLists.txt  3) Makefile/makefile
# Usage:
#   ./build_all_demo.sh [--jobs N]
#   JOBS can also be provided via env var. Default: nproc if available, else 1.

set -o pipefail

# ---- config ----
JOBS=${JOBS:-}
if [[ -z "${JOBS}" ]]; then
  if command -v nproc >/dev/null 2>&1; then
    JOBS=$(nproc)
  else
    JOBS=1
  fi
fi

BASE_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "${BASE_DIR}/.." && pwd)"

red()   { printf "\033[31m%s\033[0m\n" "$*"; }
yellow(){ printf "\033[33m%s\033[0m\n" "$*"; }
green() { printf "\033[32m%s\033[0m\n" "$*"; }

results_ok=()
results_fail=()

build_one() {
  local dir="$1"
  local name
  name="${dir#${ROOT_DIR}/}"

  echo
  yellow "===== Building: ${name} ====="
  (
    set -e
    cd "$dir"
    if [[ -f "build.sh" ]]; then
      yellow "-> Using build.sh"
      bash ./build.sh
    elif [[ -f "CMakeLists.txt" ]]; then
      yellow "-> Using CMake (mkdir -p build && cmake .. && make -j${JOBS})"
      mkdir -p build
      cd build
      cmake ..
      make -j"${JOBS}"
    elif [[ -f "Makefile" || -f "makefile" ]]; then
      yellow "-> Using Makefile (make -j${JOBS})"
      make -j"${JOBS}"
    else
      yellow "-> No known build entry (build.sh / CMakeLists.txt / Makefile). Skipping."
      exit 2
    fi
  ) && {
    green "✔ SUCCESS: ${name}"
    results_ok+=("${name}")
  } || {
    status=$?
    if [[ $status -eq 2 ]]; then
      yellow "⚠ SKIPPED: ${name}"
    else
      red "✖ FAILED (${status}): ${name}"
      results_fail+=("${name}")
    fi
  }
}

main() {
  local groups=("auto_instrument" "manual_instrument")
  # First pass: clean each subfolder's build directory to ensure full rebuilds
  for grp in "${groups[@]}"; do
    local parent="${ROOT_DIR}/demo/${grp}"
    if [[ ! -d "${parent}" ]]; then
      continue
    fi
    for d in "${parent}"/*; do
      [[ -d "$d" ]] || continue
      if [[ -e "$d/build" ]]; then
        yellow "[CLEAN] ${d#${ROOT_DIR}/}/build"
        rm -rf "$d/build"
      fi
    done
  done

  # Second pass: build each subfolder
  for grp in "${groups[@]}"; do
    local parent="${ROOT_DIR}/demo/${grp}"
    if [[ ! -d "${parent}" ]]; then
      continue
    fi
    for d in "${parent}"/*; do
      [[ -d "$d" ]] || continue
      build_one "$d"
    done
  done

  echo
  echo "===== Build Summary ====="
  echo "OK   ("${#results_ok[@]}"):"
  for x in "${results_ok[@]}"; do echo "  - $x"; done
  echo "FAIL ("${#results_fail[@]}"):"
  for x in "${results_fail[@]}"; do echo "  - $x"; done

  [[ ${#results_fail[@]} -eq 0 ]]
}

main "$@"
