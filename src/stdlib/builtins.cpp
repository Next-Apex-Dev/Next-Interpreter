// 100个内置函数注册实现
#include "common/registry.hpp"
#include "interp/sandbox.hpp"
#include "interp/file_io.hpp"
#include "interp/event_loop.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <random>
#include <chrono>
#include <thread>
#include <cstdint>

namespace next11 {

using BuiltinFn = std::function<Value(std::vector<Value>&, std::ostream*, std::istream*, const SourceRange&)>;

static inline void rb(BuiltinRegistry& reg, const char* name, int min_a, int max_a,
                      const char* ret_type, BuiltinFn fn) {
    BuiltinInfo info;
    info.name = name;
    info.min_arity = min_a;
    info.max_arity = max_a;
    info.return_type = ret_type;
    info.eval_fn = std::move(fn);
    reg.register_builtin(info);
}

// 全局沙箱校验器（默认允许当前工作目录）
inline SandboxChecker& global_sandbox() {
    static SandboxChecker sb;
    return sb;
}
// 全局文件 I/O 实例
inline FileIO& global_file_io() {
    static FileIO fio(&global_sandbox());
    return fio;
}

void register_all_builtins(BuiltinRegistry& reg) {

// ===== 数学函数（30个）=====
rb(reg, "abs", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (a[0].is_complex()) {
        const auto& c = a[0].as_complex();
        return Value::make_float(std::sqrt(c.real * c.real + c.imag * c.imag));
    }
    if (a[0].is_int()) {
        int64_t v = a[0].as_int();
        if (v == INT64_MIN) throw std::runtime_error("RUN356: abs(INT64_MIN) 溢出");
        return Value::make_int(std::llabs(v));
    }
    if (!a[0].is_float()) throw std::runtime_error("RUN229: abs 期望数值参数");
    return Value::make_float(std::fabs(a[0].as_number()));
});
rb(reg, "real", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (a[0].is_complex()) {
        return Value::make_float(a[0].as_complex().real);
    }
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN248: real 期望数值或复数参数");
    return Value::make_float(a[0].as_number());
});
rb(reg, "imag", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (a[0].is_complex()) {
        return Value::make_float(a[0].as_complex().imag);
    }
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN249: imag 期望数值或复数参数");
    return Value::make_float(0.0);
});
rb(reg, "conj", 1, 1, "complex", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (a[0].is_complex()) {
        const auto& c = a[0].as_complex();
        return Value::make_complex(c.real, -c.imag);
    }
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN250: conj 期望数值或复数参数");
    return a[0];
});
rb(reg, "min", 2, -1, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    for (auto& v : a) if (!v.is_int() && !v.is_float()) throw std::runtime_error("RUN230: min 期望数值参数");
    Value best = a[0];
    for (size_t i = 1; i < a.size(); ++i) if (a[i].as_number() < best.as_number()) best = a[i];
    return best;
});
rb(reg, "max", 2, -1, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    for (auto& v : a) if (!v.is_int() && !v.is_float()) throw std::runtime_error("RUN231: max 期望数值参数");
    Value best = a[0];
    for (size_t i = 1; i < a.size(); ++i) if (a[i].as_number() > best.as_number()) best = a[i];
    return best;
});
rb(reg, "floor", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN232: floor 期望数值参数");
    return Value::make_float(std::floor(a[0].as_number())); });
rb(reg, "ceil", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN233: ceil 期望数值参数");
    return Value::make_float(std::ceil(a[0].as_number())); });
rb(reg, "round", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN234: round 期望数值参数");
    return Value::make_float(std::round(a[0].as_number())); });
rb(reg, "sqrt", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN235: sqrt 期望数值参数");
    if (a[0].as_number() < 0) throw std::runtime_error("RUN248: sqrt 参数不能为负数");
    return Value::make_float(std::sqrt(a[0].as_number())); });
rb(reg, "pow", 2, 2, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN236: pow 期望数值参数");
    if (!a[1].is_int() && !a[1].is_float()) throw std::runtime_error("RUN251: pow 第二参数期望数值");
    return Value::make_float(std::pow(a[0].as_number(), a[1].as_number())); });
rb(reg, "sin", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN237: sin 期望数值参数");
    return Value::make_float(std::sin(a[0].as_number())); });
rb(reg, "cos", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN238: cos 期望数值参数");
    return Value::make_float(std::cos(a[0].as_number())); });
rb(reg, "tan", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN239: tan 期望数值参数");
    return Value::make_float(std::tan(a[0].as_number())); });
rb(reg, "log", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN240: log 期望数值参数");
    if (a[0].as_number() <= 0) throw std::runtime_error("RUN249: log 参数必须为正数");
    return Value::make_float(std::log(a[0].as_number())); });
rb(reg, "exp", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN241: exp 期望数值参数");
    return Value::make_float(std::exp(a[0].as_number())); });
rb(reg, "atan", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN242: atan 期望数值参数");
    return Value::make_float(std::atan(a[0].as_number())); });
rb(reg, "asin", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN243: asin 期望数值参数");
    if (a[0].as_number() < -1.0 || a[0].as_number() > 1.0) throw std::runtime_error("RUN250: asin 参数必须在 [-1, 1] 范围内");
    return Value::make_float(std::asin(a[0].as_number())); });
rb(reg, "acos", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN244: acos 期望数值参数");
    if (a[0].as_number() < -1.0 || a[0].as_number() > 1.0) throw std::runtime_error("RUN251: acos 参数必须在 [-1, 1] 范围内");
    return Value::make_float(std::acos(a[0].as_number())); });
rb(reg, "atan2", 2, 2, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN245: atan2 期望数值参数");
    if (!a[1].is_int() && !a[1].is_float()) throw std::runtime_error("RUN252: atan2 第二参数期望数值");
    return Value::make_float(std::atan2(a[0].as_number(), a[1].as_number())); });
rb(reg, "log2", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN246: log2 期望数值参数");
    if (a[0].as_number() <= 0) throw std::runtime_error("RUN254: log2 参数必须为正数");
    return Value::make_float(std::log2(a[0].as_number())); });
rb(reg, "log10", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN247: log10 期望数值参数");
    if (a[0].as_number() <= 0) throw std::runtime_error("RUN255: log10 参数必须为正数");
    return Value::make_float(std::log10(a[0].as_number())); });
rb(reg, "sign", 1, 1, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN253: sign 期望数值参数");
    double v = a[0].as_number();
    if (v > 0) return Value::make_int(1);
    if (v < 0) return Value::make_int(-1);
    return Value::make_int(0);
});
rb(reg, "clamp", 3, 3, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN254: clamp 第一参数期望数值");
    if (!a[1].is_int() && !a[1].is_float()) throw std::runtime_error("RUN255: clamp 第二参数期望数值");
    if (!a[2].is_int() && !a[2].is_float()) throw std::runtime_error("RUN256: clamp 第三参数期望数值");
    double v = a[0].as_number(), lo = a[1].as_number(), hi = a[2].as_number();
    if (v < lo) return a[1];
    if (v > hi) return a[2];
    return a[0];
});
rb(reg, "gcd", 2, 2, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() || !a[1].is_int()) throw std::runtime_error("RUN037: gcd 期望整数参数");
    int64_t xv = a[0].as_int(), yv = a[1].as_int();
    if (xv == INT64_MIN || yv == INT64_MIN) throw std::runtime_error("RUN037: gcd 参数不能为 INT64_MIN");
    int64_t x = std::llabs(xv), y = std::llabs(yv);
    while (y != 0) { int64_t t = y; y = x % y; x = t; }
    return Value::make_int(x);
});
rb(reg, "lcm", 2, 2, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() || !a[1].is_int()) throw std::runtime_error("RUN038: lcm 期望整数参数");
    int64_t xv = a[0].as_int(), yv = a[1].as_int();
    if (xv == INT64_MIN || yv == INT64_MIN) throw std::runtime_error("RUN038: lcm 参数不能为 INT64_MIN");
    int64_t x = std::llabs(xv), y = std::llabs(yv);
    if (x == 0 || y == 0) return Value::make_int(0);
    int64_t g = x, yy = y;
    while (yy != 0) { int64_t t = yy; yy = g % yy; g = t; }
    int64_t q = x / g;
    if (q > INT64_MAX / y) throw std::runtime_error("RUN038: lcm 结果溢出 int64");
    return Value::make_int(q * y);
});
rb(reg, "hypot", 2, 2, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN257: hypot 第一参数期望数值");
    if (!a[1].is_int() && !a[1].is_float()) throw std::runtime_error("RUN258: hypot 第二参数期望数值");
    return Value::make_float(std::hypot(a[0].as_number(), a[1].as_number()));
});
rb(reg, "deg2rad", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN259: deg2rad 期望数值参数");
    return Value::make_float(a[0].as_number() * 3.14159265358979323846 / 180.0);
});
rb(reg, "rad2deg", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN260: rad2deg 期望数值参数");
    return Value::make_float(a[0].as_number() * 180.0 / 3.14159265358979323846);
});

rb(reg, "factorial", 1, 1, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int()) throw std::runtime_error("RUN039: factorial 期望整数参数");
    int64_t n = a[0].as_int();
    if (n < 0) throw std::runtime_error("RUN008: factorial 参数不能为负数");
    int64_t r = 1;
    for (int64_t i = 2; i <= n; ++i) {
        if (r > INT64_MAX / i) throw std::runtime_error("RUN009: factorial 结果溢出 int64");
        r *= i;
    }
    return Value::make_int(r);
});
rb(reg, "fibonacci", 1, 1, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int()) throw std::runtime_error("RUN040: fibonacci 期望整数参数");
    int64_t n = a[0].as_int();
    if (n <= 0) return Value::make_int(0);
    if (n == 1) return Value::make_int(1);
    int64_t a1 = 0, b = 1;
    for (int64_t i = 2; i <= n; ++i) {
        if (a1 > INT64_MAX - b) throw std::runtime_error("RUN010: fibonacci 结果溢出 int64");
        int64_t t = a1 + b; a1 = b; b = t;
    }
    return Value::make_int(b);
});
rb(reg, "is_prime", 1, 1, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int()) throw std::runtime_error("RUN261: is_prime 期望整数参数");
    int64_t n = a[0].as_int();
    if (n < 2) return Value::make_bool(false);
    for (int64_t i = 2; i <= n / i; ++i) if (n % i == 0) return Value::make_bool(false);
    return Value::make_bool(true);
});
rb(reg, "pi", 0, 0, "float", [](std::vector<Value>&, std::ostream*, std::istream*, const SourceRange&) -> Value {
    return Value::make_float(3.14159265358979323846);
});
rb(reg, "complex", 2, 2, "complex", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN262: complex 第一参数期望数值");
    if (!a[1].is_int() && !a[1].is_float()) throw std::runtime_error("RUN263: complex 第二参数期望数值");
    double real = a[0].as_number();
    double imag = a[1].as_number();
    return Value::make_complex(real, imag);
});
rb(reg, "polar", 2, 2, "complex", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN264: polar 第一参数期望数值");
    if (!a[1].is_int() && !a[1].is_float()) throw std::runtime_error("RUN265: polar 第二参数期望数值");
    double rho = a[0].as_number();
    double theta = a[1].as_number();
    return Value::make_complex(rho * std::cos(theta), rho * std::sin(theta));
});

// ===== 字符串函数（25个）=====
rb(reg, "str", 1, 1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    return Value::make_string(a[0].to_display());
});
rb(reg, "concat", 2, -1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    std::string r;
    for (auto& v : a) r += v.to_display();
    return Value::make_string(r);
});
rb(reg, "split", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN133: split 期望字符串参数");
    if (!a[1].is_string()) throw std::runtime_error("RUN266: split 第二参数期望字符串");
    std::string s = a[0].as_string(), delim = a[1].as_string();
    if (delim.empty()) throw std::runtime_error("RUN266: split 分隔符不能为空");
    std::vector<Value> parts;
    size_t start = 0, pos;
    while ((pos = s.find(delim, start)) != std::string::npos) {
        parts.push_back(Value::make_string(s.substr(start, pos - start)));
        start = pos + delim.size();
    }
    parts.push_back(Value::make_string(s.substr(start)));
    return Value::make_array(std::move(parts));
});
rb(reg, "join", 2, 2, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN056: join 期望数组参数");
    if (!a[1].is_string()) throw std::runtime_error("RUN134: join 第二个参数期望字符串");
    auto& arr = a[0].as_array();
    std::string sep = a[1].as_string(), r;
    for (size_t i = 0; i < arr.size(); ++i) { if (i) r += sep; r += arr[i].to_display(); }
    return Value::make_string(r);
});
rb(reg, "replace", 3, 3, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN135: replace 期望字符串参数");
    if (!a[1].is_string()) throw std::runtime_error("RUN267: replace 第二参数期望字符串");
    if (!a[2].is_string()) throw std::runtime_error("RUN268: replace 第三参数期望字符串");
    std::string s = a[0].as_string(), from = a[1].as_string(), to = a[2].as_string();
    if (from.empty()) throw std::runtime_error("RUN267: replace 搜索串不能为空");
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) { s.replace(pos, from.size(), to); pos += to.size(); }
    return Value::make_string(s);
});
rb(reg, "substring", 2, 3, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN136: substring 期望字符串参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN269: substring 第二参数期望整数");
    if (a.size() > 2 && !a[2].is_int()) throw std::runtime_error("RUN270: substring 第三参数期望整数");
    std::string s = a[0].as_string();
    int64_t start = a[1].as_int();
    if (start < 0) start = 0;
    if (start >= static_cast<int64_t>(s.size())) return Value::make_string("");
    int64_t end = a.size() > 2 ? a[2].as_int() : static_cast<int64_t>(s.size());
    if (end > static_cast<int64_t>(s.size())) end = static_cast<int64_t>(s.size());
    if (end < start) end = start;
    return Value::make_string(s.substr(static_cast<size_t>(start), static_cast<size_t>(end - start)));
});
rb(reg, "upper", 1, 1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN137: upper 期望字符串参数");
    std::string s = a[0].as_string();
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return Value::make_string(s);
});
rb(reg, "lower", 1, 1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN138: lower 期望字符串参数");
    std::string s = a[0].as_string();
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return Value::make_string(s);
});
rb(reg, "trim", 1, 1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN139: trim 期望字符串参数");
    std::string s = a[0].as_string();
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) return Value::make_string("");
    return Value::make_string(s.substr(start, end - start + 1));
});
rb(reg, "starts_with", 2, 2, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN140: starts_with 期望字符串参数");
    if (!a[1].is_string()) throw std::runtime_error("RUN271: starts_with 第二参数期望字符串");
    std::string s = a[0].as_string(), prefix = a[1].as_string();
    return Value::make_bool(s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0);
});
rb(reg, "ends_with", 2, 2, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN141: ends_with 期望字符串参数");
    if (!a[1].is_string()) throw std::runtime_error("RUN272: ends_with 第二参数期望字符串");
    std::string s = a[0].as_string(), suffix = a[1].as_string();
    return Value::make_bool(s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0);
});
rb(reg, "contains", 2, 2, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (a[0].is_string()) {
        if (!a[1].is_string()) throw std::runtime_error("RUN273: contains 对字符串操作时第二参数期望字符串");
        std::string s = a[0].as_string(), sub = a[1].as_string(); return Value::make_bool(s.find(sub) != std::string::npos);
    }
    if (a[0].is_array()) { for (auto& v : a[0].as_array()) if (v.equals(a[1])) return Value::make_bool(true); return Value::make_bool(false); }
    if (a[0].is_set()) { for (auto& v : a[0].as_set().elements) if (v.equals(a[1])) return Value::make_bool(true); return Value::make_bool(false); }
    if (a[0].is_dict()) { for (auto& kv : a[0].as_dict().entries) if (kv.first.equals(a[1])) return Value::make_bool(true); return Value::make_bool(false); }
    throw std::runtime_error("RUN142: contains 不支持的类型: " + a[0].type_name());
});
rb(reg, "index_of", 2, 2, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN143: index_of 期望字符串参数");
    if (!a[1].is_string()) throw std::runtime_error("RUN274: index_of 第二参数期望字符串");
    std::string s = a[0].as_string(), sub = a[1].as_string();
    size_t pos = s.find(sub);
    return Value::make_int(pos == std::string::npos ? -1 : static_cast<int64_t>(pos));
});
rb(reg, "char_at", 2, 2, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN144: char_at 期望字符串参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN275: char_at 第二参数期望整数");
    std::string s = a[0].as_string();
    int64_t idx = a[1].as_int();
    if (idx < 0 || idx >= static_cast<int64_t>(s.size())) return Value::make_string("");
    return Value::make_string(std::string(1, s[static_cast<size_t>(idx)]));
});
rb(reg, "reverse_str", 1, 1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN145: reverse_str 期望字符串参数");
    std::string s = a[0].as_string();
    std::vector<size_t> boundaries;
    for (size_t i = 0; i < s.size();) {
        boundaries.push_back(i);
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) i += 1;
        else if ((c & 0xE0) == 0xC0) i += 2;
        else if ((c & 0xF0) == 0xE0) i += 3;
        else if ((c & 0xF8) == 0xF0) i += 4;
        else i += 1;
    }
    std::string r;
    r.reserve(s.size());
    for (size_t k = boundaries.size(); k > 0; --k) {
        size_t start = boundaries[k - 1];
        size_t end = (k < boundaries.size()) ? boundaries[k] : s.size();
        r.append(s, start, end - start);
    }
    return Value::make_string(r);
});
rb(reg, "repeat_str", 2, 2, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN146: repeat_str 期望字符串参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN276: repeat_str 第二参数期望整数");
    std::string s = a[0].as_string();
    int64_t n = a[1].as_int();
    if (n < 0) throw std::runtime_error("RUN365: repeat_str 重复次数不能为负数");
    std::string r;
    r.reserve(static_cast<size_t>(n) * s.size());
    for (int64_t i = 0; i < n; ++i) r += s;
    return Value::make_string(r);
});
rb(reg, "pad_left", 3, 3, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN147: pad_left 期望字符串参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN277: pad_left 第二参数期望整数");
    if (!a[2].is_string()) throw std::runtime_error("RUN278: pad_left 第三参数期望字符串");
    std::string s = a[0].as_string();
    int64_t width = a[1].as_int();
    char pad = a[2].as_string().empty() ? ' ' : a[2].as_string()[0];
    if (width > static_cast<int64_t>(s.size())) {
        std::string padded(static_cast<size_t>(width - static_cast<int64_t>(s.size())), pad);
        padded += s;
        return Value::make_string(padded);
    }
    return Value::make_string(s);
});
rb(reg, "pad_right", 3, 3, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN148: pad_right 期望字符串参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN279: pad_right 第二参数期望整数");
    if (!a[2].is_string()) throw std::runtime_error("RUN280: pad_right 第三参数期望字符串");
    std::string s = a[0].as_string();
    int64_t width = a[1].as_int();
    char pad = a[2].as_string().empty() ? ' ' : a[2].as_string()[0];
    if (width > static_cast<int64_t>(s.size())) {
        s.append(static_cast<size_t>(width - static_cast<int64_t>(s.size())), pad);
    }
    return Value::make_string(s);
});
rb(reg, "format", 1, -1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN149: format 期望字符串参数");
    std::string fmt = a[0].as_string();
    std::string result;
    size_t arg_idx = 1;
    for (size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] == '{' && i + 1 < fmt.size() && fmt[i+1] == '}') {
            if (arg_idx >= a.size()) throw std::runtime_error("RUN347: format 参数不足：占位符 " + std::to_string(arg_idx) + " 无对应参数");
            result += a[arg_idx].to_display();
            arg_idx++; i++;
        } else { result += fmt[i]; }
    }
    return Value::make_string(result);
});
rb(reg, "parse_int", 1, 1, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN281: parse_int 期望字符串参数");
    try { return Value::make_int(std::stoll(a[0].as_string())); }
    catch (...) { throw std::runtime_error("RUN009: parse_int 无法解析字符串为整数: " + a[0].as_string()); }
});
rb(reg, "parse_float", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN282: parse_float 期望字符串参数");
    try { return Value::make_float(std::stod(a[0].as_string())); }
    catch (...) { throw std::runtime_error("RUN010: parse_float 无法解析字符串为浮点数: " + a[0].as_string()); }
});
rb(reg, "to_hex", 1, 1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int()) throw std::runtime_error("RUN044: to_hex 期望整数参数");
    std::ostringstream oss;
    oss << std::hex << a[0].as_int();
    return Value::make_string(oss.str());
});
rb(reg, "from_hex", 1, 1, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN283: from_hex 期望字符串参数");
    try { return Value::make_int(std::stoll(a[0].as_string(), nullptr, 16)); }
    catch (...) { throw std::runtime_error("RUN011: from_hex 无法解析十六进制字符串: " + a[0].as_string()); }
});
rb(reg, "is_digit", 1, 1, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN284: is_digit 期望字符串参数");
    std::string s = a[0].as_string();
    if (s.empty()) return Value::make_bool(false);
    for (auto c : s) if (!std::isdigit(static_cast<unsigned char>(c))) return Value::make_bool(false);
    return Value::make_bool(true);
});
rb(reg, "is_alpha", 1, 1, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN285: is_alpha 期望字符串参数");
    std::string s = a[0].as_string();
    if (s.empty()) return Value::make_bool(false);
    for (auto c : s) if (!std::isalpha(static_cast<unsigned char>(c))) return Value::make_bool(false);
    return Value::make_bool(true);
});

// ===== 数组函数（25个）=====
rb(reg, "push", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN057: push 期望数组参数");
    a[0].as_array().push_back(a[1]);
    return a[0];
});
rb(reg, "pop", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN058: pop 期望数组参数");
    auto& arr = a[0].as_array();
    if (!arr.empty()) arr.pop_back();
    return a[0];
});
rb(reg, "shift", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN059: shift 期望数组参数");
    auto& arr = a[0].as_array();
    if (!arr.empty()) arr.erase(arr.begin());
    return a[0];
});
rb(reg, "unshift", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN060: unshift 期望数组参数");
    a[0].as_array().insert(a[0].as_array().begin(), a[1]);
    return a[0];
});
rb(reg, "sort", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN061: sort 期望数组参数");
    auto& arr = a[0].as_array();
    bool all_num = true, all_str = true;
    for (auto& v : arr) {
        if (!v.is_int() && !v.is_float()) all_num = false;
        if (!v.is_string()) all_str = false;
    }
    if (all_num) {
        std::sort(arr.begin(), arr.end(), [](const Value& x, const Value& y) { return x.as_number() < y.as_number(); });
    } else if (all_str) {
        std::sort(arr.begin(), arr.end(), [](const Value& x, const Value& y) { return x.as_string() < y.as_string(); });
    } else {
        throw std::runtime_error("RUN333: sort 数组元素必须全为数值或全为字符串");
    }
    return a[0];
});
rb(reg, "reverse_arr", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN062: reverse_arr 期望数组参数");
    auto& arr = a[0].as_array();
    std::reverse(arr.begin(), arr.end());
    return a[0];
});
rb(reg, "slice", 2, 3, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN063: slice 期望数组参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN286: slice 第二参数期望整数");
    if (a.size() > 2 && !a[2].is_int()) throw std::runtime_error("RUN287: slice 第三参数期望整数");
    auto& arr = a[0].as_array();
    int64_t start = a[1].as_int();
    int64_t end = a.size() > 2 ? a[2].as_int() : static_cast<int64_t>(arr.size());
    if (start < 0) start = 0;
    if (start > static_cast<int64_t>(arr.size())) start = static_cast<int64_t>(arr.size());
    if (end > static_cast<int64_t>(arr.size())) end = static_cast<int64_t>(arr.size());
    if (end < start) end = start;
    std::vector<Value> r(arr.begin() + static_cast<size_t>(start), arr.begin() + static_cast<size_t>(end));
    return Value::make_array(std::move(r));
});
rb(reg, "concat_arr", 2, -1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN064: concat_arr 期望数组参数");
    auto result = a[0].as_array();
    for (size_t i = 1; i < a.size(); ++i) {
        if (!a[i].is_array()) throw std::runtime_error(std::string("RUN337: concat_arr 参数 ") + std::to_string(i+1) + " 期望数组");
        auto& more = a[i].as_array();
        result.insert(result.end(), more.begin(), more.end());
    }
    return Value::make_array(std::move(result));
});
rb(reg, "fill", 2, 3, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int()) throw std::runtime_error("RUN288: fill 第一参数期望整数");
    int64_t count = a[0].as_int();
    Value val = a[1];
    std::vector<Value> r;
    for (int i = 0; i < count; ++i) r.push_back(val);
    return Value::make_array(std::move(r));
});
rb(reg, "range", 1, 3, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int()) throw std::runtime_error("RUN289: range 参数期望整数");
    if (a.size() >= 2 && !a[1].is_int()) throw std::runtime_error("RUN290: range 第二参数期望整数");
    if (a.size() >= 3 && !a[2].is_int()) throw std::runtime_error("RUN291: range 第三参数期望整数");
    int64_t start = 0, end = a[0].as_int(), step = 1;
    if (a.size() >= 2) { start = a[0].as_int(); end = a[1].as_int(); }
    if (a.size() >= 3) step = a[2].as_int();
    if (step == 0) throw std::runtime_error("RUN338: range 步长不能为零");
    std::vector<Value> r;
    if (step > 0) { for (int64_t i = start; i < end; i += step) r.push_back(Value::make_int(i)); }
    else { for (int64_t i = start; i > end; i += step) r.push_back(Value::make_int(i)); }
    return Value::make_array(std::move(r));
});
rb(reg, "zip", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN065: zip 期望数组参数");
    if (!a[1].is_array()) throw std::runtime_error("RUN065: zip 第二个参数也期望数组");
    auto& a1 = a[0].as_array();
    auto& a2 = a[1].as_array();
    size_t n = std::min(a1.size(), a2.size());
    std::vector<Value> r;
    for (size_t i = 0; i < n; ++i) {
        std::vector<Value> pair = {a1[i], a2[i]};
        r.push_back(Value::make_tuple(std::move(pair)));
    }
    return Value::make_array(std::move(r));
});
rb(reg, "flatten", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN066: flatten 期望数组参数");
    auto& arr = a[0].as_array();
    std::vector<Value> r;
    for (auto& v : arr) {
        if (v.is_array()) { auto& inner = v.as_array(); r.insert(r.end(), inner.begin(), inner.end()); }
        else r.push_back(v);
    }
    return Value::make_array(std::move(r));
});
rb(reg, "count", 2, 2, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN067: count 期望数组参数");
    auto& arr = a[0].as_array();
    int64_t cnt = 0;
    for (auto& v : arr) if (v.equals(a[1])) cnt++;
    return Value::make_int(cnt);
});
rb(reg, "sum", 1, 1, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN055: sum 期望数组参数");
    auto& arr = a[0].as_array();
    bool all_int = true;
    int64_t int_sum = 0;
    double float_sum = 0;
    for (auto& v : arr) {
        if (v.is_int()) {
            int64_t val = v.as_int();
            if ((val > 0 && int_sum > INT64_MAX - val) || (val < 0 && int_sum < INT64_MIN - val))
                throw std::runtime_error("RUN339: sum 整数累加溢出");
            int_sum += val; float_sum += static_cast<double>(val);
        }
        else if (v.is_float()) { all_int = false; float_sum += v.as_float(); }
        else throw std::runtime_error("RUN339: sum 数组元素必须为数值");
    }
    if (all_int) return Value::make_int(int_sum);
    return Value::make_float(float_sum);
});
rb(reg, "avg", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN068: avg 期望数组参数");
    auto& arr = a[0].as_array();
    if (arr.empty()) throw std::runtime_error("RUN012: avg 空数组没有平均值");
    double s = 0;
    for (auto& v : arr) {
        if (!v.is_int() && !v.is_float()) throw std::runtime_error("RUN340: avg 数组元素必须为数值");
        s += v.as_number();
    }
    return Value::make_float(s / arr.size());
});
rb(reg, "min_arr", 1, 1, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN069: min_arr 期望数组参数");
    auto& arr = a[0].as_array();
    if (arr.empty()) throw std::runtime_error("RUN013: min_arr 空数组没有最小值");
    for (auto& v : arr) if (!v.is_int() && !v.is_float()) throw std::runtime_error("RUN341: min_arr 数组元素必须为数值");
    Value best = arr[0];
    for (size_t i = 1; i < arr.size(); ++i) if (arr[i].as_number() < best.as_number()) best = arr[i];
    return best;
});
rb(reg, "max_arr", 1, 1, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN070: max_arr 期望数组参数");
    auto& arr = a[0].as_array();
    if (arr.empty()) throw std::runtime_error("RUN014: max_arr 空数组没有最大值");
    for (auto& v : arr) if (!v.is_int() && !v.is_float()) throw std::runtime_error("RUN342: max_arr 数组元素必须为数值");
    Value best = arr[0];
    for (size_t i = 1; i < arr.size(); ++i) if (arr[i].as_number() > best.as_number()) best = arr[i];
    return best;
});
rb(reg, "distinct", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN071: distinct 期望数组参数");
    auto& arr = a[0].as_array();
    std::vector<Value> r;
    for (auto& v : arr) {
        bool found = false;
        for (auto& existing : r) if (existing.equals(v)) { found = true; break; }
        if (!found) r.push_back(v);
    }
    return Value::make_array(std::move(r));
});
rb(reg, "take", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN072: take 期望数组参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN293: take 第二参数期望整数");
    auto& arr = a[0].as_array();
    int64_t n = a[1].as_int();
    if (n < 0) n = 0;
    if (n > static_cast<int64_t>(arr.size())) n = static_cast<int64_t>(arr.size());
    std::vector<Value> r(arr.begin(), arr.begin() + static_cast<size_t>(n));
    return Value::make_array(std::move(r));
});
rb(reg, "drop", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN073: drop 期望数组参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN294: drop 第二参数期望整数");
    auto& arr = a[0].as_array();
    int64_t n = a[1].as_int();
    if (n < 0) n = 0;
    if (n > static_cast<int64_t>(arr.size())) n = static_cast<int64_t>(arr.size());
    std::vector<Value> r(arr.begin() + static_cast<size_t>(n), arr.end());
    return Value::make_array(std::move(r));
});
rb(reg, "chunk", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN074: chunk 期望数组参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN295: chunk 第二参数期望整数");
    auto& arr = a[0].as_array();
    int64_t size = a[1].as_int();
    if (size <= 0) throw std::runtime_error("RUN295: chunk 块大小必须为正数");
    std::vector<Value> r;
    for (int64_t i = 0; i < static_cast<int64_t>(arr.size()); i += size) {
        int64_t end = std::min(i + size, static_cast<int64_t>(arr.size()));
        std::vector<Value> chunk(arr.begin() + static_cast<size_t>(i), arr.begin() + static_cast<size_t>(end));
        r.push_back(Value::make_array(std::move(chunk)));
    }
    return Value::make_array(std::move(r));
});
rb(reg, "interleave", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN075: interleave 期望数组参数");
    if (!a[1].is_array()) throw std::runtime_error("RUN075: interleave 第二个参数也期望数组");
    auto& a1 = a[0].as_array();
    auto& a2 = a[1].as_array();
    std::vector<Value> r;
    size_t n = std::max(a1.size(), a2.size());
    for (size_t i = 0; i < n; ++i) {
        if (i < a1.size()) r.push_back(a1[i]);
        if (i < a2.size()) r.push_back(a2[i]);
    }
    return Value::make_array(std::move(r));
});
rb(reg, "rotate", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN076: rotate 期望数组参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN292: rotate 第二参数期望整数");
    auto arr = a[0].as_array();
    int64_t k = a[1].as_int();
    if (arr.empty()) return Value::make_array(std::move(arr));
    int64_t n = static_cast<int64_t>(arr.size());
    k = ((k % n) + n) % n;
    std::vector<Value> r;
    for (int i = 0; i < n; ++i) r.push_back(arr[(i + k) % n]);
    return Value::make_array(std::move(r));
});
rb(reg, "first", 1, 1, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN077: first 期望数组参数");
    auto& arr = a[0].as_array();
    if (arr.empty()) throw std::runtime_error("RUN015: first 空数组没有第一个元素");
    return arr[0];
});
rb(reg, "last", 1, 1, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN078: last 期望数组参数");
    auto& arr = a[0].as_array();
    if (arr.empty()) throw std::runtime_error("RUN016: last 空数组没有最后一个元素");
    return arr.back();
});

// ===== 类型/转换函数（10个）=====
rb(reg, "to_int", 1, 1, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (a[0].is_int()) return a[0];
    if (a[0].is_float()) {
        double d = a[0].as_float();
        if (std::isnan(d) || std::isinf(d)) throw std::runtime_error("RUN343: to_int NaN/Inf 无法转为整数");
        if (d >= 9.2233720368547758e18 || d < -9.2233720368547758e18) throw std::runtime_error("RUN344: to_int 浮点数超出 int64 范围");
        return Value::make_int(static_cast<int64_t>(d));
    }
    if (a[0].is_string()) { try { return Value::make_int(std::stoll(a[0].as_string())); } catch (...) { throw std::runtime_error("RUN017: to_int 无法将字符串转为整数: " + a[0].as_string()); } }
    if (a[0].is_bool()) return Value::make_int(a[0].as_bool() ? 1 : 0);
    throw std::runtime_error("RUN018: to_int 不支持的类型转换");
});
rb(reg, "to_float", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (a[0].is_float()) return a[0];
    if (a[0].is_int()) return Value::make_float(static_cast<double>(a[0].as_int()));
    if (a[0].is_string()) { try { return Value::make_float(std::stod(a[0].as_string())); } catch (...) { throw std::runtime_error("RUN019: to_float 无法将字符串转为浮点数: " + a[0].as_string()); } }
    if (a[0].is_bool()) return Value::make_float(a[0].as_bool() ? 1.0 : 0.0);
    throw std::runtime_error("RUN020: to_float 不支持的类型转换");
});
rb(reg, "to_string", 1, 1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    return Value::make_string(a[0].to_display());
});
rb(reg, "to_bool", 1, 1, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (a[0].is_bool()) return a[0];
    if (a[0].is_int()) return Value::make_bool(a[0].as_int() != 0);
    if (a[0].is_float()) return Value::make_bool(a[0].as_float() != 0.0);
    if (a[0].is_string()) return Value::make_bool(!a[0].as_string().empty());
    if (a[0].is_null()) return Value::make_bool(false);
    return Value::make_bool(true);
});
rb(reg, "is_int", 1, 1, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    return Value::make_bool(a[0].is_int());
});
rb(reg, "is_float", 1, 1, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    return Value::make_bool(a[0].is_float());
});
rb(reg, "is_string", 1, 1, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    return Value::make_bool(a[0].is_string());
});
rb(reg, "is_bool", 1, 1, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    return Value::make_bool(a[0].is_bool());
});
rb(reg, "is_array", 1, 1, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    return Value::make_bool(a[0].is_array());
});
rb(reg, "is_null", 1, 1, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    return Value::make_bool(a[0].is_null());
});

// type_of(x) -> string：返回类型名
rb(reg, "type_of", 1, 1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    auto& v = a[0];
    if (v.is_null()) return Value::make_string("null");
    if (v.is_int()) return Value::make_string("int");
    if (v.is_float()) return Value::make_string("float");
    if (v.is_string()) return Value::make_string("string");
    if (v.is_bool()) return Value::make_string("bool");
    if (v.is_array()) return Value::make_string("array");
    if (v.is_struct()) return Value::make_string(v.as_struct().struct_name);
    if (v.is_complex()) return Value::make_string("complex");
    if (v.is_dict()) return Value::make_string("dict");
    if (v.is_set()) return Value::make_string("set");
    if (v.is_tuple()) return Value::make_string("tuple");
    if (v.is_fnref()) return Value::make_string("function");
    if (v.is_class()) return Value::make_string("class");
    if (v.is_bound_method()) return Value::make_string("method");
    if (v.is_generator()) return Value::make_string("generator");
    if (v.is_coroutine()) return Value::make_string("coroutine");
    if (v.is_exception()) return Value::make_string("exception");
    if (v.is_bytes()) return Value::make_string("bytes");
    if (v.is_file_handle()) return Value::make_string("file");
    if (v.is_module()) return Value::make_string("module");
    return Value::make_string("unknown");
});

// hash(x) -> int：返回哈希值
rb(reg, "hash", 1, 1, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    auto& v = a[0];
    if (v.is_int()) return Value::make_int(std::hash<int64_t>{}(v.as_int()));
    if (v.is_float()) return Value::make_int(static_cast<int64_t>(std::hash<double>{}(v.as_float())));
    if (v.is_string()) return Value::make_int(static_cast<int64_t>(std::hash<std::string>{}(v.as_string())));
    if (v.is_bool()) return Value::make_int(v.as_bool() ? 1 : 0);
    if (v.is_null()) return Value::make_int(0);
    if (v.is_complex()) {
        auto& c = v.as_complex();
        return Value::make_int(static_cast<int64_t>(std::hash<double>{}(c.real) ^ (std::hash<double>{}(c.imag) << 1)));
    }
    if (v.is_array()) {
        uint64_t h = 0;
        for (auto& elem : v.as_array()) {
            h = h * 31 + (elem.is_int() ? std::hash<int64_t>{}(elem.as_int()) : std::hash<std::string>{}(elem.to_display()));
        }
        return Value::make_int(static_cast<int64_t>(h));
    }
    if (v.is_tuple()) {
        uint64_t h = 0;
        for (auto& elem : v.as_tuple()) {
            h = h * 31 + (elem.is_int() ? std::hash<int64_t>{}(elem.as_int()) : std::hash<std::string>{}(elem.to_display()));
        }
        return Value::make_int(static_cast<int64_t>(h));
    }
    if (v.is_struct()) {
        uint64_t h = std::hash<std::string>{}(v.as_struct().struct_name);
        for (auto& kv : v.as_struct().fields) {
            h = h * 31 + std::hash<std::string>{}(kv.first);
            h = h * 31 + std::hash<std::string>{}(kv.second.to_display());
        }
        return Value::make_int(static_cast<int64_t>(h));
    }
    throw std::runtime_error("RUN270: 不可哈希类型: " + v.type_name());
});

// ===== IO/其他函数（6个，加上已有的print/input/len/type共10个）=====
rb(reg, "print_err", 1, -1, "void", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    for (size_t i = 0; i < a.size(); ++i) { if (i) std::cerr << " "; std::cerr << a[i].to_display(); }
    std::cerr << std::endl;
    return Value::make_null();
});
rb(reg, "print_raw", 1, 1, "void", [](std::vector<Value>& a, std::ostream* out, std::istream*, const SourceRange&) -> Value {
    (*out) << a[0].to_display();
    return Value::make_null();
});
rb(reg, "assert", 1, 2, "void", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    bool truthy = a[0].truthy();
    if (!truthy) {
        std::string msg = "断言失败";
        if (a.size() > 1) {
            if (!a[1].is_string()) throw std::runtime_error("RUN296: assert 第二参数期望字符串");
            msg = a[1].as_string();
        }
        throw std::runtime_error("RUN006: " + msg);
    }
    return Value::make_null();
});
rb(reg, "error", 1, 1, "void", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN048: error 消息参数必须为字符串");
    throw std::runtime_error("RUN007: " + a[0].as_string());
});
rb(reg, "now", 0, 0, "int", [](std::vector<Value>&, std::ostream*, std::istream*, const SourceRange&) -> Value {
    auto tp = std::chrono::system_clock::now();
    return Value::make_int(std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count());
});
rb(reg, "sleep", 1, 1, "void", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int()) throw std::runtime_error("RUN297: sleep 期望整数参数");
    int64_t ms = a[0].as_int();
    if (ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
    return Value::make_null();
});

// ===== 文件 I/O 函数（8个）=====


// open(path, mode, encoding?, buffer_size?) -> file_handle
rb(reg, "open", 2, 4, "file", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN046: open 路径参数必须为字符串");
    if (!a[1].is_string()) throw std::runtime_error("RUN047: open 模式参数必须为字符串");
    std::string path = a[0].as_string();
    std::string mode = a[1].as_string();
    if (a.size() > 2 && !a[2].is_string()) throw std::runtime_error("RUN298: open 第三参数(encoding)期望字符串");
    if (a.size() > 3 && !a[3].is_int()) throw std::runtime_error("RUN299: open 第四参数(buffer_size)期望整数");
    std::string encoding = a.size() > 2 ? a[2].as_string() : "utf-8";
    int64_t buffer_size = a.size() > 3 ? a[3].as_int() : 8192;
    return global_file_io().open(path, mode, encoding, buffer_size);
});

// read(handle, size?) -> string | bytes
rb(reg, "read", 1, 2, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (a.size() > 1 && !a[1].is_int()) throw std::runtime_error("RUN300: read 第二参数(size)期望整数");
    int64_t size = a.size() > 1 ? a[1].as_int() : -1;
    return global_file_io().read(a[0], size);
});

// write(handle, data) -> int (写入字节数)
rb(reg, "write", 2, 2, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    return global_file_io().write(a[0], a[1]);
});

// close(handle) -> bool
rb(reg, "close", 1, 1, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    return global_file_io().close(a[0]);
});

// stat(path) -> dict (size/mtime/mode/type)
rb(reg, "stat", 1, 1, "dict", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN049: stat 路径参数必须为字符串");
    return global_file_io().stat(a[0].as_string());
});

// atomic_write(path, content) -> int
rb(reg, "atomic_write", 2, 2, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN050: atomic_write 路径参数必须为字符串");
    return global_file_io().atomic_write(a[0].as_string(), a[1]);
});

// async_read(path, encoding?) -> coroutine（后台线程读取文件）
rb(reg, "async_read", 1, 2, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN051: async_read 路径参数必须为字符串");
    std::string path = a[0].as_string();
    if (a.size() > 1 && !a[1].is_string()) throw std::runtime_error("RUN301: async_read 第二参数(encoding)期望字符串");
    std::string encoding = a.size() > 1 ? a[1].as_string() : "utf-8";
    return AsyncRuntime::instance().submit([path, encoding]() -> std::shared_ptr<Value> {
        Value result = global_file_io().async_read(path, encoding);
        return std::make_shared<Value>(std::move(result));
    });
});

rb(reg, "async_write", 2, 2, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN052: async_write 路径参数必须为字符串");
    std::string path = a[0].as_string();
    Value data = a[1];
    return AsyncRuntime::instance().submit([path, data]() -> std::shared_ptr<Value> {
        Value result = global_file_io().async_write(path, data);
        return std::make_shared<Value>(std::move(result));
    });
});

// ===== OOP 内建函数（4.5）=====

// classmethod(fn) -> 标记函数为类方法
// 第一个参数绑定类对象（在调用时由解释器处理）
rb(reg, "classmethod", 1, 1, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    // 直接返回原函数引用，标记通过 BoundMethodData.is_classmethod 处理
    // 这里返回原 FnRef，解释器在 eval_member 中根据 __method_kind_xxx 标记处理
    return a[0];
});

// staticmethod(fn) -> 标记函数为静态方法
// 不绑定 self/cls 参数
rb(reg, "staticmethod", 1, 1, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    // 直接返回原函数引用，标记通过 __method_kind_xxx 处理
    return a[0];
});

// super(current_class, instance) -> 返回 super 代理对象
// 简化实现：返回一个 BoundMethodData，instance 字段存当前实例，
// method_name 存 "super:current_class"，解释器在调用时识别
rb(reg, "super", 1, 2, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    // super(instance) 或 super(current_class, instance)
    Value instance;
    std::string current_class;
    if (a.size() >= 2) {
        if (!a[0].is_string()) throw std::runtime_error("RUN258: super 第一个参数(类名)期望字符串");
        current_class = a[0].as_string();
        instance = a[1];
    } else {
        instance = a[0];
        // 从实例推断类名
        if (instance.is_struct()) {
            current_class = instance.as_struct().struct_name;
        }
    }
    // 返回一个标记为 super 的 BoundMethod
    auto bm = std::make_shared<BoundMethodData>();
    bm->instance = instance;
    bm->method_name = std::string("super:") + current_class;
    return Value::make_bound_method(bm);
});

// isinstance(obj, class_name) -> bool
// 检查 obj 是否为 class_name 或其子类的实例（基于 MRO 查找）
rb(reg, "isinstance", 2, 2, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    const Value& obj = a[0];
    std::string cls_name = a[1].is_string() ? a[1].as_string() :
                           (a[1].is_class() ? a[1].as_class().name : "");
    if (cls_name.empty()) return Value::make_bool(false);
    
    // 基本类型检查
    if (cls_name == "int" && obj.is_int()) return Value::make_bool(true);
    if (cls_name == "float" && obj.is_float()) return Value::make_bool(true);
    if (cls_name == "string" && obj.is_string()) return Value::make_bool(true);
    if (cls_name == "bool" && obj.is_bool()) return Value::make_bool(true);
    if (cls_name == "array" && obj.is_array()) return Value::make_bool(true);
    if (cls_name == "tuple" && obj.is_tuple()) return Value::make_bool(true);
    if (cls_name == "null" && obj.is_null()) return Value::make_bool(true);
    
    // 类实例检查（基于 MRO）
    if (obj.is_struct()) {
        std::string obj_cls_name = obj.as_struct().struct_name;
        // 如果是类对象本身
        if (obj.is_class()) {
            obj_cls_name = obj.as_class().name;
        }
        // 直接匹配
        if (obj_cls_name == cls_name) return Value::make_bool(true);
        // 继承类型通过全局类表 MRO 查找
        if (g_class_table.table) {
            auto cit = g_class_table.table->find(obj_cls_name);
            if (cit != g_class_table.table->end()) {
                for (const auto& mro_name : cit->second.mro) {
                    if (mro_name == cls_name) return Value::make_bool(true);
                }
            }
        }
        return Value::make_bool(false);
    }
    return Value::make_bool(false);
});

// issubtype(sub, super) -> bool
// 检查 sub 是否为 super 的子类型（基于 MRO 查找）
rb(reg, "issubtype", 2, 2, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    std::string sub_name = a[0].is_string() ? a[0].as_string() :
                           (a[0].is_class() ? a[0].as_class().name : "");
    std::string super_name = a[1].is_string() ? a[1].as_string() :
                             (a[1].is_class() ? a[1].as_class().name : "");
    if (sub_name.empty() || super_name.empty()) return Value::make_bool(false);
    
    // 相同类型
    if (sub_name == super_name) return Value::make_bool(true);
    
    // 基本类型的子类型关系（无继承，只有相同）
    static const std::vector<std::string> basic_types = {"int", "float", "string", "bool", "array", "tuple", "null"};
    bool sub_is_basic = std::find(basic_types.begin(), basic_types.end(), sub_name) != basic_types.end();
    bool super_is_basic = std::find(basic_types.begin(), basic_types.end(), super_name) != basic_types.end();
    if (sub_is_basic && super_is_basic) return Value::make_bool(false);
    
    // 类继承关系检查（通过全局类表 MRO 查找）
    if (g_class_table.table) {
        auto cit = g_class_table.table->find(sub_name);
        if (cit != g_class_table.table->end()) {
            for (const auto& mro_name : cit->second.mro) {
                if (mro_name == super_name) return Value::make_bool(true);
            }
        }
    }
    return Value::make_bool(false);
});

// hasattr(obj, attr_name) -> bool
rb(reg, "hasattr", 2, 2, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    const Value& obj = a[0];
    if (!a[1].is_string()) throw std::runtime_error("RUN259: hasattr 属性名期望字符串");
    std::string attr = a[1].as_string();
    if (obj.is_struct()) {
        const auto& si = obj.as_struct();
        if (si.fields.count(attr) > 0) return Value::make_bool(true);
        for (const auto& name : si.class_attr_names) {
            if (name == attr) return Value::make_bool(true);
        }
    }
    return Value::make_bool(false);
});

// getattr(obj, attr_name, default?) -> value
rb(reg, "getattr", 2, 3, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    const Value& obj = a[0];
    if (!a[1].is_string()) throw std::runtime_error("RUN260: getattr 属性名期望字符串");
    std::string attr = a[1].as_string();
    if (obj.is_struct()) {
        auto it = obj.as_struct().fields.find(attr);
        if (it != obj.as_struct().fields.end()) return it->second;
    }
    if (a.size() > 2) return a[2];
    throw std::runtime_error("RUN082: getattr 对象没有该属性: " + attr);
});

// setattr(obj, attr_name, value) -> null
rb(reg, "setattr", 3, 3, "null", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    Value& obj = a[0];
    if (!a[1].is_string()) throw std::runtime_error("RUN261: setattr 属性名期望字符串");
    std::string attr = a[1].as_string();
    if (obj.is_struct()) {
        obj.as_struct().fields[attr] = a[2];
    } else {
        throw std::runtime_error("RUN030: setattr 期望结构体参数");
    }
    return Value::make_null();
});


// cast(obj, type_name) -> value
// 安全转型：检查 obj 是否可转为 type_name，可转则返回 obj，否则抛 RUN019
rb(reg, "cast", 2, 2, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange& loc) -> Value {
    const Value& obj = a[0];
    std::string type_name = a[1].is_string() ? a[1].as_string() :
                            (a[1].is_class() ? a[1].as_class().name : "");
    if (type_name.empty()) {
        throw std::runtime_error("RUN019: cast 类型参数无效");
    }
    
    // 基本类型检查
    if (type_name == "int" && obj.is_int()) return obj;
    if (type_name == "float" && obj.is_float()) return obj;
    if (type_name == "string" && obj.is_string()) return obj;
    if (type_name == "bool" && obj.is_bool()) return obj;
    if (type_name == "array" && obj.is_array()) return obj;
    if (type_name == "tuple" && obj.is_tuple()) return obj;
    if (type_name == "null" && obj.is_null()) return obj;
    
    // 数值提升：int -> float
    if (type_name == "float" && obj.is_int()) {
        return Value::make_float(static_cast<double>(obj.as_int()));
    }
    
    // 类实例检查
    if (obj.is_struct()) {
        std::string obj_cls_name = obj.as_struct().struct_name;
        if (obj_cls_name == type_name) return obj;
        // 完整实现需要查 MRO
    }
    
    // 无法转型，抛 RUN019
    throw std::runtime_error("RUN019: 无法将类型 '" + obj.type_name() + "' 转型为 '" + type_name + "'");
});

// callable(obj) -> bool
// 检查对象是否可调用
rb(reg, "callable", 1, 1, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    const Value& obj = a[0];
    // 函数引用、类对象、bound method 都是可调用的
    if (obj.is_fnref()) return Value::make_bool(true);
    if (obj.is_class()) return Value::make_bool(true);
    if (obj.is_bound_method()) return Value::make_bool(true);
    // 结构体实例如果有 __call__ 方法也是可调用的
    if (obj.is_struct()) {
        return Value::make_bool(obj.as_struct().fields.count("__call__") > 0);
    }
    return Value::make_bool(false);
});

// protocol_check(obj, protocol_name) -> bool
// 检查对象是否满足协议（有特定方法集合）
rb(reg, "protocol_check", 2, 2, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    const Value& obj = a[0];
    std::string protocol_name = a[1].is_string() ? a[1].as_string() : "";
    if (protocol_name.empty()) return Value::make_bool(false);
    
    // 定义常用协议
    static const std::unordered_map<std::string, std::vector<std::string>> protocols = {
        {"Iterator", {"__next__"}},
        {"Iterable", {"__iter__"}},
        {"Container", {"__contains__"}},
        {"Sized", {"__len__"}},
        {"Callable", {"__call__"}},
        {"ContextManager", {"__enter__", "__exit__"}},
        {"Descriptor", {"__get__", "__set__", "__delete__"}},
        {"Sequence", {"__len__", "__getitem__"}},
        {"Mapping", {"__len__", "__getitem__", "__setitem__", "__delitem__"}},
        {"Drawable", {"draw"}},
        {"Serializable", {"serialize"}}
    };
    
    auto it = protocols.find(protocol_name);
    if (it == protocols.end()) return Value::make_bool(false);
    
    // 检查对象是否有所有必需的方法
    if (obj.is_struct()) {
        const auto& si = obj.as_struct();
        for (const auto& method : it->second) {
            bool found = si.fields.count(method) > 0;
            if (!found) {
                for (const auto& name : si.class_attr_names) {
                    if (name == method) { found = true; break; }
                }
            }
            if (!found) return Value::make_bool(false);
        }
        return Value::make_bool(true);
    }
    
    // 基本类型对特定协议的支持
    if (obj.is_array() && protocol_name == "Sequence") return Value::make_bool(true);
    if (obj.is_string() && protocol_name == "Sequence") return Value::make_bool(true);
    if (obj.is_dict() && protocol_name == "Mapping") return Value::make_bool(true);
    
    return Value::make_bool(false);
});

    // ===== 字典/集合内建函数（7.5）=====

    // dict(iterable) -> 创建字典
    rb(reg, "dict", 0, 1, "dict", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        auto dict_data = std::make_shared<DictData>();
        if (a.empty()) return Value::make_dict(std::move(dict_data));
        // 从数组/可迭代对象创建字典（期望元素为二元组）
        if (a[0].is_array()) {
            for (const auto& item : a[0].as_array()) {
                if (item.is_tuple() && item.as_tuple().size() >= 2) {
                    Value key = item.as_tuple()[0];
                    Value val = item.as_tuple()[1];
                    dict_data->entries.emplace_back(key, val);
                }
            }
        }
        return Value::make_dict(std::move(dict_data));
    });

    // set(iterable) -> 创建集合
    rb(reg, "set", 0, 1, "set", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        auto set_data = std::make_shared<SetData>();
        if (a.empty()) return Value::make_set(std::move(set_data));
        // 从数组/可迭代对象创建集合
        if (a[0].is_array()) {
            for (const auto& item : a[0].as_array()) {
                bool found = false;
                for (const auto& existing : set_data->elements) {
                    if (existing.equals(item)) { found = true; break; }
                }
                if (!found) set_data->elements.push_back(item);
            }
        } else if (a[0].is_set()) {
            // 复制集合
            for (const auto& item : a[0].as_set().elements) set_data->elements.push_back(item);
        }
        return Value::make_set(std::move(set_data));
    });

    // keys(dict) -> 返回键列表
    rb(reg, "keys", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_dict()) throw std::runtime_error("RUN079: keys 期望字典参数");
        std::vector<Value> keys;
        for (const auto& kv : a[0].as_dict().entries) keys.push_back(kv.first);
        return Value::make_array(std::move(keys));
    });

    // values(dict) -> 返回值列表
    rb(reg, "values", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_dict()) throw std::runtime_error("RUN080: values 期望字典参数");
        std::vector<Value> vals;
        for (const auto& kv : a[0].as_dict().entries) vals.push_back(kv.second);
        return Value::make_array(std::move(vals));
    });

    // items(dict) -> 返回键值对列表
    rb(reg, "items", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a[0].is_dict()) throw std::runtime_error("RUN081: items 期望字典参数");
        std::vector<Value> items;
        for (const auto& kv : a[0].as_dict().entries) {
            std::vector<Value> pair = {kv.first, kv.second};
            items.push_back(Value::make_tuple(std::move(pair)));
        }
        return Value::make_array(std::move(items));
    });

    // ===== 生成器函数（9.3）=====
    // next(gen) -> 获取生成器的下一个值
    rb(reg, "next", 1, 1, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange& loc) -> Value {
        if (!a[0].is_generator()) {
            throw std::runtime_error("RUN024: next 期望生成器参数");
        }
        throw std::runtime_error("RUN026: next(gen) 必须直接调用，不能通过间接路径调用");
    });

    // iter(obj) -> 获取迭代器
    rb(reg, "iter", 1, 1, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange& loc) -> Value {
        // 如果是数组/字符串/元组，返回迭代器对象
        if (a[0].is_array() || a[0].is_string() || a[0].is_tuple() || a[0].is_generator()) {
            return a[0];
        }
        // 检查是否有 __iter__ 方法
        throw std::runtime_error("RUN026: iter 对象不可迭代");
    });

    // StopIteration 异常（用字符串常量表示）
    rb(reg, "StopIteration", 0, 0, "string", [](std::vector<Value>&, std::ostream*, std::istream*, const SourceRange&) -> Value {
        return Value::make_string("StopIteration");
    });

    // ===== 装饰器工具函数（10.1-10.4）=====
    // @property - 属性装饰器
    rb(reg, "property", 1, 1, "fn", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        // 标记函数为属性 getter，返回带有属性标记的函数引用
        if (a[0].is_fnref()) {
            std::string fn_name = a[0].as_fnref().name;
            return Value::make_fnref(FnRef("__property__" + fn_name));
        }
        return a[0];
    });

    // wraps(func) - 包装函数装饰器
    rb(reg, "wraps", 1, 1, "fn", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        // 返回原函数，用于保留元数据
        return a[0];
    });

    // lru_cache - 简单的 LRU 缓存装饰器
    rb(reg, "lru_cache", 0, 1, "fn", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (!a.empty() && a[0].is_fnref()) {
            return a[0];
        }
        throw std::runtime_error("RUN031: lru_cache 期望函数参数");
    });

    // cached_property - 缓存属性装饰器
    rb(reg, "cached_property", 1, 1, "fn", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
        if (a[0].is_fnref()) {
            std::string fn_name = a[0].as_fnref().name;
            return Value::make_fnref(FnRef("__cached_property__" + fn_name));
        }
        throw std::runtime_error("RUN034: cached_property 期望函数参数");
    });

    // ===== REPL 内建函数（11.4）=====

    // exit() - 退出 REPL
    rb(reg, "exit", 0, 0, "void", [](std::vector<Value>& a, std::ostream* out, std::istream*, const SourceRange&) -> Value {
        throw std::runtime_error("EXIT");
    });

    // help() - 显示帮助信息
    rb(reg, "help", 0, 0, "string", [](std::vector<Value>&, std::ostream* out, std::istream*, const SourceRange&) -> Value {
        std::string help_text = R"(Next1.1 REPL 帮助
==================
REPL 命令:
  :quit, :q  - 退出 REPL
  :help, :h  - 显示此帮助
  :save       - 保存会话到 .next11_session
  :load       - 加载会话

快捷键:
  上/下箭头   - 浏览历史记录
  Tab         - 自动补全
  Esc         - 清空当前行

内置函数分类:
  数学函数: abs, sqrt, sin, cos, pow, min, max 等
  字符串函数: str, concat, split, join, format 等
  数组函数: push, pop, slice, range, map, filter 等
  类型转换: to_int, to_float, to_string, to_bool 等
  文件 I/O: open, read, write, close 等
  其他: print, println, input, type, len 等

输入 :quit 退出 REPL)";
        return Value::make_string(help_text);
    });

    // reload() - 重新加载模块
    rb(reg, "reload", 0, 1, "null", [](std::vector<Value>& a, std::ostream* out, std::istream*, const SourceRange&) -> Value {
        throw std::runtime_error("RUN014: reload 尚未实现，需要 REPL 驱动支持");
    });

} // register_all_builtins

} // namespace next11