# CMakeLists for benchmark_valgrind

file(GLOB BENCH_FASTGRIND_SOURCES *.cpp)

add_executable(benchmark_valgrind ${BENCH_FASTGRIND_SOURCES})

target_link_libraries(benchmark_valgrind PRIVATE pthread dl -lgcc_s)

target_include_directories(benchmark_valgrind PRIVATE /usr/include/valgrind)

target_compile_definitions(benchmark_valgrind PRIVATE 
    USE_VALGRIND=1
    VALGRIND_MONITOR=1
)

# Copy run_valgrind.sh script to binary output directory
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/run_valgrind.sh 
               ${CMAKE_CURRENT_BINARY_DIR}/run_valgrind.sh 
               COPYONLY)
