file(GLOB BENCH_FASTGRIND_SOURCES *.cpp)

add_executable(benchmark_raw ${BENCH_FASTGRIND_SOURCES})
target_link_libraries(benchmark_raw PRIVATE pthread dl -lgcc_s)

add_executable(benchmark_fastgrind ${BENCH_FASTGRIND_SOURCES})
target_compile_definitions(benchmark_fastgrind PRIVATE FASTGRIND)
target_link_libraries(benchmark_fastgrind PRIVATE pthread dl -lgcc_s)
target_link_libraries(benchmark_fastgrind PRIVATE fastgrind_includes fastgrind_wrap_flags fastgrind_instrument)

add_executable(benchmark_valgrind ${BENCH_FASTGRIND_SOURCES})
target_compile_definitions(benchmark_fastgrind PRIVATE FASTGRIND)
target_link_libraries(benchmark_valgrind PRIVATE pthread dl -lgcc_s)
target_include_directories(benchmark_valgrind PRIVATE /usr/include/valgrind)
target_compile_definitions(benchmark_valgrind PRIVATE  USE_VALGRIND=1 VALGRIND_MONITOR=1)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/run_valgrind.sh 
               ${CMAKE_CURRENT_BINARY_DIR}/run_valgrind.sh 
               COPYONLY)
file(CHMOD ${CMAKE_CURRENT_BINARY_DIR}/run_valgrind.sh
     PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                 GROUP_READ GROUP_EXECUTE
                 WORLD_READ WORLD_EXECUTE)
