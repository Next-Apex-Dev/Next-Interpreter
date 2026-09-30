// 标准库注册接口 - 对齐 tasks 8.1
#pragma once
#include "common/registry.hpp"

namespace next11 {

class StdLibRegistry {
public:
    // 向 BuiltinRegistry 注册所有内置函数（100个）
    static void register_all();
    // 向 BuiltinRegistry 注册所有标准库工具（100个）
    static void register_all_tools();
};

} // namespace next11