# Fastgrind Demo Examples

This directory contains comprehensive demonstration examples for the **Fastgrind** memory profiler. Each demo showcases different instrumentation approaches and build system integrations, helping users understand how to integrate Fastgrind into their projects.

## Demo Overview

The demos are organized into two main categories based on instrumentation approach:

- **`manual_instrument/`** - Manual instrumentation examples with simpler compilation options
- **`auto_instrument/`** - Automatic instrumentation examples with more complex compilation options

Each subdirectory contains independent compilation examples using different build systems (bash, CMake, Makefile).

## Quick Start

Build all demos at once:
```bash
./build_all_demo.sh
```

Or navigate to individual demo directories and build them separately.

## Instrumentation Approaches

### Manual Instrumentation (`manual_instrument/`)

**Description**: Manual instrumentation requires developers to explicitly add Fastgrind function calls in source code but offers simpler compilation configuration.

**Key Characteristics**:
- **Simpler compilation**: Minimal compiler flags and linker options
- **Manual code changes**: Requires adding Fastgrind API calls in source code
- **Direct control**: Fine-grained control over what gets instrumented
- **Lower complexity**: Straightforward build configuration

**Compilation Options**:
- **Compiler flags**: Basic options

```bash
g++ -O3 -Wall -Wextra -std=c++11 \
    -I/path/to/fastgrind/include \
    source_files...
    # -DFASTGRIND_JE_MALLOC (if use jemalloc)
    # -DFASTGRIND_TC_MALLOC (if use tcmalloc)
```

**Linker Options**:
- **Wrap flags**: Symbol wrapping for memory allocators (Lists all supported below)

  ```bash
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
  ```

### Automatic Instrumentation (`auto_instrument/`)

**Description**: Automatic instrumentation leverages compiler-based instrumentation to automatically insert Fastgrind calls without manual code modification, but requires more complex compilation configuration.

**Key Characteristics**:
- **Complex compilation**: Advanced compiler flags and instrumentation options
- **No code changes**: Automatic insertion of instrumentation calls
- **Compiler-driven**: Uses compiler instrumentation capabilities
- **Higher complexity**: Sophisticated build configuration with exclusion lists

**Compilation Options**:
- **Compiler flags**: Enhanced flags with instrumentation

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

g++ -O3 -Wall -Wextra -std=c++11 \
    ${INSTRUMENT_FLAGS[@]} \            # exclude instrument lists
    -DFASTGRIND_INSTRUMENT \            # define FASTGRIND_INSTRUMENT for auto instrument
    -Wl,--export-dynamic \              # export symbol
    -I/path/to/fastgrind/include \
    source_files...
    # -DFASTGRIND_JE_MALLOC (if use jemalloc)
    # -DFASTGRIND_TC_MALLOC (if use tcmalloc)
```

## Build System Examples

Each instrumentation approach includes examples for three build systems:

### 1. `simple_demo/`

**Purpose**: Minimal single-file demonstration.

**Files**:
- `main.cpp` - Simple memory allocation example
- `build.sh` - Bash build script

**Usage**:
```bash
cd auto_instrument/simple_demo    # or manual_instrument/simple_demo
./build.sh
./build/app
```

### 2. `compile_with_bash/`

**Purpose**: Multi-package bash script compilation.

**Description**: Demonstrates building multiple static libraries and linking them together using pure bash scripts.

**Package Structure**:
- `pkgA/` - Base package with fundamental functionality
- `pkgB/` - Intermediate package depending on pkgA
- `pkgC/` - Advanced package depending on pkgB

**Files**:
- `main.cpp` - Main application entry point
- `build.sh` - Comprehensive bash build script
- `pkgA/`, `pkgB/`, `pkgC/` - Individual package directories

**Usage**:
```bash
cd auto_instrument/compile_with_bash    # or manual_instrument/compile_with_bash
./build.sh
./build/app
```

### 3. `compile_with_cmake/`

**Purpose**: CMake-based build system integration.

**Description**: Shows how to integrate Fastgrind instrumentation into CMake build systems with proper dependency management and configuration.

**Key Features**:
- Modern CMake practices (3.10+)
- Static library compilation
- Transitive dependency handling
- Conditional compilation flags

**Files**:
- `CMakeLists.txt` - CMake configuration
- `main.cpp` - Main application
- `pkgA/`, `pkgB/`, `pkgC/` - Package directories

**Usage**:
```bash
cd auto_instrument/compile_with_cmake    # or manual_instrument/compile_with_cmake
mkdir build && cd build
cmake ..
make
./app
```

### 4. `compile_with_makefile/`

**Purpose**: GNU Make-based build system integration.

**Description**: Traditional Makefile approach for projects that prefer GNU Make over CMake.

**Files**:
- `Makefile` - GNU Make configuration
- `main.cpp` - Main application
- `pkgA/`, `pkgB/`, `pkgC/` - Package directories

**Usage**:
```bash
cd auto_instrument/compile_with_makefile    # or manual_instrument/compile_with_makefile
make
./app
```

## Key Differences Summary

| Aspect | Manual Instrumentation | Automatic Instrumentation |
|--------|------------------------|----------------------------|
| **Code Changes** | Required (manual API calls) | None (compiler-driven) |
| **Compilation Complexity** | Simple | Complex |
| **Compiler Flags** | Standard | Enhanced with defines |
| **Linker Options** | Basic wrap flags | Export-dynamic + wrap flags |
| **Exclusion Lists** | Not needed | Extensive system exclusions |
| **Setup Effort** | Low (simple build) | High (complex configuration) |
| **Runtime Overhead** | Lower (selective) | Higher (comprehensive) |
| **Maintenance** | Manual updates needed | Automatic coverage |

## Output Files

When Fastgrind-instrumented demos are executed, they generate:

- `fastgrind.json` - JSON format containing detailed memory allocation tracking (per time step, per thread, per function)
- `fastgrind.text` - Perf-like report with memory usage summary

## Getting Started

1. **For beginners**: Start with `manual_instrument/simple_demo/` to understand basic concepts
2. **For quick setup**: Use `manual_instrument/` examples for minimal configuration overhead
3. **For comprehensive coverage**: Use `auto_instrument/` examples for complete automatic instrumentation
4. **For build system integration**: Choose the appropriate `compile_with_*` example matching your build system

## Troubleshooting

- **Linking errors**: Ensure all wrap flags are properly specified
- **Missing symbols**: Check that `-Wl,--export-dynamic` is used in automatic instrumentation
- **System header conflicts**: Verify exclusion lists in automatic instrumentation
- **Compilation failures**: Check that `FASTGRIND_INSTRUMENT` is defined for automatic mode
- **TCMalloc/JEMalloc conflicts**： Add `-DFASTGRIND_TC_MALLOC` or `-DFASTGRIND_JE_MALLOC` flags



## Demonstrations and Examples

The `demo/` directory provides comprehensive examples for both instrumentation approaches:

### Available Demos

#### **Simple Examples**
- `manual_instrument/simple_demo/` - Basic single-file manual instrumentation
- `auto_instrument/simple_demo/` - Basic single-file automatic instrumentation

#### **Build System Examples**
- `compile_with_bash/` - Bash script-based multi-package compilation
- `compile_with_cmake/` - CMake-based modern build configuration  
- `compile_with_makefile/` - GNU Make traditional build approach

### Quick Demo Execution

```bash
# Build all demos
./demo/build_all_demo.sh

# Or run individual demos
cd demo/manual_instrument/simple_demo
./build.sh && ./build/app

cd demo/auto_instrument/compile_with_cmake
mkdir build && cd build && cmake .. && make && ./app
```