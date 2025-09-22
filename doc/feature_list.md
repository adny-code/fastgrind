# Fastgrind Feature List

## Key Features
- 🔍 **Memory Allocation Tracking**: Monitor malloc/free, new/delete, and POSIX memory functions
- 📊 **Call Stack Analysis**: Capture and analyze function call chains with configurable depth (default: 64 frames)
- 🧵 **Thread-Safe Operation**: Per-thread local tracking with global aggregation
- ⏱️ **Time-Based Aggregation**: Memory usage statistics organized by time slices
- 🔧 **Multiple Allocator Support**: Compatible with glibc, jemalloc, and tcmalloc
- 📈 **Performance Benchmarking**: Compare overhead against Valgrind
- � **Modern C++ Support**: Full compatibility with C++11/14/17 features


## Fastgrind.h Library

### Core Functionality

The `fastgrind.h` header provides a single-file solution for memory profiling with the following key features:

#### **Memory Allocation Interception**

- Wraps standard allocation functions (`malloc`, `calloc`, `realloc`, `free`)
- Intercepts C++ operators (`new`, `new[]`, `delete`, `delete[]`, including nothrow variants)
- Supports POSIX memory functions (`posix_memalign`, `memalign`, `valloc`)
- Compatible with aligned memory allocation functions (C++17)

#### **Call Stack Management**

The library provides two distinct approaches for function instrumentation:

##### **Manual Instrumentation Call Stack Management**
- **RAII-style Probes**: Use `FAST_GRIND` macro for explicit stack frame tracking
- **Selective Instrumentation**: Developers manually place macros in the beginning of functions of interest
- **Explicit Control**: Fine-grained control over which functions appear in call stacks
- **Low Overhead**: Only tracks explicitly marked functions, minimizing performance impact
- **Simple Configuration**: No complex compiler flags or exclusion lists required

```cpp
void myFunction() {
    __FASTGRIND__::FAST_GRIND;  // Explicit call stack tracking
    // Function implementation
}
```

##### **Automatic Instrumentation Call Stack Management**  
- **Compiler-Driven Tracking**: Automatic insertion of instrumentation calls by compiler
- **Comprehensive Coverage**: All functions automatically included in call stack tracking
- **Symbol Resolution**: Automatic function name extraction from call addresses using runtime symbol lookup
- **Advanced Filtering**: System header exclusion lists prevent tracking unwanted functions
- **Complex Configuration**: Requires advanced compiler flags and exclusion patterns

```cpp
// With -DFASTGRIND_INSTRUMENT flag, all functions automatically tracked
void myFunction() {
    // No manual macro needed - automatically instrumented
}
```

##### **Common Features (Both Approaches)**
- **Configurable Depth**: Adjustable call stack capture depth (default: 64 frames)
- **Symbol Resolution**: Function name extraction from call addresses
- **Cross-Platform Support**: Works on various Unix-like systems

#### **Thread-Safe Architecture**
- **Per-Thread Local Storage**: Minimizes contention with thread-local accumulators
- **Global Aggregation**: Periodic merging into mutex-protected global container
- **Recursion Protection**: Thread-local guards prevent instrumentation recursion


### API Reference


### Configuration

- The default max callstack depth is 64. You can modify macro __MEM_MAX_STACK_DEPTH to increase this limition.

#### **Configuration Macros**

```cpp
// Not defined, if needed, define it in compile flags
#define FASTGRIND_INSTRUMENT          // Enable automatic instrumentation
#define FASTGRIND_JE_MALLOC           // Use jemalloc allocator
#define FASTGRIND_TC_MALLOC           // Use tcmalloc allocator

// Primary instrumentation macro
#define FAST_GRIND fastgrind __fg(__FUNCTION__)

// Defined, could be modified in fastgrind.h
#define FAST_GRIND_STATUS 1           // Enable/disable profiling globally
#define __MEM_MAX_STACK_DEPTH 64      // Default tracking stack depth
#define __MEM_SAMPLE_INTERVAL_MS 500  // Default time frame (ms)
```

### Scope of Application

Fastgrind is particularly well-suited for:

- **Performance-Critical Applications**: Minimal overhead design for production use
- **Memory Leak Detection**: Identify allocation/deallocation mismatches
- **Memory Usage Optimization**: Analyze allocation patterns and hotspots
- **Multi-threaded Applications**: Thread-safe tracking across concurrent operations
- **Large-Scale C++ Projects**: Multi-package compilation and linking support
- **Third-party Library Integration**: Non-intrusive instrumentation of external dependencies



## Supported Features and Compatibility

### C++ Language Features

Based on comprehensive testing in `testcase/cpp_feature_test/`:

#### **Fully Supported**
- ✅ **RAII and Smart Pointers**: `std::unique_ptr`, `std::shared_ptr`, custom RAII classes
- ✅ **STL Containers**: `std::vector`, `std::map`, `std::unordered_map`, etc.
- ✅ **Modern C++ (C++11/14/17)**: Auto, lambdas, range-based loops
- ✅ **Namespace Operations**: Multi-level namespaces, using declarations
- ✅ **Template Instantiation**: Class and function templates
- ✅ **Exception Handling**: Memory tracking across exception boundaries

#### **Limited Support**
- ⚠️ **Template Metaprogramming**: Complex template constructs may show generic names
- ⚠️ **Anonymous Functions**: Lambda expressions may appear as unnamed in reports

### Memory Allocator Compatibility

Validated through `testcase/glibc_je_tc_availabe/`:

#### **Standard C Library Functions**
- ✅ `malloc`, `calloc`, `realloc`, `free`
- ✅ `posix_memalign`, `memalign`, `valloc`
- ✅ Zero-size allocation handling
- ✅ NULL pointer safety

#### **C++ Operators**
- ✅ `operator new`, `operator new[]`
- ✅ `operator delete`, `operator delete[]`
- ✅ Nothrow variants (`std::nothrow`)
- ✅ Aligned allocation (C++17): `operator new`/`delete` with `std::align_val_t`

#### **Third-Party Allocators**
- ✅ **jemalloc**: Full compatibility with je_malloc family
- ✅ **tcmalloc**: Complete support for tc_malloc functions
- ✅ **glibc malloc**: Standard system allocator support

### Multi-Threading Support

- ✅ **Thread-Local Storage**: Per-thread memory tracking without contention
- ✅ **Concurrent Allocations**: Safe tracking across multiple threads
- ✅ **Cross-Thread Analysis**: Global aggregation of per-thread statistics
- ✅ **Thread Creation/Destruction**: Proper cleanup on thread exit

### Build System Integration

Demonstrated in `demo/` directory:

- ✅ **CMake**: Modern CMake (3.10+) with proper dependency management
- ✅ **GNU Make**: Traditional Makefile-based builds
- ✅ **Bash Scripts**: Simple script-based compilation
- ✅ **Multi-Package Projects**: Static library compilation and linking