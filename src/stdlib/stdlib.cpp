// 标准库注册实现
#include "stdlib.hpp"

namespace next11 {

// 前置声明
void register_all_builtins(BuiltinRegistry& reg);
void register_all_stdlib_tools(BuiltinRegistry& reg);

void StdLibRegistry::register_all() {
    BuiltinRegistry::instance().register_defaults();
}

void StdLibRegistry::register_all_tools() {
    register_all_stdlib_tools(BuiltinRegistry::instance());
}

} // namespace next11