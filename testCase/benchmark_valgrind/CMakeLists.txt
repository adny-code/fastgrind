# CMakeLists for benchmark_valgrind

file(GLOB BENCH_FASTGRIND_SOURCES *.cpp)

add_executable(benchmark_valgrind ${BENCH_FASTGRIND_SOURCES})

target_link_libraries(benchmark_valgrind PRIVATE pthread dl -lgcc_s)
