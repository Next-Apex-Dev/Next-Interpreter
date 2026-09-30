// 执行环境与调用帧 - 对齐 design 2.1.3.3
// Environment: 作用域栈管理（压入/弹出/查找）
// CallFrame: 函数调用上下文（创建/绑定形参/执行/返回）
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include "common/value.hpp"

namespace next11 {

class Environment {
public:
    using Scope = std::unordered_map<std::string, Value>;

    Environment() { _scopes.emplace_back(); }

    void push_scope() { _scopes.emplace_back(); }
    void pop_scope() { if (_scopes.size() > 1) _scopes.pop_back(); }

    void declare(const std::string& name, Value val) {
        _scopes.back()[name] = std::move(val);
    }

    Value* lookup(const std::string& name) {
        for (int i = static_cast<int>(_scopes.size()) - 1; i >= 0; --i) {
            auto it = _scopes[i].find(name);
            if (it != _scopes[i].end()) return &it->second;
        }
        return nullptr;
    }

    void assign(const std::string& name, Value val) {
        for (int i = static_cast<int>(_scopes.size()) - 1; i >= 0; --i) {
            auto it = _scopes[i].find(name);
            if (it != _scopes[i].end()) { it->second = std::move(val); return; }
        }
        _scopes.back()[name] = std::move(val);
    }

    size_t depth() const { return _scopes.size(); }

    const Scope& global_scope() const { return _scopes.front(); }

    void reset_with_global(const Scope& global) {
        Scope copy = global;
        _scopes.clear();
        _scopes.push_back(std::move(copy));
    }

    void update_global(const Scope& global) {
        if (!_scopes.empty()) _scopes[0] = global;
    }

    static Environment from_global(const Scope& global) {
        Environment env;
        Scope copy = global;
        env._scopes.clear();
        env._scopes.push_back(std::move(copy));
        return env;
    }

private:
    std::vector<Scope> _scopes;
};

struct CallFrame {
    std::string fn_name;
    std::unordered_map<std::string, Value> params;
    std::optional<Value> return_value;
    int depth;
    // 1.3 扩展字段：调试与异步/生成器支持
    std::string file;          // 当前帧文件名
    int line = 0;              // 当前帧行号
    int col = 0;               // 当前帧列号
    bool is_async = false;     // 是否异步帧
    void* generator = nullptr; // 若为生成器帧，指向生成器对象（void* 避免循环依赖）

    CallFrame(std::string name, int d) : fn_name(std::move(name)), depth(d) {}

    void bind_param(const std::string& param_name, Value val) {
        params[param_name] = std::move(val);
    }

    void set_return(Value v) {
        return_value = std::move(v);
    }

    Value get_return_or_null() const {
        return return_value.has_value() ? *return_value : Value::make_null();
    }
};

} // namespace next11