# Fastgrind

Fastgrind is a header-only memory profiler for C++ applications on Linux toolchains that support GNU ld `--wrap`. This release branch is trimmed for package consumption: it ships the runtime header, the installable CMake package, the Python trace viewer, and one installed-package CMake example.

## Release Contents

- `include/fastgrind.h`: header-only runtime
- `CMakeLists.txt`: installable CMake package export
- `tools/fastgrind.py`: trace inspection and UI tool
- `demo/cmake_installed_package`: minimal consumer example using `find_package`
- `doc/compile.md`: integration details for `fastgrind::manual` and `fastgrind::auto`

## Install

Install fastgrind into any prefix before using it from another CMake project:

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
cmake --install build --prefix "$HOME/.local"
```

This installs:

- `include/fastgrind.h`
- `lib/cmake/fastgrind/fastgrindConfig.cmake`
- `lib/cmake/fastgrind/fastgrindConfigVersion.cmake`
- `lib/cmake/fastgrind/fastgrindTargets.cmake`
- `bin/fastgrind.py`

## Use From Your CMake Project

Point CMake at the install prefix when configuring your own project:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/.local"
cmake --build build -j$(nproc)
```

Minimal consumer setup:

```cmake
find_package(Threads REQUIRED)
find_package(fastgrind CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE fastgrind::manual Threads::Threads)
# Or use fastgrind::auto for compiler-driven instrumentation.
```

The repository includes a working installed-package case in `demo/cmake_installed_package`.

## Instrumentation Modes

`fastgrind::manual`

- Adds the include path
- Adds the allocator wrap linker flags
- Use `__FASTGRIND__::FAST_GRIND;` inside the functions you want to track

`fastgrind::auto`

- Includes everything from `fastgrind::manual`
- Adds `-DFASTGRIND_INSTRUMENT`
- Adds `-finstrument-functions`
- Adds the exclude-file list used by the project
- Adds `-Wl,--export-dynamic`

Automatic mode still requires including `fastgrind.h` in at least one translation unit.

## Output Files

When an instrumented program exits, fastgrind writes these files to the current working directory:

- `fastgrind.text`: human-readable summary
- `fastgrind.fgb`: binary trace for the viewer

Use the viewer directly from the repository or from the installed prefix:

```bash
python tools/fastgrind.py ui fastgrind.fgb
python tools/fastgrind.py inspect fastgrind.fgb
python tools/fastgrind.py export-html fastgrind.fgb
```

If `bin` is on your `PATH`, the installed script can be invoked as:

```bash
fastgrind.py ui fastgrind.fgb
```

## Requirements

- Linux
- GCC or Clang
- GNU ld compatible `--wrap` support


## Contributing and Support

For questions, bug reports, or contributions, please contact us:
- **Email**: zfzmalloc@gmail.com
- **GitHub**: https://github.com/adny-code/fastgrind
- **Issues**: Report bugs and feature requests via GitHub Issues


## License

Fastgrind is released under the MIT License. See `LICENSE` for details.
