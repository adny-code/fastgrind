# CMakeLists for benchmark_merecorder

file(GLOB BENCH_MERECORDER_SOURCES
    *.cpp
)

add_executable(benchmark_merecorder ${BENCH_MERECORDER_SOURCES})

target_link_libraries(benchmark_merecorder
    PRIVATE pthread dl -Wl,-Bstatic -Wl,-Bdynamic -lgcc_s -Wl,--export-dynamic
)

merecorder_apply_options(benchmark_merecorder)
