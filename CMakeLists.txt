cmake_minimum_required(VERSION 3.13)

# 设置项目名称和版本
set(CMAKE_CXX_STANDARD 17)
project(mmPool VERSION 1.0 LANGUAGES CXX)

# 设置安装路径（可选）
set(CMAKE_INSTALL_PREFIX /usr/local)

# 添加包含目录
include_directories(include)

set(WRAP_FLAGS 
    -Wl,--wrap=malloc
    -Wl,--wrap=free
    -Wl,--wrap=_Znwm
    -Wl,--wrap=_Znam
    -Wl,--wrap=_ZdlPv
    -Wl,--wrap=_ZdaPv
    -Wl,--wrap=_ZdaPvm
    -Wl,--wrap=_ZdlPvm)

# test1 使用 tcmalloc
add_executable(test1 test/test1.cpp)
target_link_libraries(test1 pthread dl -Wl,-Bstatic -ltcmalloc -Wl,-Bdynamic -lgcc_s unwind)
target_link_options(test1 PRIVATE ${WRAP_FLAGS})
target_compile_definitions(test1 PRIVATE TCMALLOC)

# test2 使用 jemalloc
add_executable(test2 test/test2.cpp)
target_link_libraries(test2 pthread dl -Wl,-Bstatic -ljemalloc -Wl,-Bdynamic -lgcc_s)
target_link_options(test2 PRIVATE ${WRAP_FLAGS})
target_compile_definitions(test2 PRIVATE JE_MALLOC)

# 安装头文件和静态库
install(DIRECTORY include/ DESTINATION include)
