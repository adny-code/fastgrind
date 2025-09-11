# CMakeLists for benchmark_raw

file(GLOB BENCH_FASTGRIND_SOURCES
    *.cpp
)

add_executable(benchmark_raw ${BENCH_FASTGRIND_SOURCES})

target_link_libraries(benchmark_raw
    PRIVATE pthread dl -Wl,-Bstatic -Wl,-Bdynamic -lgcc_s -Wl,--export-dynamic
)

fastgrind_apply_options(benchmark_raw)
