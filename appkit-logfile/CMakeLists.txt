cmake_minimum_required(VERSION 3.16)
project(appkit-logfile LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_GENERATOR MATCHES "Visual Studio")
    message(WARNING "appkit-logfile: Visual Studio generator is not supported; use Ninja or Makefiles.")
endif()

option(APPKIT_LOGFILE_WERROR "Treat compiler warnings as errors" ON)
set(APPKIT_LOGFILE_FLAGS -Wall -Wextra -Wpedantic)
if(APPKIT_LOGFILE_WERROR)
    list(APPEND APPKIT_LOGFILE_FLAGS -Werror)
endif()

add_library(appkit_logfile STATIC src/logfile.cpp)
target_include_directories(appkit_logfile PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 交付态默认留桩由源码里的 #define FLAG 保证；这里只负责打开参考实现。
# 要看参考实现：cmake -DAPPKIT_LOGFILE_REFERENCE=ON
option(APPKIT_LOGFILE_REFERENCE "编译 RotatingFileLog::rotate 的参考实现（默认留桩）" OFF)
if(APPKIT_LOGFILE_REFERENCE)
    target_compile_definitions(appkit_logfile PRIVATE APPKIT_LOGFILE_REFERENCE)
endif()

enable_testing()
add_executable(logfile_test tests/logfile_test.cpp)
target_link_libraries(logfile_test PRIVATE appkit_logfile)

if(MSVC)
    target_compile_options(appkit_logfile PRIVATE /W4 /permissive-)
    target_compile_options(logfile_test PRIVATE /W4 /permissive-)
    if(APPKIT_LOGFILE_WERROR)
        target_compile_options(appkit_logfile PRIVATE /WX)
        target_compile_options(logfile_test PRIVATE /WX)
    endif()
else()
    target_compile_options(appkit_logfile PRIVATE ${APPKIT_LOGFILE_FLAGS})
    target_compile_options(logfile_test PRIVATE ${APPKIT_LOGFILE_FLAGS})
endif()

add_test(NAME logfile_test COMMAND logfile_test)
