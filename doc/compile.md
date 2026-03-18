# Fastgrind Compile & Link Options

This is an introduction document about how to compile **fastgrind.h** in your project

## Recommended CMake Integration

If your project already uses CMake, prefer the exported interface targets instead of manually copying compiler and linker flags.

### Installed package

Build and install fastgrind before using `find_package`:

```bash
cmake -S . -B build -DFASTGRIND_BUILD_TESTS=OFF -DFASTGRIND_INSTALL=ON
cmake --build build -j$(nproc)
cmake --install build --prefix "$HOME/.local"
```

If you install to a non-standard prefix, configure the consumer project with `-DCMAKE_PREFIX_PATH=/path/to/prefix` so CMake can locate `fastgrindConfig.cmake`.

```cmake
find_package(fastgrind CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE fastgrind::manual)
# or fastgrind::auto
```

### Vendored package

```cmake
set(FASTGRIND_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FASTGRIND_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(external/fastgrind)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE fastgrind::auto)
```

### Target meanings

- `fastgrind::manual`: include path + allocation wrap flags
- `fastgrind::auto`: everything in `fastgrind::manual` plus `FASTGRIND_INSTRUMENT`, `-finstrument-functions`, exclude-file list, and `-Wl,--export-dynamic`
- Allocator selection still uses your own target compile definitions such as `FASTGRIND_JE_MALLOC` or `FASTGRIND_TC_MALLOC`

Automatic mode still requires including `fastgrind.h` in at least one translation unit.

## Manual Instrumentation

**Description**: Manual instrumentation requires developers to explicitly add `__FASTGRIND__::FAST_GRIND` in source code but offers simpler compilation configuration.

### Compile Options:
```bash
g++ -O3 -Wall -Wextra -std=c++11 \
    -I/path/to/fastgrind/include \
    source_files...
    # -DFASTGRIND_JE_MALLOC (if use jemalloc)
    # -DFASTGRIND_TC_MALLOC (if use tcmalloc)
```

### Link Options:
- **Wrap flags**: Symbol wrapping for memory allocators (Lists all supported below)

  ```bash
  WRAP_FLAGS=(
  # C standard library memory allocation functions
  -Wl,--wrap=malloc                 # Standard memory allocation
  -Wl,--wrap=calloc                 # Zero-initialized memory allocation
  -Wl,--wrap=realloc                # Memory reallocation
  -Wl,--wrap=free                   # Memory deallocation
  
  # C++ standard operator new/delete (basic versions)
  -Wl,--wrap=_Znwm                  # operator new(size_t)
  -Wl,--wrap=_Znam                  # operator new[](size_t)
  -Wl,--wrap=_ZdlPv                 # operator delete(void*)
  -Wl,--wrap=_ZdaPv                 # operator delete[](void*)
  
  # C++ nothrow operator new/delete
  -Wl,--wrap=_ZnwmRKSt9nothrow_t    # operator new(size_t, nothrow)
  -Wl,--wrap=_ZnamRKSt9nothrow_t    # operator new[](size_t, nothrow)
  -Wl,--wrap=_ZdlPvRKSt9nothrow_t   # operator delete(void*, nothrow)
  -Wl,--wrap=_ZdaPvRKSt9nothrow_t   # operator delete[](void*, nothrow)
  
  # POSIX and Linux-specific memory allocation functions
  -Wl,--wrap=valloc                 # Page-aligned memory allocation
  -Wl,--wrap=pvalloc                # Page-aligned allocation (multiple of page size)
  -Wl,--wrap=memalign               # Aligned memory allocation
  -Wl,--wrap=posix_memalign         # POSIX aligned memory allocation
  -Wl,--wrap=reallocarray           # Array reallocation with overflow check
  -Wl,--wrap=aligned_alloc          # C11 aligned allocation
  
  # C++ sized delete operators (C++14)
  -Wl,--wrap=_ZdaPvm                # operator delete[](void*, size_t)
  -Wl,--wrap=_ZdlPvm                # operator delete(void*, size_t)
  
  # C++ aligned allocation operators (C++17)
  -Wl,--wrap=_ZnwmSt11align_val_t   # operator new(size_t, align_val_t)
  -Wl,--wrap=_ZnamSt11align_val_t   # operator new[](size_t, align_val_t)
  -Wl,--wrap=_ZdlPvSt11align_val_t  # operator delete(void*, align_val_t)
  -Wl,--wrap=_ZdaPvSt11align_val_t  # operator delete[](void*, align_val_t)
  
  # C++ sized aligned delete operators (C++17)
  -Wl,--wrap=_ZdlPvmSt11align_val_t # operator delete(void*, size_t, align_val_t)
  -Wl,--wrap=_ZdaPvmSt11align_val_t # operator delete[](void*, size_t, align_val_t)
  
  # C++ nothrow sized delete operators
  -Wl,--wrap=_ZdlPvmRKSt9nothrow_t  # operator delete(void*, size_t, nothrow)
  -Wl,--wrap=_ZdaPvmRKSt9nothrow_t  # operator delete[](void*, size_t, nothrow)
  
  # C++ nothrow aligned allocation operators (C++17)
  -Wl,--wrap=_ZnwmSt11align_val_tRKSt9nothrow_t    # operator new(size_t, align_val_t, nothrow)
  -Wl,--wrap=_ZnamSt11align_val_tRKSt9nothrow_t    # operator new[](size_t, align_val_t, nothrow)
  -Wl,--wrap=_ZdlPvSt11align_val_tRKSt9nothrow_t   # operator delete(void*, align_val_t, nothrow)
  -Wl,--wrap=_ZdaPvSt11align_val_tRKSt9nothrow_t   # operator delete[](void*, align_val_t, nothrow)
  
  # C++ nothrow sized aligned delete operators
  -Wl,--wrap=_ZdlPvmSt11align_val_tRKSt9nothrow_t  # operator delete(void*, size_t, align_val_t, nothrow)
  -Wl,--wrap=_ZdaPvmSt11align_val_tRKSt9nothrow_t  # operator delete[](void*, size_t, align_val_t, nothrow)
  )
  ```

**Example**: [demo/manual_instrument/simple_demo/build.sh](../demo/manual_instrument/simple_demo/build.sh)


## Auto Instrumentation

**Description**: Automatic instrument functions outside the exclude file, but requires more complex compilation configuration.

### Compile Options:
```bash
EXCLUDE_FILE_LISTS=(
    /usr/include/
    /usr/lib/
    /usr/local/
    fastgrind.h
)
EXCLUDE_FILE_LISTS=$(IFS=,; echo "${EXCLUDE_FILE_LISTS[*]}")

INSTRUMENT_FLAGS=(
  -finstrument-functions
  -finstrument-functions-exclude-file-list=${EXCLUDE_FILE_LISTS}
)

# "${INSTRUMENT_FLAGS[@]}": exclude instrument lists
# -DFASTGRIND_INSTRUMENT:   define FASTGRIND_INSTRUMENT for auto instrument
# -Wl,--export-dynamic:     export symbol
g++ -O3 -Wall -Wextra -std=c++11 \
    "${INSTRUMENT_FLAGS[@]}" \
    -DFASTGRIND_INSTRUMENT \
    -Wl,--export-dynamic \
    -I/path/to/fastgrind/include \
    source_files...
    # -DFASTGRIND_JE_MALLOC (if use jemalloc)
    # -DFASTGRIND_TC_MALLOC (if use tcmalloc)
```

### Link Options:

Same as [manual instrumentation's link options](#link-options)

**Example**: [demo/auto_instrument/simple_demo/build.sh](../demo/auto_instrument/simple_demo/build.sh)

## Key Differences Summary

| Aspect | Manual Instrumentation | Automatic Instrumentation |
|--------|------------------------|----------------------------|
| **Code Changes** | Required (manual API calls) | Just include "fastgrind.h" |
| **Compilation Complexity** | Simple | Complex |
| **Compiler Flags** | Standard | Enhanced with defines |
| **Linker Options** | Basic wrap flags | Export-dynamic + wrap flags |
| **Exclusion Lists** | Not needed | Extensive system exclusions |
| **Setup Effort** | Low (simple build) | High (complex configuration) |
| **Runtime Overhead** | Lower (selective) | Higher (comprehensive) |
| **Maintenance** | Manual updates needed | Automatic coverage |
