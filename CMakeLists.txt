cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 11)
project(mmPool VERSION 1.0 LANGUAGES CXX)

set(CMAKE_INSTALL_PREFIX /usr/local)

include_directories(include)

set(WRAP_FLAGS 
    # C++98
    -Wl,--wrap=malloc
    -Wl,--wrap=calloc
    -Wl,--wrap=realloc
    -Wl,--wrap=free
    -Wl,--wrap=_Znwm
    -Wl,--wrap=_Znam
    -Wl,--wrap=_ZdlPv
    -Wl,--wrap=_ZdaPv
    -Wl,--wrap=_ZnwmRKSt9nothrow_t
    -Wl,--wrap=_ZnamRKSt9nothrow_t
    -Wl,--wrap=_ZdlPvRKSt9nothrow_t
    -Wl,--wrap=_ZdaPvRKSt9nothrow_t
    # -Wl,--wrap=sbrk
    # -Wl,--wrap=brk
    # -Wl,--wrap=mmap
    # -Wl,--wrap=munmap
    # -Wl,--wrap=mremap
    -Wl,--wrap=valloc
    -Wl,--wrap=pvalloc
    -Wl,--wrap=memalign
    -Wl,--wrap=posix_memalign
    -Wl,--wrap=reallocarray
    # C++14
    -Wl,--wrap=_ZdaPvm
    -Wl,--wrap=_ZdlPvm
    # c++17
    -Wl,--wrap=aligned_alloc
    -Wl,--wrap=_ZnwmSt11align_val_t
    -Wl,--wrap=_ZnamSt11align_val_t
    -Wl,--wrap=_ZdlPvSt11align_val_t
    -Wl,--wrap=_ZdaPvSt11align_val_t
    -Wl,--wrap=_ZdlPvmSt11align_val_t
    -Wl,--wrap=_ZdaPvmSt11align_val_t
    -Wl,--wrap=_ZdlPvmRKSt9nothrow_t
    -Wl,--wrap=_ZdaPvmRKSt9nothrow_t
    -Wl,--wrap=_ZnwmSt11align_val_tRKSt9nothrow_t
    -Wl,--wrap=_ZnamSt11align_val_tRKSt9nothrow_t
    -Wl,--wrap=_ZdlPvSt11align_val_tRKSt9nothrow_t
    -Wl,--wrap=_ZdaPvSt11align_val_tRKSt9nothrow_t
    -Wl,--wrap=_ZdlPvmSt11align_val_tRKSt9nothrow_t
    -Wl,--wrap=_ZdaPvmSt11align_val_tRKSt9nothrow_t
    )

set(INSTRUMENT_EXCLUDE_FILES
    # Must be excluded
    /usr/include/c++/
    /usr/include/x86_64-linux-gnu/c++/
    /usr/lib/gcc/
    /usr/include/x86_64-linux-gnu/
    /usr/include/linux/
    /usr/include/asm
    /usr/include/asm-generic
    /usr/include/sys/
    /usr/include/bits/
    /usr/include/gnu/
    /usr/include/glib-2.0/
    /usr/lib/x86_64-linux-gnu/glib-2.0/include/
    /usr/include/c++/v1
    /usr/lib/clang/
    /usr/local/include/
    # Third-Party, should be excluded
    /usr/include/boost/
    /usr/include/eigen3/
    /usr/include/openssl/
    /usr/include/libunwind/
    /usr/include/jemalloc/
    /usr/include/gperftools/
    /usr/include/google/
    /usr/include/valgrind/
    /usr/include/cuda/
    /usr/local/cuda/include/
    /opt/local/include/
    ${CMAKE_SOURCE_DIR}/third_party/
    ${CMAKE_SOURCE_DIR}/extern/
    ${CMAKE_SOURCE_DIR}/vendor/
)

list(JOIN INSTRUMENT_EXCLUDE_FILES "," INSTRUMENT_EXCLUDE_FILES_JOINED)

# testGLibcAvailable
add_executable(testGLibcAvailable testCase/testGLibcAvailable.cpp)
target_link_libraries(testGLibcAvailable pthread dl -Wl,-Bstatic -Wl,-Bdynamic -lgcc_s -Wl,--export-dynamic)
target_link_options(testGLibcAvailable PRIVATE ${WRAP_FLAGS})
target_compile_options(testGLibcAvailable PRIVATE -g -O0)

# testJEMallocAvailable
add_executable(testJEMallocAvailable testCase/testJEMallocAvailable.cpp)
target_link_libraries(testJEMallocAvailable pthread dl -Wl,-Bstatic -ljemalloc -Wl,-Bdynamic -lgcc_s -Wl,--export-dynamic)
target_link_options(testJEMallocAvailable PRIVATE ${WRAP_FLAGS})
target_compile_definitions(testJEMallocAvailable PRIVATE MERECORDER_JE_MALLOC MERECORDER_INSTRUMENT)
target_compile_options(testJEMallocAvailable PRIVATE -g -O0 -finstrument-functions 
                        -finstrument-functions-exclude-file-list=${INSTRUMENT_EXCLUDE_FILES_JOINED})

# testTCMallocAvailable
add_executable(testTCMallocAvailable testCase/testTCMallocAvailable.cpp)
target_link_libraries(testTCMallocAvailable pthread dl -Wl,-Bstatic -ltcmalloc -Wl,-Bdynamic -lgcc_s -lunwind -Wl,--export-dynamic)
target_link_options(testTCMallocAvailable PRIVATE ${WRAP_FLAGS})
target_compile_definitions(testTCMallocAvailable PRIVATE MERECORDER_TC_MALLOC)
target_compile_options(testTCMallocAvailable PRIVATE -g -O0 -finstrument-functions)

install(DIRECTORY include/ DESTINATION include)
