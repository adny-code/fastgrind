# Fastgrind Test Cases

This directory contains several test cases for the **Fastgrind** memory profiler. Each test case validates different aspects of memory allocation tracking, instrumentation capabilities, and compatibility with various programming scenarios.

## Building Test Cases

All test cases are managed by the top-level CMake.

```bash
mkdir build
cd build
cmake ..
make
```


## Test Case Overview

### 1. benchmark_box_grouping/

**Purpose**: Performance benchmarking and memory allocation patterns testing.

**Description**: This test case implements a geometric algorithms that performs memory allocations and deallocations. It's designed to benchmark Fastgrind's overhead compared to raw execution and Valgrind.

**Key Features**:
- Single-threaded and multi-threaded(default 16 threads) memory allocation patterns
- Performance comparison between raw, Fastgrind-instrumented, and Valgrind execution
- Complex data structure operations with frequent memory operations

**Build Targets**:
- `benchmark_raw` - Baseline execution without instrumentation
- `benchmark_fastgrind` - Fastgrind-instrumented version
- `benchmark_valgrind` - Valgrind-compatible version (if Valgrind is available)

**Usage**:
```bash
cd build/testcase/benchmark_box_grouping
./benchmark_raw
./benchmark_fastgrind
./run_valgrind.sh
```

### 2. cpp_feature_test/

**Purpose**: Modern C++ feature compatibility validation.

**Description**: Comprehensive test suite verifying that Fastgrind correctly handles modern C++ language features, including RAII, smart pointers, templates, and namespace operations.

**Key Features**:
- Modern C++ feature testing (C++17 and beyond)
- Namespace and package dependency validation
- Memory allocation in complex C++ constructs
- Template instantiation and memory tracking

**Note**:
Weak support for template metaprogramming and anonymous functions in summary report

**Build Target**: `cpp_feature_test`

**Usage**:
```bash
cd build/testcase/cpp_feature_test
./cpp_feature_test
```

### 3. glibc_je_tc_availabe/

**Purpose**: Memory allocator compatibility testing.

**Description**: Tests Fastgrind's ability to intercept and track memory operations across different memory allocators commonly used in Linux systems.

**Key Features**:
- Standard C library memory functions (`malloc`, `free`, `realloc`, `calloc`)
- C++ new/delete operators (including nothrow variants)
- POSIX memory functions (`posix_memalign`, `memalign`, `valloc`)
- Multi-threaded allocation patterns
- Edge case testing (zero-size allocations, null pointer handling)

**Build Targets**:
- `testGLibcAvailable` - Standard glibc malloc/free testing
- `testJEMallocAvailable` - jemalloc compatibility testing  
- `testTCMallocAvailable` - tcmalloc compatibility testing

**Usage**:
```bash
cd build/testcase/glibc_je_tc_availabe
./testGLibcAvailable
./testJEMallocAvailable
./testTCMallocAvailable
```

### 4. multi_pkg_compile/

**Purpose**: Multi-package compilation and linking validation.

**Description**: Tests Fastgrind's instrumentation capabilities across multiple compilation units and static libraries, ensuring proper symbol resolution and memory tracking in complex build scenarios.

**Key Features**:
- Multiple static library compilation (`libpkgA.a`, `libpkgB.a`, `libpkgC.a`)
- Cross-package memory allocation tracking
- Multi-threaded execution across different packages
- Symbol resolution testing across compilation boundaries

**Package Structure**:
- `pkgA/` - Package A implementation
- `pkgB/` - Package B implementation  
- `pkgC/` - Package C implementation

**Build Target**: `multi_pkg_main`

**Usage**:
```bash
cd build/testcase/multi_pkg_compile
./multi_pkg_main
```

### 5. thirdparty_leveldb_test/

**Purpose**: Third-party library integration testing.

**Description**: Contains test results for LevelDB integration, demonstrating Fastgrind's ability to track memory usage in third-party applications.

**Contents**:
- `fastgrind.json` - JSON format memory tracking results
- `fastgrind.text` - Human-readable memory tracking report


### 6. thridparty_zlib_test/

**Purpose**: Third-party compression library testing.

**Description**: Contains test results for zlib integration, validating memory tracking in in third-party applications.

**Contents**:
- `fastgrind.json` - JSON format memory tracking results
- `fastgrind.text` - Human-readable memory tracking report


## Output Files

When Fastgrind-instrumented tests are executed, they generate:

- `fastgrind.json` - JSON format containing detailed memory allocation tracking (per time step, per thread, per function)
- `fastgrind.text` - Perf-like report with memory usage summary


## Benchmarking and Validation

The `testcase/` directory contains comprehensive validation suites:

### Benchmark
- **Raw Execution**: Baseline performance without instrumentation
- **Fastgrind Execution**: Measure instrumentation overhead
- **Valgrind Comparison**: Performance comparison with Valgrind

```bash
cd build/testcase/benchmark_box_grouping
./benchmark_raw          # Baseline
./benchmark_fastgrind    # With Fastgrind
./run_valgrind.sh        # Valgrind comparison
```



### Feature Validation

- **Modern C++ Features** (`cpp_feature_test/`)

- **Allocator Compatibility** (`glibc_je_tc_availabe/`)  

- **Multi-Package Compilation** (`multi_pkg_compile/`)

- **Third-Party Integration** (`thirdparty_*/`)

  

### Third-party Test
- **leveldb** (`thirdparty_leveldb_test`)

- **zlib** (`thirdparty_zlib_test`)
