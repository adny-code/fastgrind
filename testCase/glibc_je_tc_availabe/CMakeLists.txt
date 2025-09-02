# CMakeLists for glibc / jemalloc / tcmalloc availability test executables

add_executable(testGLibcAvailable testGLibcAvailable.cpp)
target_link_libraries(testGLibcAvailable pthread dl -Wl,-Bstatic -Wl,-Bdynamic -lgcc_s -Wl,--export-dynamic)
target_link_options(testGLibcAvailable PRIVATE ${WRAP_FLAGS})
target_compile_options(testGLibcAvailable PRIVATE -g -O0)

add_executable(testJEMallocAvailable testJEMallocAvailable.cpp)
target_link_libraries(testJEMallocAvailable pthread dl -Wl,-Bstatic -ljemalloc -Wl,-Bdynamic -lgcc_s -Wl,--export-dynamic)
target_link_options(testJEMallocAvailable PRIVATE ${WRAP_FLAGS})
target_compile_definitions(testJEMallocAvailable PRIVATE MERECORDER_JE_MALLOC MERECORDER_INSTRUMENT)
if (INSTRUMENT_EXCLUDE_FILES_JOINED)
    target_compile_options(testJEMallocAvailable PRIVATE -g -O0 -finstrument-functions -finstrument-functions-exclude-file-list=${INSTRUMENT_EXCLUDE_FILES_JOINED})
else()
    target_compile_options(testJEMallocAvailable PRIVATE -g -O0)
endif()

add_executable(testTCMallocAvailable testTCMallocAvailable.cpp)
target_link_libraries(testTCMallocAvailable pthread dl -Wl,-Bstatic -ltcmalloc -Wl,-Bdynamic -lgcc_s -lunwind -Wl,--export-dynamic)
target_link_options(testTCMallocAvailable PRIVATE ${WRAP_FLAGS})
target_compile_definitions(testTCMallocAvailable PRIVATE MERECORDER_TC_MALLOC)
target_compile_options(testTCMallocAvailable PRIVATE -g -O0)
