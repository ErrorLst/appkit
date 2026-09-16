cmake_minimum_required(VERSION 3.16)
project(appkit-config LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_GENERATOR MATCHES "Visual Studio")
    message(WARNING "appkit-config: Visual Studio generator is not supported; use Ninja or Makefiles.")
endif()

option(APPKIT_CONFIG_WERROR "Treat compiler warnings as errors" ON)
set(APPKIT_CONFIG_FLAGS -Wall -Wextra -Wpedantic)
if(APPKIT_CONFIG_WERROR)
    list(APPEND APPKIT_CONFIG_FLAGS -Werror)
endif()

add_library(appkit_config STATIC src/config.cpp)
target_include_directories(appkit_config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 交付态默认留桩由源码里的 #define FLAG 保证；这里只负责打开参考实现。
# 要看参考实现：cmake -DAPPKIT_CONFIG_REFERENCE=ON
option(APPKIT_CONFIG_REFERENCE "编译 Config::expand 的参考实现（默认留桩）" OFF)
if(APPKIT_CONFIG_REFERENCE)
    target_compile_definitions(appkit_config PRIVATE APPKIT_CONFIG_REFERENCE)
endif()

enable_testing()
add_executable(config_test tests/config_test.cpp)
target_link_libraries(config_test PRIVATE appkit_config)
target_compile_definitions(config_test PRIVATE
    APPKIT_CONFIG_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
    APPKIT_CONFIG_EXAMPLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/config")

if(MSVC)
    target_compile_options(appkit_config PRIVATE /W4 /permissive-)
    target_compile_options(config_test PRIVATE /W4 /permissive-)
    if(APPKIT_CONFIG_WERROR)
        target_compile_options(appkit_config PRIVATE /WX)
        target_compile_options(config_test PRIVATE /WX)
    endif()
else()
    target_compile_options(appkit_config PRIVATE ${APPKIT_CONFIG_FLAGS})
    target_compile_options(config_test PRIVATE ${APPKIT_CONFIG_FLAGS})
endif()

add_test(NAME config_test COMMAND config_test)
