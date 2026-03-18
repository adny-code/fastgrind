# Fastgrind CMake Integration

This release branch is centered on the installed CMake package. The recommended workflow is:

1. Install fastgrind to a prefix.
2. Use `find_package(fastgrind CONFIG REQUIRED)` in the consumer project.
3. Link either `fastgrind::manual` or `fastgrind::auto`.

## Install Fastgrind

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
cmake --install build --prefix "$HOME/.local"
```

If the prefix is not part of your default CMake search path, pass it when configuring your own project:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/.local"
```

## Consumer Example

The repository ships a minimal installed-package case in `demo/cmake_installed_package`.

Its `CMakeLists.txt` uses this pattern:

```cmake
find_package(Threads REQUIRED)
find_package(fastgrind CONFIG REQUIRED)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE fastgrind::manual Threads::Threads)
```

To validate the installed package end to end:

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
cmake --install build --prefix "$PWD/stage"

cmake -S demo/cmake_installed_package -B demo/cmake_installed_package/build \
  -DCMAKE_PREFIX_PATH="$PWD/stage"
cmake --build demo/cmake_installed_package/build -j$(nproc)
./demo/cmake_installed_package/build/app
```

## Exported Targets

`fastgrind::manual`

- Adds the fastgrind header include path
- Adds the allocator wrap linker flags
- Intended for explicit function-scope tracking with `__FASTGRIND__::FAST_GRIND;`

`fastgrind::auto`

- Includes everything from `fastgrind::manual`
- Adds `-DFASTGRIND_INSTRUMENT`
- Adds `-finstrument-functions`
- Adds the repository exclude-file list
- Adds `-Wl,--export-dynamic`

Automatic mode still requires including `fastgrind.h` in at least one translation unit.

The package also exports these lower-level interface targets when you need finer control:

- `fastgrind::includes`
- `fastgrind::wrap_flags`
- `fastgrind::instrument`

## Allocator Selection

Allocator-specific behavior is still controlled from the consumer target with compile definitions such as:

- `FASTGRIND_JE_MALLOC`
- `FASTGRIND_TC_MALLOC`

Example:

```cmake
target_compile_definitions(my_app PRIVATE FASTGRIND_TC_MALLOC)
target_link_libraries(my_app PRIVATE fastgrind::manual)
```

## Platform Notes

- Fastgrind relies on GNU ld compatible `--wrap` support.
- The installed-package workflow is intended for Linux builds with GCC or Clang.
- Output files overwrite previous `fastgrind.fgb` and `fastgrind.text` files in the current working directory.
