// 扩展点注册表 - 对齐 design 4.6 扩展点架构
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include "common/value.hpp"
#include "common/type.hpp"
#include "common/ast.hpp"

namespace next11 {

// ===== 注册 ID 与配置异常 =====
struct RegistrationId {
    std::string registry;
    std::string name;
    int seq;
};

class ConfigError : public std::runtime_error {
public:
    explicit ConfigError(const std::string& msg) : std::runtime_error(msg) {}
};

// ===== 运算符注册表 =====
struct OperatorInfo {
    std::string symbol;          // 运算符符号，如 "+"
    int precedence;              // 优先级（1-11）
    bool right_assoc;            // 是否右结合
    std::string category;        // "arithmetic"|"comparison"|"logical"|"pipe"|"assign"
    std::function<Value(const Value&, const Value&)> eval_fn;
};

class OperatorRegistry {
public:
    static OperatorRegistry& instance() {
        static OperatorRegistry reg;
        return reg;
    }

    RegistrationId register_op(const OperatorInfo& info) {
        std::lock_guard<std::mutex> lk(_mtx);
        if (_ops.count(info.symbol)) {
            throw ConfigError("运算符 '" + info.symbol + "' 已注册");
        }
        _ops[info.symbol] = info;
        return {"Operator", info.symbol, static_cast<int>(_ops.size())};
    }

    const OperatorInfo* lookup(const std::string& sym) const {
        std::lock_guard<std::mutex> lk(_mtx);
        auto it = _ops.find(sym);
        return it == _ops.end() ? nullptr : &it->second;
    }

    bool has(const std::string& sym) const { std::lock_guard<std::mutex> lk(_mtx); return _ops.count(sym) > 0; }

    std::vector<std::string> all_symbols() const {
        std::lock_guard<std::mutex> lk(_mtx);
        std::vector<std::string> r;
        for (auto& kv : _ops) r.push_back(kv.first);
        return r;
    }

    void register_defaults();

private:
    OperatorRegistry() = default;
    std::unordered_map<std::string, OperatorInfo> _ops;
    mutable std::mutex _mtx;
};

// ===== 语句注册表 =====
struct StmtHandlerInfo {
    std::string keyword;         // 语句关键字，如 "let"
    std::string category;        // "decl"|"control"|"expr"
    std::function<bool(const std::string&)> match_fn;
};

class StmtRegistry {
public:
    static StmtRegistry& instance() {
        static StmtRegistry reg;
        return reg;
    }

    RegistrationId register_handler(const StmtHandlerInfo& info) {
        std::lock_guard<std::mutex> lk(_mtx);
        if (_handlers.count(info.keyword)) {
            throw ConfigError("语句类型 '" + info.keyword + "' 已注册");
        }
        _handlers[info.keyword] = info;
        return {"Stmt", info.keyword, static_cast<int>(_handlers.size())};
    }

    const StmtHandlerInfo* lookup(const std::string& kw) const {
        std::lock_guard<std::mutex> lk(_mtx);
        auto it = _handlers.find(kw);
        return it == _handlers.end() ? nullptr : &it->second;
    }

    bool has(const std::string& kw) const { std::lock_guard<std::mutex> lk(_mtx); return _handlers.count(kw) > 0; }

    void register_defaults();

private:
    StmtRegistry() = default;
    std::unordered_map<std::string, StmtHandlerInfo> _handlers;
    mutable std::mutex _mtx;
};

// ===== 内建函数注册表 =====
struct BuiltinInfo {
    std::string name;
    int min_arity;
    int max_arity;               // -1 表示不限
    std::string return_type;     // 类型描述
    std::function<Value(std::vector<Value>&, std::ostream*, std::istream*, const SourceRange&)> eval_fn;
};

class BuiltinRegistry {
public:
    static BuiltinRegistry& instance() {
        static BuiltinRegistry reg;
        return reg;
    }

    RegistrationId register_builtin(const BuiltinInfo& info) {
        std::lock_guard<std::mutex> lk(_mtx);
        if (_builtins.count(info.name)) {
            throw ConfigError("内建函数 '" + info.name + "' 已注册");
        }
        _builtins[info.name] = info;
        return {"Builtin", info.name, static_cast<int>(_builtins.size())};
    }

    const BuiltinInfo* lookup(const std::string& name) const {
        std::lock_guard<std::mutex> lk(_mtx);
        auto it = _builtins.find(name);
        return it == _builtins.end() ? nullptr : &it->second;
    }

    bool has(const std::string& name) const { std::lock_guard<std::mutex> lk(_mtx); return _builtins.count(name) > 0; }

    std::vector<std::string> all_names() const {
        std::lock_guard<std::mutex> lk(_mtx);
        std::vector<std::string> r;
        for (auto& kv : _builtins) r.push_back(kv.first);
        return r;
    }

    void register_defaults();

private:
    BuiltinRegistry() = default;
    std::unordered_map<std::string, BuiltinInfo> _builtins;
    mutable std::mutex _mtx;
};

// ===== 类型注册表 =====
struct TypeInfo {
    std::string name;
    TypeKind kind;
    std::shared_ptr<Type> type;
    bool is_builtin_type;        // int/float/string/bool/void
};

class TypeRegistry {
public:
    static TypeRegistry& instance() {
        static TypeRegistry reg;
        return reg;
    }

    RegistrationId register_type(const TypeInfo& info) {
        std::lock_guard<std::mutex> lk(_mtx);
        if (_types.count(info.name)) {
            throw ConfigError("类型 '" + info.name + "' 已注册");
        }
        _types[info.name] = info;
        return {"Type", info.name, static_cast<int>(_types.size())};
    }

    const TypeInfo* lookup(const std::string& name) const {
        std::lock_guard<std::mutex> lk(_mtx);
        auto it = _types.find(name);
        return it == _types.end() ? nullptr : &it->second;
    }

    bool has(const std::string& name) const { std::lock_guard<std::mutex> lk(_mtx); return _types.count(name) > 0; }

    std::vector<std::string> all_names() const {
        std::lock_guard<std::mutex> lk(_mtx);
        std::vector<std::string> r;
        for (auto& kv : _types) r.push_back(kv.first);
        return r;
    }

    void register_defaults();

private:
    TypeRegistry() = default;
    std::unordered_map<std::string, TypeInfo> _types;
    mutable std::mutex _mtx;
};

// ===== 类型转换注册表 =====
struct ConversionInfo {
    std::string from_type;
    std::string to_type;
    bool is_implicit;            // 隐式转换还是显式转换
    std::function<Value(const Value&)> convert_fn;
};

class ConversionRegistry {
public:
    static ConversionRegistry& instance() {
        static ConversionRegistry reg;
        return reg;
    }

    RegistrationId register_conversion(const ConversionInfo& info) {
        std::lock_guard<std::mutex> lk(_mtx);
        _key key{info.from_type, info.to_type};
        if (_convs.count(key)) {
            throw ConfigError("类型转换 '" + info.from_type + "->" + info.to_type + "' 已注册");
        }
        _convs[key] = info;
        return {"Conversion", info.from_type + "->" + info.to_type, static_cast<int>(_convs.size())};
    }

    const ConversionInfo* lookup(const std::string& from, const std::string& to) const {
        std::lock_guard<std::mutex> lk(_mtx);
        auto it = _convs.find({from, to});
        return it == _convs.end() ? nullptr : &it->second;
    }

    bool can_convert(const std::string& from, const std::string& to) const {
        std::lock_guard<std::mutex> lk(_mtx);
        return _convs.count({from, to}) > 0;
    }

    Value convert(const Value& v, const std::string& to) const {
        std::lock_guard<std::mutex> lk(_mtx);
        auto it = _convs.find({v.type_name(), to});
        if (it == _convs.end()) return v;
        return it->second.convert_fn(v);
    }

    void register_defaults();

private:
    ConversionRegistry() = default;
    struct _key {
        std::string from, to;
        bool operator==(const _key& o) const { return from == o.from && to == o.to; }
    };
    struct _key_hash {
        size_t operator()(const _key& k) const {
            return std::hash<std::string>()(k.from) ^ (std::hash<std::string>()(k.to) << 1);
        }
    };
    std::unordered_map<_key, ConversionInfo, _key_hash> _convs;
    mutable std::mutex _mtx;
};

// ===== 统一初始化 =====
inline void register_all_defaults() {
    OperatorRegistry::instance().register_defaults();
    StmtRegistry::instance().register_defaults();

    TypeRegistry::instance().register_defaults();
    ConversionRegistry::instance().register_defaults();
}

// 全局类表指针（解释器设置，builtins 读取，用于 isinstance/issubtype MRO 查找）
struct ClassTableAccessor {
    std::unordered_map<std::string, ClassObjectData>* table = nullptr;
};
inline ClassTableAccessor g_class_table;

} // namespace next11