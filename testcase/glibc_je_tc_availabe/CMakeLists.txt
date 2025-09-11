# CMakeLists for glibc / jemalloc / tcmalloc availability test executables

add_executable(testGLibcAvailable testGLibcAvailable.cpp)
target_link_libraries(testGLibcAvailable PRIVATE pthread dl fastgrind_wrap_flags fastgrind_includes)

add_executable(testJEMallocAvailable testJEMallocAvailable.cpp)
target_link_libraries(testJEMallocAvailable PRIVATE pthread dl fastgrind_wrap_flags fastgrind_includes -lgcc_s -Wl,-Bstatic -ljemalloc -Wl,-Bdynamic)
target_compile_definitions(testJEMallocAvailable PRIVATE FASTGRIND_JE_MALLOC)
target_link_libraries(testJEMallocAvailable PRIVATE fastgrind_instrument)

add_executable(testTCMallocAvailable testTCMallocAvailable.cpp)
target_link_libraries(testTCMallocAvailable PRIVATE pthread dl fastgrind_wrap_flags fastgrind_includes -lgcc_s -lunwind -Wl,-Bstatic -ltcmalloc -Wl,-Bdynamic)
target_compile_definitions(testTCMallocAvailable PRIVATE FASTGRIND_TC_MALLOC)
