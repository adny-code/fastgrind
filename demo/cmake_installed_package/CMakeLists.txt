cmake_minimum_required(VERSION 3.13)

project(fastgrind_installed_package_demo CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(fastgrind CONFIG REQUIRED)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE fastgrind::manual Threads::Threads)

# Switch to fastgrind::auto when you want compiler-driven instrumentation.
# Automatic mode still requires including fastgrind.h in one translation unit.