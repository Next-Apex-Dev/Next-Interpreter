cmake_minimum_required(VERSION 3.16)
project(next11 CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# C++20 协程支持
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-fcoroutines)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-fcoroutines)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/await)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -Wno-pedantic)

include_directories(${CMAKE_SOURCE_DIR}/src)

set(SOURCES
    main.cpp
    src/common/registry.cpp
    src/lexer/lexer.cpp
    src/parser/parser.cpp
    src/semantic/semantic_analyzer.cpp
    src/interp/interpreter.cpp
    src/stdlib/stdlib.cpp
    src/stdlib/builtins.cpp
    src/stdlib/stdlib_tools.cpp
    src/stdlib/gui.cpp
    src/driver/pipeline.cpp
    src/driver/cli_driver.cpp
    src/driver/repl_driver.cpp
)

add_executable(next11 ${SOURCES})

# 增大栈空间至 4MB
if(MINGW)
    target_link_options(next11 PRIVATE "-Wl,--stack,0x400000")
    # 静态链接，便于分发
    target_link_options(next11 PRIVATE "-static")
elseif(MSVC)
    target_link_options(next11 PRIVATE "/STACK:4194304")
endif()

# Windows GUI 库链接
if(WIN32)
    target_link_libraries(next11 PRIVATE user32 gdi32 comctl32)
endif()

set_target_properties(next11 PROPERTIES OUTPUT_NAME next11)