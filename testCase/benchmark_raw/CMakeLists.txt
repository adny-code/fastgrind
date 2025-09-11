# CMakeLists for benchmark_raw

file(GLOB BENCH_FASTGRIND_SOURCES *.cpp)

add_executable(benchmark_raw ${BENCH_FASTGRIND_SOURCES})

target_link_libraries(benchmark_raw PRIVATE pthread dl -lgcc_s)
