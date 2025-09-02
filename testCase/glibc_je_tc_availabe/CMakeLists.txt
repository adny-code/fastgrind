# CMakeLists for glibc / jemalloc / tcmalloc availability test executables

add_executable(testGLibcAvailable testGLibcAvailable.cpp)
target_link_libraries(testGLibcAvailable PRIVATE pthread dl merecorder_wrap merecorder_includes -Wl,-Bstatic -Wl,-Bdynamic -lgcc_s -Wl,--export-dynamic)
merecorder_apply_debug_options(testGLibcAvailable)

add_executable(testJEMallocAvailable testJEMallocAvailable.cpp)
target_link_libraries(testJEMallocAvailable PRIVATE pthread dl merecorder_wrap merecorder_includes -Wl,-Bstatic -ljemalloc -Wl,-Bdynamic -lgcc_s -Wl,--export-dynamic)
target_compile_definitions(testJEMallocAvailable PRIVATE MERECORDER_JE_MALLOC)
target_link_libraries(testJEMallocAvailable PRIVATE merecorder_instrument)

add_executable(testTCMallocAvailable testTCMallocAvailable.cpp)
target_link_libraries(testTCMallocAvailable PRIVATE pthread dl merecorder_wrap merecorder_includes -Wl,-Bstatic -ltcmalloc -Wl,-Bdynamic -lgcc_s -lunwind -Wl,--export-dynamic)
target_compile_definitions(testTCMallocAvailable PRIVATE MERECORDER_TC_MALLOC)
merecorder_apply_debug_options(testTCMallocAvailable)
