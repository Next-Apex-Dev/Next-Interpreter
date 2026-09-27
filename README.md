# Next1.1 编程语言解释器

Next1.1 是一门支持函数式、面向对象、元编程、协程、GUI 等特性的脚本语言，由 C++20 实现。

## 功能特性

- 基本类型：int、float、string、bool、complex、null
- 容器：array、tuple、dict、set
- 控制流：if/else、while、for-in、match、try-catch
- 函数：闭包、生成器、协程、异步
- 面向对象：class、继承、方法
- 结构体与枚举
- 泛型
- 模块导入
- 文件 I/O
- GUI（Windows）
- REPL 交互式环境

## 编译要求

- C++20 编译器（g++ 13+、clang 16+、MSVC 2022+）
- CMake 3.16+
- Windows 需要 user32、gdi32、comctl32 系统库

## 编译方法

### 方法一：CMake

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

编译后生成 `next11.exe`（Windows）或 `next11`（Linux/macOS）。

### 方法二：直接用 g++ 编译

```bash
g++ -std=c++20 -fcoroutines -O2 -static -Isrc -Wl,--stack,0x400000 \
    main.cpp src/common/registry.cpp src/lexer/lexer.cpp \
    src/parser/parser.cpp src/semantic/semantic_analyzer.cpp \
    src/interp/interpreter.cpp src/stdlib/stdlib.cpp \
    src/stdlib/builtins.cpp src/stdlib/stdlib_tools.cpp \
    src/stdlib/gui.cpp src/driver/pipeline.cpp \
    src/driver/cli_driver.cpp src/driver/repl_driver.cpp \
    -luser32 -lgdi32 -lcomctl32 -o next11
```

## 运行

```bash
# 运行脚本文件
next11 hello.next

# 启动 REPL
next11
```

## 测试

```bash
# 运行全部测试
next11 tests/hello.next
next11 tests/func.next
next11 tests/struct_enum.next
# ... 共 36 个测试
```

或使用测试脚本：

```bash
run_tests.bat
```

## 项目结构

```
main.cpp                 程序入口
src/
  common/                公共模块（AST、Value、Type、Registry）
  lexer/                 词法分析
  parser/                语法分析
  semantic/              语义分析
  interp/                解释器（执行引擎、作用域、协程、文件I/O、沙箱）
  stdlib/                标准库（内置函数、GUI、工具函数）
  driver/                驱动（CLI、REPL、Pipeline）
tests/                   36 个测试文件
next11-ide/              Electron IDE（可选）
CMakeLists.txt           CMake 构建配置
error_codes.md           错误码文档
```

## 许可证

MIT License