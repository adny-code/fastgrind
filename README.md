# mmPool - Memory Pool & Profiling Tool

## Overview
A lightweight memory pool built on jemalloc with memory allocation tracking and call stack analysis capabilities.

## Features
- ✅ Memory allocation/deallocation monitoring
- 📊 Call stack tracing (default depth=64)
- 🧵 Thread-safe operation
- ⏱️ Time-based memory usage aggregation
- 🔍 Leak detection support

## Quick Start

### Build & Install
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
sudo make install  # Optional system-wide installation
```

## Basic Usage
```c++
#include "mem_probe.h"

void example() {
    MEM_PROBE;  // Enable call stack tracking
    int* arr = new int[100];  // Tracked allocation
    delete[] arr;             // Tracked deallocation
}
```


## API Reference
### Key Classes
- memGlobalInfo: Singleton for global memory stats
- memLocalInfo	Thread-local memory tracking
- memStack	Call stack management
- memProbe	RAII-style stack probe

## Configuration Macros
```c++
#define MEM_PROBE_STATUS 1    // Enable/disable profiling
#define JE_MALLOC          // Use jemalloc (default)
```

## Sample Output
```bash
Func Memory Info
0: threadId:28765 alloc 4096 free 2048
  main
  processData
  operator new

Callstack Info
0: frame:0x45a2b1
  main
  processData
  operator new
```

## Advanced Options
### CMake Variables
```bash
set(CMAKE_CXX_STANDARD 17)    # C++ standard
set(WRAP_FLAGS -Wl,--wrap=malloc)  # Function 
```

### Linker Options
```bash
-ljemalloc -lpthread -ldl
```

## Testing
```bash
./test1  # Basic functionality
./test2  # Threading tests
```

## License
- MIT License