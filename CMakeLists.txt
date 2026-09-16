# ===========================================================================
#  appkit —— 顶层构建：一次配置，为两个模块统一产出一份 compile_commands.json
#  用法：
#    cmake -S . -B build
#    cmake --build build
#    ctest --test-dir build --output-on-failure
# ===========================================================================
cmake_minimum_required(VERSION 3.16)
project(appkit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 导出 compile_commands.json 供 clangd 建立索引（Makefile / Ninja 支持；
# Visual Studio 生成器会静默忽略，请用 -G Ninja）。
# 仓库根的 .clangd 把 CompilationDatabase 指向 build/，两个模块共用这一份。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 必须在顶层开启测试，ctest 才能一次跑完所有子项目的用例。
enable_testing()

add_subdirectory(appkit-config)
add_subdirectory(appkit-logfile)
