# CMakeLists for benchmark_fastgrind

file(GLOB BENCH_FASTGRIND_SOURCES *.cpp)

add_executable(benchmark_fastgrind ${BENCH_FASTGRIND_SOURCES})

target_link_libraries(benchmark_fastgrind PRIVATE pthread dl -lgcc_s)
