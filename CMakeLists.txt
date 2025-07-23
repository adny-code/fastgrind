cmake_minimum_required(VERSION 3.10)

# 设置项目名称和版本
set(CMAKE_CXX_STANDARD 17)
project(mmPool VERSION 1.0 LANGUAGES CXX)

# 设置安装路径（可选）
set(CMAKE_INSTALL_PREFIX /usr/local)

# 添加包含目录
include_directories(include)

# 添加库目标
add_library(merecord STATIC src/memRecord.cpp)

# 可选：添加可执行文件以测试库
add_executable(test test/test1.cpp)

target_link_libraries(test pthread)

# 安装头文件和静态库
install(TARGETS merecord ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
