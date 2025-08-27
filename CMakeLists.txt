cmake_minimum_required(VERSION 3.13)

# 设置项目名称和版本
set(CMAKE_CXX_STANDARD 11)
project(mmPool VERSION 1.0 LANGUAGES CXX)

# 设置安装路径（可选）
set(CMAKE_INSTALL_PREFIX /usr/local)

# 添加包含目录
include_directories(include)

set(
    WRAP_FLAGS 
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

# test1 使用 tcmalloc
add_executable(test1 test/test1.cpp)
target_link_libraries(test1 pthread dl -Wl,-Bstatic -ltcmalloc -Wl,-Bdynamic -lgcc_s unwind)
target_link_options(test1 PRIVATE ${WRAP_FLAGS})
target_compile_definitions(test1 PRIVATE TC_MALLOC)
target_compile_options(test1 PRIVATE -g -O0)

# test2 使用 jemalloc
add_executable(test2 test/test2.cpp)
target_link_libraries(test2 pthread dl -Wl,-Bstatic -ljemalloc -Wl,-Bdynamic -lgcc_s)
target_link_options(test2 PRIVATE ${WRAP_FLAGS})
target_compile_definitions(test2 PRIVATE JE_MALLOC)
target_compile_options(test2 PRIVATE -g -O0)

# test3 使用默认的 malloc/free
add_executable(test3 test/test3.cpp)
target_link_libraries(test3 pthread dl -Wl,-Bstatic -Wl,-Bdynamic -lgcc_s)
target_link_options(test3 PRIVATE ${WRAP_FLAGS})
target_compile_options(test3 PRIVATE -g -O0)

# 安装头文件和静态库
install(DIRECTORY include/ DESTINATION include)
