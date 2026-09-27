// 符号表模块 - 对齐 spec 6.3
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include "type.hpp"

namespace next11 {

enum class SymbolCategory {
    Variable, Function, Struct, Enum, TypeAlias, TypeParam, Module
};

struct SourceLoc {
    std::string file;
    int line;
    int col;
};

struct SymbolEntry {
    std::string name;
    std::shared_ptr<Type> type;
    int scope_level = 0;
    SourceLoc decl_loc;
    SymbolCategory category = SymbolCategory::Variable;
    // 函数特有
    std::vector<std::string> type_params;
    std::vector<Param> params;
    TypeRef return_type;
    // struct 特有
    std::vector<Param> struct_fields;
    // enum 特有
    std::vector<std::string> enum_variants;
};

struct Scope {
    int level = 0;
    std::unordered_map<std::string, SymbolEntry> symbols;
};

class SymbolTable {
public:
    std::vector<Scope> _scopes;
    std::unordered_map<std::string, SourceLoc> _exited_decls; // 已退出作用域的变量声明位置

    SymbolTable() { enter_scope(); } // 全局作用域

    void enter_scope() {
        Scope s;
        s.level = static_cast<int>(_scopes.size());
        _scopes.push_back(std::move(s));
    }

    void exit_scope() {
        if (_scopes.size() > 1) {
            for (auto& kv : _scopes.back().symbols) {
                _exited_decls[kv.first] = kv.second.decl_loc;
            }
            _scopes.pop_back();
        }
    }

    int current_level() const { return static_cast<int>(_scopes.size()) - 1; }

    bool declare(const std::string& name, SymbolEntry entry) {
        auto& cur = _scopes.back();
        if (cur.symbols.count(name)) return false;
        entry.scope_level = cur.level;
        cur.symbols[name] = std::move(entry);
        return true;
    }

    std::optional<SymbolEntry> lookup(const std::string& name) const {
        for (int i = static_cast<int>(_scopes.size()) - 1; i >= 0; --i) {
            auto it = _scopes[i].symbols.find(name);
            if (it != _scopes[i].symbols.end()) return it->second;
        }
        return std::nullopt;
    }

    std::optional<SymbolEntry> lookup_local(const std::string& name) const {
        auto it = _scopes.back().symbols.find(name);
        if (it != _scopes.back().symbols.end()) return it->second;
        return std::nullopt;
    }

    std::optional<SourceLoc> decl_loc_of(const std::string& name) const {
        for (int i = static_cast<int>(_scopes.size()) - 1; i >= 0; --i) {
            auto it = _scopes[i].symbols.find(name);
            if (it != _scopes[i].symbols.end()) return it->second.decl_loc;
        }
        return std::nullopt;
    }

    std::optional<SourceLoc> lookup_exited(const std::string& name) const {
        auto it = _exited_decls.find(name);
        if (it != _exited_decls.end()) return it->second;
        return std::nullopt;
    }
};

} // namespace next11