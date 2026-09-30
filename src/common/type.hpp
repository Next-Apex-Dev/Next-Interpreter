// 类型系统模块 - 对齐 spec 6.4 / design 2.3.2.4
// Type 继承体系: Type (基类) ← BasicType / ArrayType / StructType / GenericInstance
// 派生类各自持有专属字段，基类通过访问器方法统一访问
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <map>
#include <cassert>
#include "ast.hpp"

namespace next11 {

enum class TypeKind {
    Basic, Array, Tuple, Struct, Enum, Func, GenericInstance, TypeParam, Unknown
};

enum class BaseType { Int, Float, String, Bool, Void };

class BasicType;
class ArrayType;
class StructType;
class GenericInstance;

class Type {
public:
    TypeKind kind = TypeKind::Unknown;
    std::string struct_name;
    std::vector<std::shared_ptr<Type>> tuple_elems;
    std::string enum_name;
    std::vector<std::string> enum_variants;
    std::vector<std::shared_ptr<Type>> func_params;
    std::shared_ptr<Type> func_return;
    std::vector<std::string> type_params;

    Type() = default;
    virtual ~Type() = default;

    virtual std::shared_ptr<Type> clone() const {
        return std::make_shared<Type>(*this);
    }

    static std::shared_ptr<Type> make_basic(BaseType b);
    static std::shared_ptr<Type> make_int();
    static std::shared_ptr<Type> make_float();
    static std::shared_ptr<Type> make_string();
    static std::shared_ptr<Type> make_bool();
    static std::shared_ptr<Type> make_void();
    static std::shared_ptr<Type> make_array(std::shared_ptr<Type> e);
    static std::shared_ptr<Type> make_unknown();
    static std::shared_ptr<Type> make_type_param(std::string name);

    virtual std::string to_string() const;

    BaseType get_base() const;
    std::shared_ptr<Type> get_elem() const;
    void set_elem(std::shared_ptr<Type> e);
    const std::vector<std::pair<std::string, std::shared_ptr<Type>>>& get_fields() const;
    std::vector<std::pair<std::string, std::shared_ptr<Type>>>& get_fields_mut();
    const std::map<std::string, std::shared_ptr<Type>>& get_bindings() const;
};

class BasicType : public Type {
public:
    BaseType base = BaseType::Void;
    BasicType() { kind = TypeKind::Basic; }
    explicit BasicType(BaseType b) : base(b) { kind = TypeKind::Basic; }
    std::shared_ptr<Type> clone() const override {
        return std::make_shared<BasicType>(*this);
    }
    std::string to_string() const override;
};

class ArrayType : public Type {
public:
    std::shared_ptr<Type> elem;
    ArrayType() { kind = TypeKind::Array; }
    explicit ArrayType(std::shared_ptr<Type> e) : elem(std::move(e)) { kind = TypeKind::Array; }
    std::shared_ptr<Type> clone() const override {
        return std::make_shared<ArrayType>(*this);
    }
    std::string to_string() const override;
};

class StructType : public Type {
public:
    std::vector<std::pair<std::string, std::shared_ptr<Type>>> fields;
    StructType() { kind = TypeKind::Struct; }
    explicit StructType(std::string n) { kind = TypeKind::Struct; struct_name = std::move(n); }
    std::shared_ptr<Type> clone() const override {
        return std::make_shared<StructType>(*this);
    }
    std::string to_string() const override;
};

class GenericInstance : public Type {
public:
    std::map<std::string, std::shared_ptr<Type>> bindings;
    GenericInstance() { kind = TypeKind::GenericInstance; }
    explicit GenericInstance(std::string n) { kind = TypeKind::GenericInstance; struct_name = std::move(n); }
    std::shared_ptr<Type> clone() const override {
        return std::make_shared<GenericInstance>(*this);
    }
    std::string to_string() const override;
};

inline BaseType Type::get_base() const {
    auto* bt = dynamic_cast<const BasicType*>(this);
    return bt ? bt->base : BaseType::Void;
}

inline std::shared_ptr<Type> Type::get_elem() const {
    auto* at = dynamic_cast<const ArrayType*>(this);
    return at ? at->elem : nullptr;
}

inline void Type::set_elem(std::shared_ptr<Type> e) {
    auto* at = dynamic_cast<ArrayType*>(this);
    if (at) at->elem = std::move(e);
}

inline const std::vector<std::pair<std::string, std::shared_ptr<Type>>>& Type::get_fields() const {
    auto* st = dynamic_cast<const StructType*>(this);
    if (st) return st->fields;
    static const std::vector<std::pair<std::string, std::shared_ptr<Type>>> empty;
    return empty;
}

inline std::vector<std::pair<std::string, std::shared_ptr<Type>>>& Type::get_fields_mut() {
    auto* st = dynamic_cast<StructType*>(this);
    if (!st) throw std::runtime_error("get_fields_mut called on non-StructType");
    return st->fields;
}

inline const std::map<std::string, std::shared_ptr<Type>>& Type::get_bindings() const {
    auto* gi = dynamic_cast<const GenericInstance*>(this);
    if (gi) return gi->bindings;
    static const std::map<std::string, std::shared_ptr<Type>> empty;
    return empty;
}

inline std::shared_ptr<Type> Type::make_basic(BaseType b) {
    return std::make_shared<BasicType>(b);
}
inline std::shared_ptr<Type> Type::make_int() { return make_basic(BaseType::Int); }
inline std::shared_ptr<Type> Type::make_float() { return make_basic(BaseType::Float); }
inline std::shared_ptr<Type> Type::make_string() { return make_basic(BaseType::String); }
inline std::shared_ptr<Type> Type::make_bool() { return make_basic(BaseType::Bool); }
inline std::shared_ptr<Type> Type::make_void() { return make_basic(BaseType::Void); }
inline std::shared_ptr<Type> Type::make_array(std::shared_ptr<Type> e) {
    return std::make_shared<ArrayType>(std::move(e));
}
inline std::shared_ptr<Type> Type::make_unknown() {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::Unknown;
    return t;
}
inline std::shared_ptr<Type> Type::make_type_param(std::string name) {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::TypeParam;
    t->struct_name = std::move(name);
    return t;
}

inline std::string Type::to_string() const {
    switch (kind) {
        case TypeKind::Basic: {
            auto* bt = dynamic_cast<const BasicType*>(this);
            if (bt) return bt->BasicType::to_string();
            return "?";
        }
        case TypeKind::Array: {
            auto* at = dynamic_cast<const ArrayType*>(this);
            if (at) return at->ArrayType::to_string();
            return "?[]";
        }
        case TypeKind::Tuple: {
            std::string r = "(";
            for (size_t i = 0; i < tuple_elems.size(); ++i) {
                if (i) r += ", ";
                r += tuple_elems[i]->to_string();
            }
            return r + ")";
        }
        case TypeKind::Struct: return struct_name;
        case TypeKind::Enum: return enum_name;
        case TypeKind::Func: return "fn";
        case TypeKind::GenericInstance: return struct_name + "<...>";
        case TypeKind::TypeParam: return struct_name;
        case TypeKind::Unknown: return "unknown";
    }
    return "?";
}

inline std::string BasicType::to_string() const {
    switch (base) {
        case BaseType::Int: return "int";
        case BaseType::Float: return "float";
        case BaseType::String: return "string";
        case BaseType::Bool: return "bool";
        case BaseType::Void: return "void";
    }
    return "?";
}

inline std::string ArrayType::to_string() const {
    return (elem ? elem->to_string() : "?") + "[]";
}

inline std::string StructType::to_string() const {
    return struct_name;
}

inline std::string GenericInstance::to_string() const {
    return struct_name + "<...>";
}

inline bool is_compatible(const std::shared_ptr<Type>& expected,
                          const std::shared_ptr<Type>& actual) {
    if (!expected || !actual) return true;
    if (expected->kind == TypeKind::Unknown || actual->kind == TypeKind::Unknown)
        return true;
    if (expected->kind == TypeKind::Basic && actual->kind == TypeKind::Basic) {
        if (expected->get_base() == actual->get_base()) return true;
        if (expected->get_base() == BaseType::Float && actual->get_base() == BaseType::Int)
            return true;
        return false;
    }
    if (expected->kind == TypeKind::Array && actual->kind == TypeKind::Array) {
        return is_compatible(expected->get_elem(), actual->get_elem());
    }
    if (expected->kind == TypeKind::Tuple && actual->kind == TypeKind::Tuple) {
        if (expected->tuple_elems.size() != actual->tuple_elems.size()) return false;
        for (size_t i = 0; i < expected->tuple_elems.size(); ++i) {
            if (!is_compatible(expected->tuple_elems[i], actual->tuple_elems[i])) return false;
        }
        return true;
    }
    if (expected->kind == actual->kind) {
        if (expected->kind == TypeKind::Struct || expected->kind == TypeKind::Enum)
            return expected->struct_name == actual->struct_name;
        if (expected->kind == TypeKind::TypeParam)
            return true;
    }
    return false;
}

inline std::shared_ptr<Type> type_from_ref(const TypeRef& ref) {
    if (!ref.valid) return Type::make_unknown();
    if (ref.is_tuple) {
        auto t = std::make_shared<Type>();
        t->kind = TypeKind::Tuple;
        for (auto& e : ref.tuple_elems) t->tuple_elems.push_back(type_from_ref(e));
        return t;
    }
    if (ref.is_array) {
        return Type::make_array(ref.elem ? type_from_ref(*ref.elem) : Type::make_unknown());
    }
    if (ref.name == "int") return Type::make_int();
    if (ref.name == "float") return Type::make_float();
    if (ref.name == "string") return Type::make_string();
    if (ref.name == "bool") return Type::make_bool();
    if (ref.name == "void") return Type::make_void();
    if (!ref.type_args.empty()) {
        return std::make_shared<GenericInstance>(ref.name);
    }
    return std::make_shared<StructType>(ref.name);
}

} // namespace next11
