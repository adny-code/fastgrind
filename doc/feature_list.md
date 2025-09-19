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