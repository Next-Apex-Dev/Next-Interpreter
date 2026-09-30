// 扩展点注册表默认实现
#include "common/registry.hpp"
#include <sstream>

namespace next11 {

// 前置声明：100个内置函数注册（定义在 src/stdlib/builtins.cpp）
void register_all_builtins(BuiltinRegistry& reg);
// 前置声明：100个标准库工具注册（定义在 src/stdlib/stdlib_tools.cpp）
void register_all_stdlib_tools(BuiltinRegistry& reg);
// 前置声明：GUI 函数注册（定义在 src/stdlib/gui.cpp）
void register_all_gui();

// ===== OperatorRegistry 默认运算符 =====
void OperatorRegistry::register_defaults() {
    auto reg = [this](const std::string& sym, int prec, bool ra, const std::string& cat,
                      Value(*fn)(const Value&, const Value&)) {
        OperatorInfo info;
        info.symbol = sym;
        info.precedence = prec;
        info.right_assoc = ra;
        info.category = cat;
        info.eval_fn = fn;
        register_op(info);
    };

    reg("+", 6, false, "arithmetic", [](const Value& l, const Value& r) -> Value {
        if (l.is_string() && r.is_string())
            return Value::make_string(l.to_display() + r.to_display());
        if (l.is_array() && r.is_array()) {
            auto result = l.as_array();
            auto& rhs = r.as_array();
            result.insert(result.end(), rhs.begin(), rhs.end());
            return Value::make_array(std::move(result));
        }
        // 复数加法
        if (l.is_complex() && r.is_complex()) {
            const auto& lc = l.as_complex();
            const auto& rc = r.as_complex();
            return Value::make_complex(lc.real + rc.real, lc.imag + rc.imag);
        }
        if (l.is_complex() && (r.is_int() || r.is_float())) {
            const auto& lc = l.as_complex();
            return Value::make_complex(lc.real + r.as_number(), lc.imag);
        }
        if ((l.is_int() || l.is_float()) && r.is_complex()) {
            const auto& rc = r.as_complex();
            return Value::make_complex(l.as_number() + rc.real, rc.imag);
        }
        if (l.is_int() && r.is_int()) {
            int64_t a = l.as_int(), b = r.as_int();
            if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) throw std::runtime_error("RUN350: 整数加法溢出");
            return Value::make_int(a + b);
        }
        if ((l.is_int() || l.is_float()) && (r.is_int() || r.is_float()))
            return Value::make_float(l.as_number() + r.as_number());
        throw std::runtime_error("RUN120: + 运算符不支持的类型: " + l.type_name() + " + " + r.type_name());
    });
    reg("-", 6, false, "arithmetic", [](const Value& l, const Value& r) -> Value {
        if (l.is_set() && r.is_set()) {
            // 集合差集
            auto set_data = std::make_shared<SetData>();
            for (const auto& le : l.as_set().elements) {
                bool found = false;
                for (const auto& re : r.as_set().elements) {
                    if (le.equals(re)) { found = true; break; }
                }
                if (!found) set_data->elements.push_back(le);
            }
            return Value::make_set(std::move(set_data));
        }
        // 复数减法
        if (l.is_complex() && r.is_complex()) {
            const auto& lc = l.as_complex();
            const auto& rc = r.as_complex();
            return Value::make_complex(lc.real - rc.real, lc.imag - rc.imag);
        }
        if (l.is_complex() && (r.is_int() || r.is_float())) {
            const auto& lc = l.as_complex();
            return Value::make_complex(lc.real - r.as_number(), lc.imag);
        }
        if ((l.is_int() || l.is_float()) && r.is_complex()) {
            const auto& rc = r.as_complex();
            return Value::make_complex(l.as_number() - rc.real, -rc.imag);
        }
        if (l.is_int() && r.is_int()) {
            int64_t a = l.as_int(), b = r.as_int();
            if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b)) throw std::runtime_error("RUN351: 整数减法溢出");
            return Value::make_int(a - b);
        }
        if ((l.is_int() || l.is_float()) && (r.is_int() || r.is_float()))
            return Value::make_float(l.as_number() - r.as_number());
        throw std::runtime_error("RUN121: - 运算符不支持的类型: " + l.type_name() + " - " + r.type_name());
    });
    reg("*", 7, false, "arithmetic", [](const Value& l, const Value& r) -> Value {
        // 复数乘法: (a+bi)*(c+di) = (ac-bd)+(ad+bc)i
        if (l.is_complex() && r.is_complex()) {
            const auto& lc = l.as_complex();
            const auto& rc = r.as_complex();
            double real = lc.real * rc.real - lc.imag * rc.imag;
            double imag = lc.real * rc.imag + lc.imag * rc.real;
            return Value::make_complex(real, imag);
        }
        if (l.is_complex() && (r.is_int() || r.is_float())) {
            const auto& lc = l.as_complex();
            double s = r.as_number();
            return Value::make_complex(lc.real * s, lc.imag * s);
        }
        if ((l.is_int() || l.is_float()) && r.is_complex()) {
            const auto& rc = r.as_complex();
            double s = l.as_number();
            return Value::make_complex(s * rc.real, s * rc.imag);
        }
        if (l.is_int() && r.is_int()) {
            int64_t a = l.as_int(), b = r.as_int();
            if (a != 0 && b != 0) {
                if (a == INT64_MIN) throw std::runtime_error("RUN352: 整数乘法溢出");
                if (b > 0) { if (a > INT64_MAX / b || a < INT64_MIN / b) throw std::runtime_error("RUN352: 整数乘法溢出"); }
                else if (b < -1) { if (a > INT64_MIN / b || a < INT64_MAX / b) throw std::runtime_error("RUN352: 整数乘法溢出"); }
                else if (b == -1) { if (a == INT64_MIN) throw std::runtime_error("RUN352: 整数乘法溢出"); }
            }
            return Value::make_int(a * b);
        }
        if ((l.is_int() || l.is_float()) && (r.is_int() || r.is_float()))
            return Value::make_float(l.as_number() * r.as_number());
        throw std::runtime_error("RUN122: * 运算符不支持的类型: " + l.type_name() + " * " + r.type_name());
    });
    reg("/", 7, false, "arithmetic", [](const Value& l, const Value& r) -> Value {
        if ((r.is_int() || r.is_float()) && r.as_number() == 0.0) throw std::runtime_error("RUN001: 除零错误");
        // 复数除法: (a+bi)/(c+di) = (ac+bd)/(c^2+d^2) + (bc-ad)/(c^2+d^2)i
        if (l.is_complex() && r.is_complex()) {
            const auto& lc = l.as_complex();
            const auto& rc = r.as_complex();
            double denom = rc.real * rc.real + rc.imag * rc.imag;
            if (denom == 0.0) throw std::runtime_error("RUN001: 复数除零错误");
            double real = (lc.real * rc.real + lc.imag * rc.imag) / denom;
            double imag = (lc.imag * rc.real - lc.real * rc.imag) / denom;
            return Value::make_complex(real, imag);
        }
        if (l.is_complex() && (r.is_int() || r.is_float())) {
            const auto& lc = l.as_complex();
            double s = r.as_number();
            if (s == 0.0) throw std::runtime_error("RUN001: 除零错误");
            return Value::make_complex(lc.real / s, lc.imag / s);
        }
        if ((l.is_int() || l.is_float()) && r.is_complex()) {
            const auto& rc = r.as_complex();
            double denom = rc.real * rc.real + rc.imag * rc.imag;
            if (denom == 0.0) throw std::runtime_error("RUN001: 复数除零错误");
            double s = l.as_number();
            double real = (s * rc.real) / denom;
            double imag = (-s * rc.imag) / denom;
            return Value::make_complex(real, imag);
        }
        if (l.is_int() && r.is_int()) {
            int64_t a = l.as_int(), b = r.as_int();
            if (a == INT64_MIN && b == -1) throw std::runtime_error("RUN353: 整数除法溢出");
            return Value::make_int(a / b);
        }
        if ((l.is_int() || l.is_float()) && (r.is_int() || r.is_float()))
            return Value::make_float(l.as_number() / r.as_number());
        throw std::runtime_error("RUN123: / 运算符不支持的类型: " + l.type_name() + " / " + r.type_name());
    });
    reg("%", 7, false, "arithmetic", [](const Value& l, const Value& r) -> Value {
        if (!l.is_int() || !r.is_int()) throw std::runtime_error("RUN124: % 运算符要求整数操作数: " + l.type_name() + " % " + r.type_name());
        if (r.as_int() == 0) throw std::runtime_error("RUN001: 模零错误");
        int64_t a = l.as_int(), b = r.as_int();
        if (a == INT64_MIN && b == -1) throw std::runtime_error("RUN354: 整数取模溢出");
        return Value::make_int(a % b);
    });
    reg("==", 4, false, "comparison", [](const Value& l, const Value& r) -> Value {
        return Value::make_bool(l.equals(r));
    });
    reg("!=", 4, false, "comparison", [](const Value& l, const Value& r) -> Value {
        return Value::make_bool(!l.equals(r));
    });
    reg("<", 4, false, "comparison", [](const Value& l, const Value& r) -> Value {
        if (l.is_string() && r.is_string()) return Value::make_bool(l.as_string() < r.as_string());
        if ((l.is_int() || l.is_float()) && (r.is_int() || r.is_float())) return Value::make_bool(l.as_number() < r.as_number());
        throw std::runtime_error("RUN125: < 运算符不支持的类型: " + l.type_name() + " < " + r.type_name());
    });
    reg("<=", 4, false, "comparison", [](const Value& l, const Value& r) -> Value {
        if (l.is_string() && r.is_string()) return Value::make_bool(l.as_string() <= r.as_string());
        if ((l.is_int() || l.is_float()) && (r.is_int() || r.is_float())) return Value::make_bool(l.as_number() <= r.as_number());
        throw std::runtime_error("RUN126: <= 运算符不支持的类型: " + l.type_name() + " <= " + r.type_name());
    });
    reg(">", 4, false, "comparison", [](const Value& l, const Value& r) -> Value {
        if (l.is_string() && r.is_string()) return Value::make_bool(l.as_string() > r.as_string());
        if ((l.is_int() || l.is_float()) && (r.is_int() || r.is_float())) return Value::make_bool(l.as_number() > r.as_number());
        throw std::runtime_error("RUN127: > 运算符不支持的类型: " + l.type_name() + " > " + r.type_name());
    });
    reg(">=", 4, false, "comparison", [](const Value& l, const Value& r) -> Value {
        if (l.is_string() && r.is_string()) return Value::make_bool(l.as_string() >= r.as_string());
        if ((l.is_int() || l.is_float()) && (r.is_int() || r.is_float())) return Value::make_bool(l.as_number() >= r.as_number());
        throw std::runtime_error("RUN128: >= 运算符不支持的类型: " + l.type_name() + " >= " + r.type_name());
    });
    // in 运算符：成员检查
    reg("in", 4, false, "comparison", [](const Value& l, const Value& r) -> Value {
        if (r.is_set()) {
            for (const auto& e : r.as_set().elements) {
                if (e.equals(l)) return Value::make_bool(true);
            }
            return Value::make_bool(false);
        }
        if (r.is_array()) {
            for (const auto& e : r.as_array()) {
                if (e.equals(l)) return Value::make_bool(true);
            }
            return Value::make_bool(false);
        }
        if (r.is_tuple()) {
            for (const auto& e : r.as_tuple()) {
                if (e.equals(l)) return Value::make_bool(true);
            }
            return Value::make_bool(false);
        }
        if (r.is_string()) {
            if (!l.is_string()) throw std::runtime_error("RUN130: in 运算符字符串成员检查要求左操作数为字符串");
            std::string sub = l.as_string();
            std::string s = r.as_string();
            return Value::make_bool(s.find(sub) != std::string::npos);
        }
        if (r.is_dict()) {
            for (const auto& kv : r.as_dict().entries) {
                if (kv.first.equals(l)) return Value::make_bool(true);
            }
            return Value::make_bool(false);
        }
        throw std::runtime_error("RUN129: in 运算符不支持的类型: " + l.type_name() + " in " + r.type_name());
    });
    reg("&&", 3, false, "logical", [](const Value& l, const Value& r) -> Value {
        return Value::make_bool(l.truthy() && r.truthy());
    });
    reg("||", 2, false, "logical", [](const Value& l, const Value& r) -> Value {
        return Value::make_bool(l.truthy() || r.truthy());
    });
    reg("=", 1, true, "assign", [](const Value&, const Value& r) -> Value { return r; });
    reg("|>", 1, false, "pipe", [](const Value&, const Value& r) -> Value { return r; });
    // 集合运算符
    reg("|", 8, false, "set", [](const Value& l, const Value& r) -> Value {
        // 集合并集
        if (l.is_set() && r.is_set()) {
            auto set_data = std::make_shared<SetData>();
            // 添加左边集合的所有元素
            for (const auto& e : l.as_set().elements) set_data->elements.push_back(e);
            // 添加右边集合中不重复的元素
            for (const auto& re : r.as_set().elements) {
                bool found = false;
                for (const auto& le : set_data->elements) {
                    if (le.equals(re)) { found = true; break; }
                }
                if (!found) set_data->elements.push_back(re);
            }
            return Value::make_set(std::move(set_data));
        }
        throw std::runtime_error("RUN021: 集合并集 | 要求两侧均为集合类型");
    });
    reg("&", 8, false, "set", [](const Value& l, const Value& r) -> Value {
        // 集合交集
        if (l.is_set() && r.is_set()) {
            auto set_data = std::make_shared<SetData>();
            // 查找两个集合中都存在的元素
            for (const auto& le : l.as_set().elements) {
                for (const auto& re : r.as_set().elements) {
                    if (le.equals(re)) {
                        set_data->elements.push_back(le);
                        break;
                    }
                }
            }
            return Value::make_set(std::move(set_data));
        }
        throw std::runtime_error("RUN022: 集合交集 & 要求两侧均为集合类型");
    });

    reg("^", 8, false, "set", [](const Value& l, const Value& r) -> Value {
        // 集合对称差
        if (l.is_set() && r.is_set()) {
            auto set_data = std::make_shared<SetData>();
            // 左边有但右边没有
            for (const auto& le : l.as_set().elements) {
                bool found = false;
                for (const auto& re : r.as_set().elements) {
                    if (le.equals(re)) { found = true; break; }
                }
                if (!found) set_data->elements.push_back(le);
            }
            // 右边有但左边没有
            for (const auto& re : r.as_set().elements) {
                bool found = false;
                for (const auto& le : l.as_set().elements) {
                    if (le.equals(re)) { found = true; break; }
                }
                if (!found) set_data->elements.push_back(re);
            }
            return Value::make_set(std::move(set_data));
        }
        throw std::runtime_error("RUN023: 集合对称差 ^ 要求两侧均为集合类型");
    });
}

// ===== StmtRegistry 默认语句 =====
void StmtRegistry::register_defaults() {
    auto reg = [this](const std::string& kw, const std::string& cat) {
        StmtHandlerInfo info;
        info.keyword = kw;
        info.category = cat;
        info.match_fn = [kw](const std::string& s) { return s == kw; };
        register_handler(info);
    };
    reg("let", "decl");
    reg("fn", "decl");
    reg("def", "decl");
    reg("struct", "decl");
    reg("enum", "decl");
    reg("type", "decl");
    reg("if", "control");
    reg("while", "control");
    reg("for", "control");
    reg("match", "control");
    reg("return", "control");
    reg("break", "control");
    reg("continue", "control");
    reg("with", "control");
    reg("async", "control");
    reg("try", "control");
    reg("throw", "control");
    reg("del", "control");
}

// ===== BuiltinRegistry 默认内建函数 =====
void BuiltinRegistry::register_defaults() {
    {
        BuiltinInfo info;
        info.name = "print";
        info.min_arity = 0;
        info.max_arity = -1;
        info.return_type = "void";
        info.eval_fn = [](std::vector<Value>& args, std::ostream* out, std::istream*, const SourceRange&) -> Value {
            if (args.empty()) (*out) << std::endl;
            else {
                (*out) << args[0].to_display();
                for (size_t i = 1; i < args.size(); ++i) (*out) << " " << args[i].to_display();
                (*out) << std::endl;
            }
            return Value::make_null();
        };
        register_builtin(info);
    }
    {
        BuiltinInfo info;
        info.name = "println";
        info.min_arity = 0;
        info.max_arity = -1;
        info.return_type = "void";
        info.eval_fn = [](std::vector<Value>& args, std::ostream* out, std::istream*, const SourceRange&) -> Value {
            if (args.empty()) (*out) << std::endl;
            else {
                (*out) << args[0].to_display();
                for (size_t i = 1; i < args.size(); ++i) (*out) << " " << args[i].to_display();
                (*out) << std::endl;
            }
            return Value::make_null();
        };
        register_builtin(info);
    }
    {
        BuiltinInfo info;
        info.name = "input";
        info.min_arity = 0;
        info.max_arity = 0;
        info.return_type = "string";
        info.eval_fn = [](std::vector<Value>&, std::ostream*, std::istream* in, const SourceRange&) -> Value {
            std::string line;
            std::getline(*in, line);
            return Value::make_string(line);
        };
        register_builtin(info);
    }
    {
        BuiltinInfo info;
        info.name = "len";
        info.min_arity = 1;
        info.max_arity = 1;
        info.return_type = "int";
        info.eval_fn = [](std::vector<Value>& args, std::ostream*, std::istream*, const SourceRange&) -> Value {
            if (args.empty()) return Value::make_int(0);
            if (args[0].is_array()) return Value::make_int(static_cast<int64_t>(args[0].as_array().size()));
            if (args[0].is_string()) return Value::make_int(static_cast<int64_t>(args[0].as_string().size()));
            if (args[0].is_dict()) return Value::make_int(static_cast<int64_t>(args[0].as_dict().entries.size()));
            if (args[0].is_set()) return Value::make_int(static_cast<int64_t>(args[0].as_set().elements.size()));
            if (args[0].is_tuple()) return Value::make_int(static_cast<int64_t>(args[0].as_tuple().size()));
            throw std::runtime_error("RUN130: len 不支持的类型: " + args[0].type_name());
        };
        register_builtin(info);
    }
    {
        BuiltinInfo info;
        info.name = "type";
        info.min_arity = 1;
        info.max_arity = 1;
        info.return_type = "string";
        info.eval_fn = [](std::vector<Value>& args, std::ostream*, std::istream*, const SourceRange&) -> Value {
            if (args.empty()) return Value::make_string("null");
            return Value::make_string(args[0].type_name());
        };
        register_builtin(info);
    }

    // 注册100个内置函数（数学/字符串/数组/类型/IO）
    register_all_builtins(*this);
    // 注册100个标准库工具（math_/str_/arr_/dict_/set_/time_/util_）
    register_all_stdlib_tools(*this);
    // 注册 GUI 函数
    register_all_gui();
}

// ===== TypeRegistry 默认类型 =====
void TypeRegistry::register_defaults() {
    auto reg = [this](const std::string& n, BaseType b) {
        TypeInfo info;
        info.name = n;
        info.kind = TypeKind::Basic;
        info.type = Type::make_basic(b);
        info.is_builtin_type = true;
        register_type(info);
    };
    reg("int", BaseType::Int);
    reg("float", BaseType::Float);
    reg("string", BaseType::String);
    reg("bool", BaseType::Bool);
    reg("void", BaseType::Void);
}

// ===== ConversionRegistry 默认转换 =====
void ConversionRegistry::register_defaults() {
    {
        ConversionInfo info;
        info.from_type = "int";
        info.to_type = "float";
        info.is_implicit = true;
        info.convert_fn = [](const Value& v) -> Value {
            return Value::make_float(static_cast<double>(v.as_int()));
        };
        register_conversion(info);
    }
    {
        ConversionInfo info;
        info.from_type = "int";
        info.to_type = "string";
        info.is_implicit = false;
        info.convert_fn = [](const Value& v) -> Value {
            return Value::make_string(std::to_string(v.as_int()));
        };
        register_conversion(info);
    }
    {
        ConversionInfo info;
        info.from_type = "float";
        info.to_type = "string";
        info.is_implicit = false;
        info.convert_fn = [](const Value& v) -> Value {
            std::ostringstream oss;
            oss << v.as_float();
            return Value::make_string(oss.str());
        };
        register_conversion(info);
    }
    {
        ConversionInfo info;
        info.from_type = "bool";
        info.to_type = "string";
        info.is_implicit = false;
        info.convert_fn = [](const Value& v) -> Value {
            return Value::make_string(v.as_bool() ? "true" : "false");
        };
        register_conversion(info);
    }
    {
        ConversionInfo info;
        info.from_type = "float";
        info.to_type = "int";
        info.is_implicit = false;
        info.convert_fn = [](const Value& v) -> Value {
            double d = v.as_float();
            if (std::isnan(d) || std::isinf(d)) throw std::runtime_error("RUN362: float->int 转换: NaN/Inf");
            if (d >= 9.2233720368547758e18 || d < -9.2233720368547758e18) throw std::runtime_error("RUN363: float->int 转换: 超出 int64 范围");
            return Value::make_int(static_cast<int64_t>(d));
        };
        register_conversion(info);
    }
}

} // namespace next11