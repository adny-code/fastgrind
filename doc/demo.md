# Fastgrind Demo Examples

This is an introduction document about the **demo/**

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

## Output Files

When Fastgrind-instrumented demos are executed, they generate:

- `fastgrind.json` - JSON format containing detailed memory allocation tracking (per time step, per thread, per function)
- `fastgrind.text` - Perf-like report with memory usage summary

## Getting Started

1. **For beginners**: Start with `manual_instrument/simple_demo/` to understand basic concepts
2. **For quick setup**: Use `manual_instrument/` examples for minimal configuration overhead
3. **For comprehensive coverage**: Use `auto_instrument/` examples for complete automatic instrumentation
4. **For build system integration**: Choose the appropriate `compile_with_*` example matching your build system