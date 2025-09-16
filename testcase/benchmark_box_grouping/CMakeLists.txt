# Compile benchmark programs for raw, fastgrind, and valgrind execution

file(GLOB BENCH_FASTGRIND_SOURCES *.cpp)

add_executable(benchmark_raw ${BENCH_FASTGRIND_SOURCES})
target_link_libraries(benchmark_raw PRIVATE pthread dl -lgcc_s)

add_executable(benchmark_fastgrind ${BENCH_FASTGRIND_SOURCES})
target_compile_definitions(benchmark_fastgrind PRIVATE FASTGRIND)
target_link_libraries(benchmark_fastgrind PRIVATE pthread dl -lgcc_s)
target_link_libraries(benchmark_fastgrind PRIVATE fastgrind_includes fastgrind_wrap_flags fastgrind_instrument)

find_program(VALGRIND_EXECUTABLE valgrind)
find_path(VALGRIND_INCLUDE_DIR valgrind/valgrind.h
          PATHS /usr/include /usr/local/include)
if(VALGRIND_EXECUTABLE AND VALGRIND_INCLUDE_DIR)
    message(STATUS "Valgrind found: ${VALGRIND_EXECUTABLE}")
    message(STATUS "Valgrind headers found: ${VALGRIND_INCLUDE_DIR}")
    
    add_executable(benchmark_valgrind ${BENCH_FASTGRIND_SOURCES})
    target_compile_definitions(benchmark_valgrind PRIVATE)
    target_link_libraries(benchmark_valgrind PRIVATE pthread dl -lgcc_s)
    target_include_directories(benchmark_valgrind PRIVATE ${VALGRIND_INCLUDE_DIR})
    target_compile_definitions(benchmark_valgrind PRIVATE USE_VALGRIND=1 VALGRIND_MONITOR=1)
    
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/run_valgrind.sh 
                   ${CMAKE_CURRENT_BINARY_DIR}/run_valgrind.sh 
                   COPYONLY)
    file(CHMOD ${CMAKE_CURRENT_BINARY_DIR}/run_valgrind.sh
         PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                     GROUP_READ GROUP_EXECUTE
                     WORLD_READ WORLD_EXECUTE)
else()
    message(WARNING "Valgrind not found. benchmark_valgrind target will not be built.")
    if(NOT VALGRIND_EXECUTABLE)
        message(STATUS "Valgrind executable not found in PATH")
    endif()
    if(NOT VALGRIND_INCLUDE_DIR)
        message(STATUS "Valgrind headers not found in standard include paths")
    endif()
endif()
