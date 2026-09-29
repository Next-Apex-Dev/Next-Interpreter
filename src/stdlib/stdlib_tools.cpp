// 100个标准库工具注册实现
#include "common/registry.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <random>
#include <chrono>
#include <thread>
#include <mutex>
#include <iomanip>

namespace next11 {

using BuiltinFn = std::function<Value(std::vector<Value>&, std::ostream*, std::istream*, const SourceRange&)>;

static std::mt19937_64 g_shared_rng(std::chrono::steady_clock::now().time_since_epoch().count());
static std::mutex g_rng_mtx;

static inline void rt(BuiltinRegistry& reg, const char* name, int min_a, int max_a,
                      const char* ret_type, BuiltinFn fn) {
    BuiltinInfo info;
    info.name = name;
    info.min_arity = min_a;
    info.max_arity = max_a;
    info.return_type = ret_type;
    info.eval_fn = std::move(fn);
    reg.register_builtin(info);
}

void register_all_stdlib_tools(BuiltinRegistry& reg) {

// ===== math_ 工具（20个）=====
rt(reg, "math_cbrt", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN204: math_cbrt 期望数值参数"); return Value::make_float(std::cbrt(a[0].as_number())); });
rt(reg, "math_sinh", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN205: math_sinh 期望数值参数"); return Value::make_float(std::sinh(a[0].as_number())); });
rt(reg, "math_cosh", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN206: math_cosh 期望数值参数"); return Value::make_float(std::cosh(a[0].as_number())); });
rt(reg, "math_tanh", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN207: math_tanh 期望数值参数"); return Value::make_float(std::tanh(a[0].as_number())); });
rt(reg, "math_asinh", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN208: math_asinh 期望数值参数"); return Value::make_float(std::asinh(a[0].as_number())); });
rt(reg, "math_acosh", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN209: math_acosh 期望数值参数"); if (a[0].as_number() < 1.0) throw std::runtime_error("RUN256: math_acosh 参数必须 >= 1"); return Value::make_float(std::acosh(a[0].as_number())); });
rt(reg, "math_atanh", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN210: math_atanh 期望数值参数"); double v = a[0].as_number(); if (v <= -1.0 || v >= 1.0) throw std::runtime_error("RUN257: math_atanh 参数必须在 (-1, 1) 范围内"); return Value::make_float(std::atanh(a[0].as_number())); });
rt(reg, "math_erf", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN211: math_erf 期望数值参数"); return Value::make_float(std::erf(a[0].as_number())); });
rt(reg, "math_erfc", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN212: math_erfc 期望数值参数"); return Value::make_float(std::erfc(a[0].as_number())); });
rt(reg, "math_lgamma", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN213: math_lgamma 期望数值参数"); return Value::make_float(std::lgamma(a[0].as_number())); });
rt(reg, "math_tgamma", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN214: math_tgamma 期望数值参数"); return Value::make_float(std::tgamma(a[0].as_number())); });
rt(reg, "math_nearbyint", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN215: math_nearbyint 期望数值参数"); return Value::make_float(std::nearbyint(a[0].as_number())); });
rt(reg, "math_rint", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN216: math_rint 期望数值参数"); return Value::make_float(std::rint(a[0].as_number())); });
rt(reg, "math_trunc", 1, 1, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN217: math_trunc 期望数值参数"); return Value::make_float(std::trunc(a[0].as_number())); });
rt(reg, "math_copysign", 2, 2, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN218: math_copysign 期望数值参数"); if (!a[1].is_int() && !a[1].is_float()) throw std::runtime_error("RUN218: math_copysign 第二参数期望数值"); return Value::make_float(std::copysign(a[0].as_number(), a[1].as_number())); });
rt(reg, "math_fmod", 2, 2, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN219: math_fmod 期望数值参数"); if (!a[1].is_int() && !a[1].is_float()) throw std::runtime_error("RUN219: math_fmod 第二参数期望数值"); if (a[1].as_number() == 0.0) throw std::runtime_error("RUN001: math_fmod 除零错误"); return Value::make_float(std::fmod(a[0].as_number(), a[1].as_number())); });
rt(reg, "math_remainder", 2, 2, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN220: math_remainder 期望数值参数"); if (!a[1].is_int() && !a[1].is_float()) throw std::runtime_error("RUN220: math_remainder 第二参数期望数值"); if (a[1].as_number() == 0.0) throw std::runtime_error("RUN001: math_remainder 除零错误"); return Value::make_float(std::remainder(a[0].as_number(), a[1].as_number())); });
rt(reg, "math_nextafter", 2, 2, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN221: math_nextafter 期望数值参数"); if (!a[1].is_int() && !a[1].is_float()) throw std::runtime_error("RUN221: math_nextafter 第二参数期望数值"); return Value::make_float(std::nextafter(a[0].as_number(), a[1].as_number())); });
rt(reg, "math_pow_int", 2, 2, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() || !a[1].is_int()) throw std::runtime_error("RUN041: math_pow_int 期望整数参数");
    int64_t base = a[0].as_int(), exp = a[1].as_int(), r = 1;
    if (exp < 0) throw std::runtime_error("RUN345: math_pow_int 指数不能为负数");
    if (base == INT64_MIN) throw std::runtime_error("RUN347: math_pow_int base=INT64_MIN 绝对值溢出");
    for (int64_t i = 0; i < exp; ++i) {
        int64_t ab = base < 0 ? -base : base;
        int64_t ar = r < 0 ? -r : r;
        if (base != 0 && ar > INT64_MAX / ab) throw std::runtime_error("RUN346: math_pow_int 结果溢出 int64");
        r *= base;
    }
    return Value::make_int(r);
});
rt(reg, "math_is_nan", 1, 1, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value { if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN222: math_is_nan 期望数值参数"); return Value::make_bool(std::isnan(a[0].as_number())); });

// ===== str_ 工具（20个）=====
rt(reg, "str_camel_case", 1, 1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN185: str_camel_case 期望字符串参数");
    std::string s = a[0].as_string(), r;
    bool upper_next = false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '_' || s[i] == '-' || s[i] == ' ') { upper_next = true; continue; }
        r += upper_next ? static_cast<char>(std::toupper(static_cast<unsigned char>(s[i]))) : s[i];
        upper_next = false;
    }
    return Value::make_string(r);
});
rt(reg, "str_snake_case", 1, 1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN186: str_snake_case 期望字符串参数");
    std::string s = a[0].as_string(), r;
    for (size_t i = 0; i < s.size(); ++i) {
        if (std::isupper(static_cast<unsigned char>(s[i])) && !r.empty()) r += '_';
        r += static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    }
    return Value::make_string(r);
});
rt(reg, "str_kebab_case", 1, 1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN187: str_kebab_case 期望字符串参数");
    std::string s = a[0].as_string(), r;
    for (size_t i = 0; i < s.size(); ++i) {
        if (std::isupper(static_cast<unsigned char>(s[i])) && !r.empty()) r += '-';
        r += static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    }
    return Value::make_string(r);
});
rt(reg, "str_title_case", 1, 1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN188: str_title_case 期望字符串参数");
    std::string s = a[0].as_string(), r;
    bool upper_next = true;
    for (auto c : s) {
        if (c == ' ' || c == '_' || c == '-') { r += c; upper_next = true; }
        else { r += upper_next ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c; upper_next = false; }
    }
    return Value::make_string(r);
});
rt(reg, "str_capitalize", 1, 1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN189: str_capitalize 期望字符串参数");
    std::string s = a[0].as_string();
    if (!s.empty()) {
        s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
        for (size_t i = 1; i < s.size(); ++i)
            s[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    }
    return Value::make_string(s);
});
rt(reg, "str_words", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN190: str_words 期望字符串参数");
    std::string s = a[0].as_string();
    std::vector<Value> words;
    std::string cur;
    for (auto c : s) {
        if (c == ' ' || c == '\t' || c == '\n') { if (!cur.empty()) { words.push_back(Value::make_string(cur)); cur.clear(); } }
        else cur += c;
    }
    if (!cur.empty()) words.push_back(Value::make_string(cur));
    return Value::make_array(std::move(words));
});
rt(reg, "str_lines", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN191: str_lines 期望字符串参数");
    std::string s = a[0].as_string();
    std::vector<Value> lines;
    std::string cur;
    for (auto c : s) {
        if (c == '\n') { lines.push_back(Value::make_string(cur)); cur.clear(); }
        else cur += c;
    }
    lines.push_back(Value::make_string(cur));
    return Value::make_array(std::move(lines));
});
rt(reg, "str_strip", 1, 1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN192: str_strip 期望字符串参数");
    std::string s = a[0].as_string();
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) return Value::make_string("");
    return Value::make_string(s.substr(start, end - start + 1));
});
rt(reg, "str_lstrip", 1, 1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN193: str_lstrip 期望字符串参数");
    std::string s = a[0].as_string();
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return Value::make_string("");
    return Value::make_string(s.substr(start));
});
rt(reg, "str_rstrip", 1, 1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN194: str_rstrip 期望字符串参数");
    std::string s = a[0].as_string();
    size_t end = s.find_last_not_of(" \t\n\r");
    if (end == std::string::npos) return Value::make_string("");
    return Value::make_string(s.substr(0, end + 1));
});
rt(reg, "str_center", 2, 3, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN195: str_center 期望字符串参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN306: str_center 第二参数期望整数");
    if (a.size() > 2 && !a[2].is_string()) throw std::runtime_error("RUN307: str_center 第三参数期望字符串");
    std::string s = a[0].as_string();
    int64_t width = a[1].as_int();
    char pad = (a.size() > 2 && !a[2].as_string().empty()) ? a[2].as_string()[0] : ' ';
    int64_t total = width - static_cast<int64_t>(s.size());
    if (total <= 0) return Value::make_string(s);
    int64_t left = total / 2, right = total - left;
    return Value::make_string(std::string(static_cast<size_t>(left), pad) + s + std::string(static_cast<size_t>(right), pad));
});
rt(reg, "str_count", 2, 2, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN196: str_count 期望字符串参数");
    if (!a[1].is_string()) throw std::runtime_error("RUN308: str_count 第二参数期望字符串");
    std::string s = a[0].as_string(), sub = a[1].as_string();
    if (sub.empty()) return Value::make_int(0);
    int64_t cnt = 0; size_t pos = 0;
    while ((pos = s.find(sub, pos)) != std::string::npos) { cnt++; pos += sub.size(); }
    return Value::make_int(cnt);
});
rt(reg, "str_find_all", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN197: str_find_all 期望字符串参数");
    if (!a[1].is_string()) throw std::runtime_error("RUN309: str_find_all 第二参数期望字符串");
    std::string s = a[0].as_string(), sub = a[1].as_string();
    std::vector<Value> positions;
    if (sub.empty()) return Value::make_array(std::move(positions));
    size_t pos = 0;
    while ((pos = s.find(sub, pos)) != std::string::npos) {
        positions.push_back(Value::make_int(static_cast<int64_t>(pos)));
        pos += sub.size();
    }
    return Value::make_array(std::move(positions));
});
rt(reg, "str_replace_all", 3, 3, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN198: str_replace_all 期望字符串参数");
    if (!a[1].is_string()) throw std::runtime_error("RUN310: str_replace_all 第二参数期望字符串");
    if (!a[2].is_string()) throw std::runtime_error("RUN311: str_replace_all 第三参数期望字符串");
    std::string s = a[0].as_string(), from = a[1].as_string(), to = a[2].as_string();
    if (from.empty()) throw std::runtime_error("RUN310: str_replace_all 搜索串不能为空");
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) { s.replace(pos, from.size(), to); pos += to.size(); }
    return Value::make_string(s);
});
rt(reg, "str_split_lines", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN199: str_split_lines 期望字符串参数");
    std::string s = a[0].as_string();
    std::vector<Value> lines;
    std::string cur;
    for (auto c : s) {
        if (c == '\n') { lines.push_back(Value::make_string(cur)); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) lines.push_back(Value::make_string(cur));
    return Value::make_array(std::move(lines));
});
rt(reg, "str_is_empty", 1, 1, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN200: str_is_empty 期望字符串参数");
    return Value::make_bool(a[0].as_string().empty());
});
rt(reg, "str_is_numeric", 1, 1, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN201: str_is_numeric 期望字符串参数");
    std::string s = a[0].as_string();
    if (s.empty()) return Value::make_bool(false);
    bool has_dot = false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i == 0 && s[i] == '-') continue;
        if (s[i] == '.' && !has_dot) { has_dot = true; continue; }
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return Value::make_bool(false);
    }
    return Value::make_bool(true);
});
rt(reg, "str_is_alphanumeric", 1, 1, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN202: str_is_alphanumeric 期望字符串参数");
    std::string s = a[0].as_string();
    if (s.empty()) return Value::make_bool(false);
    for (auto c : s) if (!std::isalnum(static_cast<unsigned char>(c))) return Value::make_bool(false);
    return Value::make_bool(true);
});
rt(reg, "str_char_code", 1, 1, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_string()) throw std::runtime_error("RUN203: str_char_code 期望字符串参数");
    std::string s = a[0].as_string();
    if (s.empty()) return Value::make_int(-1);
    return Value::make_int(static_cast<int64_t>(static_cast<unsigned char>(s[0])));
});
rt(reg, "str_from_char_code", 1, 1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int()) throw std::runtime_error("RUN045: str_from_char_code 期望整数参数");
    return Value::make_string(std::string(1, static_cast<char>(a[0].as_int())));
});

// ===== arr_ 工具（20个）=====
rt(reg, "arr_sort_by_key", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN094: arr_sort_by_key 期望数组参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN312: arr_sort_by_key 第二参数期望整数");
    auto arr = a[0].as_array();
    int64_t key_idx = a[1].as_int();
    if (key_idx < 0) throw std::runtime_error("RUN357: arr_sort_by_key 索引不能为负数");
    auto get_key = [key_idx](const Value& v) -> Value {
        if (v.is_array() && key_idx < static_cast<int64_t>(v.as_array().size())) return v.as_array()[static_cast<size_t>(key_idx)];
        if (v.is_tuple() && key_idx < static_cast<int64_t>(v.as_tuple().size())) return v.as_tuple()[static_cast<size_t>(key_idx)];
        return v;
    };
    bool all_num = true, all_str = true;
    for (auto& v : arr) {
        auto k = get_key(v);
        if (!k.is_int() && !k.is_float()) all_num = false;
        if (!k.is_string()) all_str = false;
    }
    if (all_num) {
        std::sort(arr.begin(), arr.end(), [key_idx](const Value& x, const Value& y) {
            auto get_key = [key_idx](const Value& v) -> Value {
                if (v.is_array() && key_idx < static_cast<int>(v.as_array().size())) return v.as_array()[key_idx];
                if (v.is_tuple() && key_idx < static_cast<int>(v.as_tuple().size())) return v.as_tuple()[key_idx];
                return v;
            };
            return get_key(x).as_number() < get_key(y).as_number();
        });
    } else if (all_str) {
        std::sort(arr.begin(), arr.end(), [key_idx](const Value& x, const Value& y) {
            auto get_key = [key_idx](const Value& v) -> Value {
                if (v.is_array() && key_idx < static_cast<int>(v.as_array().size())) return v.as_array()[key_idx];
                if (v.is_tuple() && key_idx < static_cast<int>(v.as_tuple().size())) return v.as_tuple()[key_idx];
                return v;
            };
            return get_key(x).as_string() < get_key(y).as_string();
        });
    } else {
        throw std::runtime_error("RUN334: arr_sort_by_key 键值必须全为数值或全为字符串");
    }
    return Value::make_array(std::move(arr));
});
rt(reg, "arr_group_by", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN095: arr_group_by 期望数组参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN313: arr_group_by 第二参数期望整数");
    auto& arr = a[0].as_array();
    int64_t key_idx = a[1].as_int();
    if (key_idx < 0) throw std::runtime_error("RUN358: arr_group_by 索引不能为负数");
    auto get_key = [key_idx](const Value& v) -> Value {
        if (v.is_array() && key_idx < static_cast<int64_t>(v.as_array().size())) return v.as_array()[static_cast<size_t>(key_idx)];
        if (v.is_tuple() && key_idx < static_cast<int64_t>(v.as_tuple().size())) return v.as_tuple()[static_cast<size_t>(key_idx)];
        return v;
    };
    std::vector<Value> groups;
    std::vector<Value> keys_seen;
    for (auto& v : arr) {
        Value key = get_key(v);
        bool found = false;
        for (size_t i = 0; i < keys_seen.size(); ++i) if (keys_seen[i].equals(key)) { found = true; break; }
        if (!found) {
            keys_seen.push_back(key);
            std::vector<Value> group;
            for (auto& w : arr) if (get_key(w).equals(key)) group.push_back(w);
            groups.push_back(Value::make_array(std::move(group)));
        }
    }
    return Value::make_array(std::move(groups));
});
rt(reg, "arr_partition", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN096: arr_partition 期望数组参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN314: arr_partition 第二参数期望整数");
    auto& arr = a[0].as_array();
    int64_t pivot_idx = a[1].as_int();
    if (pivot_idx < 0 || pivot_idx >= static_cast<int64_t>(arr.size())) throw std::runtime_error("RUN132: arr_partition 索引越界");
    bool all_num = true, all_str = true;
    for (auto& v : arr) {
        if (!v.is_int() && !v.is_float()) all_num = false;
        if (!v.is_string()) all_str = false;
    }
    if (!all_num && !all_str) throw std::runtime_error("RUN335: arr_partition 数组元素必须全为数值或全为字符串");
    std::vector<Value> left, right;
    for (auto& v : arr) {
        if (all_num) {
            if (v.as_number() < arr[pivot_idx].as_number()) left.push_back(v);
            else right.push_back(v);
        } else {
            if (v.as_string() < arr[pivot_idx].as_string()) left.push_back(v);
            else right.push_back(v);
        }
    }
    std::vector<Value> result = {Value::make_array(std::move(left)), Value::make_array(std::move(right))};
    return Value::make_array(std::move(result));
});
rt(reg, "arr_sliding_window", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN097: arr_sliding_window 期望数组参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN315: arr_sliding_window 第二参数期望整数");
    auto& arr = a[0].as_array();
    int64_t size = a[1].as_int();
    if (size <= 0 || size > static_cast<int64_t>(arr.size())) return Value::make_array({});
    std::vector<Value> windows;
    for (int64_t i = 0; i + size <= static_cast<int64_t>(arr.size()); ++i) {
        std::vector<Value> w(arr.begin() + static_cast<size_t>(i), arr.begin() + static_cast<size_t>(i + size));
        windows.push_back(Value::make_array(std::move(w)));
    }
    return Value::make_array(std::move(windows));
});
rt(reg, "arr_combinations", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN098: arr_combinations 期望数组参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN316: arr_combinations 第二参数期望整数");
    auto& arr = a[0].as_array();
    int64_t k64 = a[1].as_int();
    if (k64 <= 0 || k64 > static_cast<int64_t>(arr.size())) return Value::make_array({});
    int k = static_cast<int>(k64);
    std::vector<Value> result;
    std::vector<int> indices(k);
    for (int i = 0; i < k; ++i) indices[i] = i;
    while (true) {
        std::vector<Value> combo;
        for (int i = 0; i < k; ++i) combo.push_back(arr[indices[i]]);
        result.push_back(Value::make_array(std::move(combo)));
        int i = k - 1;
        while (i >= 0 && indices[i] == static_cast<int>(arr.size()) - k + i) --i;
        if (i < 0) break;
        indices[i]++;
        for (int j = i + 1; j < k; ++j) indices[j] = indices[j-1] + 1;
    }
    return Value::make_array(std::move(result));
});
rt(reg, "arr_permutations", 1, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN099: arr_permutations 期望数组参数");
    if (a.size() >= 2 && !a[1].is_int()) throw std::runtime_error("RUN317: arr_permutations 第二参数期望整数");
    auto arr = a[0].as_array();
    int64_t k64 = a.size() >= 2 ? a[1].as_int() : static_cast<int64_t>(arr.size());
    if (k64 <= 0 || k64 > static_cast<int64_t>(arr.size())) return Value::make_array({});
    int k = static_cast<int>(k64);
    std::vector<Value> result;
    std::vector<int> indices(arr.size());
    for (size_t i = 0; i < indices.size(); ++i) indices[i] = static_cast<int>(i);
    do {
        std::vector<Value> perm;
        for (int i = 0; i < k; ++i) perm.push_back(arr[indices[i]]);
        result.push_back(Value::make_array(std::move(perm)));
        std::reverse(indices.begin() + k, indices.end());
    } while (std::next_permutation(indices.begin(), indices.end()));
    return Value::make_array(std::move(result));
});
rt(reg, "arr_difference", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN100: arr_difference 期望数组参数");
    if (!a[1].is_array()) throw std::runtime_error("RUN100: arr_difference 第二个参数也期望数组");
    auto& a1 = a[0].as_array();
    auto& a2 = a[1].as_array();
    std::vector<Value> r;
    for (auto& v : a1) {
        bool found = false;
        for (auto& w : a2) if (v.equals(w)) { found = true; break; }
        if (!found) r.push_back(v);
    }
    return Value::make_array(std::move(r));
});
rt(reg, "arr_intersection", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN101: arr_intersection 期望数组参数");
    if (!a[1].is_array()) throw std::runtime_error("RUN101: arr_intersection 第二个参数也期望数组");
    auto& a1 = a[0].as_array();
    auto& a2 = a[1].as_array();
    std::vector<Value> r;
    for (auto& v : a1) {
        for (auto& w : a2) if (v.equals(w)) { r.push_back(v); break; }
    }
    return Value::make_array(std::move(r));
});
rt(reg, "arr_union", 2, -1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    std::vector<Value> r;
    for (size_t arr_idx = 0; arr_idx < a.size(); ++arr_idx) {
        if (!a[arr_idx].is_array()) continue;
        auto& arr = a[arr_idx].as_array();
        for (auto& v : arr) {
            bool found = false;
            for (auto& existing : r) if (existing.equals(v)) { found = true; break; }
            if (!found) r.push_back(v);
        }
    }
    return Value::make_array(std::move(r));
});
rt(reg, "arr_symmetric_diff", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN102: arr_symmetric_diff 期望数组参数");
    if (!a[1].is_array()) throw std::runtime_error("RUN102: arr_symmetric_diff 第二个参数也期望数组");
    auto& a1 = a[0].as_array();
    auto& a2 = a[1].as_array();
    std::vector<Value> r;
    for (auto& v : a1) {
        bool found = false;
        for (auto& w : a2) if (v.equals(w)) { found = true; break; }
        if (!found) r.push_back(v);
    }
    for (auto& v : a2) {
        bool found = false;
        for (auto& w : a1) if (v.equals(w)) { found = true; break; }
        if (!found) r.push_back(v);
    }
    return Value::make_array(std::move(r));
});
rt(reg, "arr_rotate_left", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN103: arr_rotate_left 期望数组参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN318: arr_rotate_left 第二参数期望整数");
    auto arr = a[0].as_array();
    int64_t k64 = a[1].as_int();
    if (arr.empty()) return Value::make_array(std::move(arr));
    int n = static_cast<int>(arr.size());
    int k = static_cast<int>(((k64 % n) + n) % n);
    std::rotate(arr.begin(), arr.begin() + k, arr.end());
    return Value::make_array(std::move(arr));
});
rt(reg, "arr_rotate_right", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN104: arr_rotate_right 期望数组参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN319: arr_rotate_right 第二参数期望整数");
    auto arr = a[0].as_array();
    int64_t k64 = a[1].as_int();
    if (arr.empty()) return Value::make_array(std::move(arr));
    int n = static_cast<int>(arr.size());
    int k = static_cast<int>(((k64 % n) + n) % n);
    std::rotate(arr.begin(), arr.end() - k, arr.end());
    return Value::make_array(std::move(arr));
});
rt(reg, "arr_window", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN105: arr_window 期望数组参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN320: arr_window 第二参数期望整数");
    auto& arr = a[0].as_array();
    int64_t size = a[1].as_int();
    if (size <= 0) return Value::make_array({});
    std::vector<Value> windows;
    for (int64_t i = 0; i + size <= static_cast<int64_t>(arr.size()); ++i) {
        std::vector<Value> w(arr.begin() + static_cast<size_t>(i), arr.begin() + static_cast<size_t>(i + size));
        windows.push_back(Value::make_array(std::move(w)));
    }
    return Value::make_array(std::move(windows));
});
rt(reg, "arr_chunks_of", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN106: arr_chunks_of 期望数组参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN321: arr_chunks_of 第二参数期望整数");
    auto& arr = a[0].as_array();
    int64_t size = a[1].as_int();
    if (size <= 0) throw std::runtime_error("RUN343: arr_chunks_of 块大小必须为正数");
    std::vector<Value> r;
    for (int64_t i = 0; i < static_cast<int64_t>(arr.size()); i += size) {
        int64_t end = std::min(i + size, static_cast<int64_t>(arr.size()));
        std::vector<Value> chunk(arr.begin() + static_cast<size_t>(i), arr.begin() + static_cast<size_t>(end));
        r.push_back(Value::make_array(std::move(chunk)));
    }
    return Value::make_array(std::move(r));
});
rt(reg, "arr_step_by", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN107: arr_step_by 期望数组参数");
    if (!a[1].is_int()) throw std::runtime_error("RUN322: arr_step_by 第二参数期望整数");
    auto& arr = a[0].as_array();
    int64_t step = a[1].as_int();
    if (step <= 0) throw std::runtime_error("RUN344: arr_step_by 步长必须为正数");
    std::vector<Value> r;
    for (int64_t i = 0; i < static_cast<int64_t>(arr.size()); i += step) r.push_back(arr[static_cast<size_t>(i)]);
    return Value::make_array(std::move(r));
});
rt(reg, "arr_enumerate", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN108: arr_enumerate 期望数组参数");
    auto& arr = a[0].as_array();
    std::vector<Value> r;
    for (size_t i = 0; i < arr.size(); ++i) {
        std::vector<Value> pair = {Value::make_int(static_cast<int64_t>(i)), arr[i]};
        r.push_back(Value::make_tuple(std::move(pair)));
    }
    return Value::make_array(std::move(r));
});
rt(reg, "arr_pairwise", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN109: arr_pairwise 期望数组参数");
    auto& arr = a[0].as_array();
    std::vector<Value> r;
    for (size_t i = 0; i + 1 < arr.size(); ++i) {
        std::vector<Value> pair = {arr[i], arr[i+1]};
        r.push_back(Value::make_tuple(std::move(pair)));
    }
    return Value::make_array(std::move(r));
});
rt(reg, "arr_compact", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN110: arr_compact 期望数组参数");
    auto& arr = a[0].as_array();
    std::vector<Value> r;
    for (auto& v : arr) if (!v.is_null()) r.push_back(v);
    return Value::make_array(std::move(r));
});
rt(reg, "arr_prepend", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[1].is_array()) throw std::runtime_error("RUN111: arr_prepend 第二个参数期望数组");
    auto arr = a[1].as_array();
    arr.insert(arr.begin(), a[0]);
    return Value::make_array(std::move(arr));
});
rt(reg, "arr_append_unique", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN112: arr_append_unique 期望数组参数");
    auto arr = a[0].as_array();
    bool found = false;
    for (auto& v : arr) if (v.equals(a[1])) { found = true; break; }
    if (!found) arr.push_back(a[1]);
    return Value::make_array(std::move(arr));
});

// ===== dict_ 工具（15个）=====
rt(reg, "dict_create", 0, -1, "struct", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (a.size() % 2 != 0) throw std::runtime_error("RUN091: dict_create 参数必须成对（键值对）");
    StructInstance si;
    si.struct_name = "dict";
    for (size_t i = 0; i + 1 < a.size(); i += 2) si.fields[a[i].to_display()] = a[i+1];
    return Value::make_struct(std::move(si));
});
rt(reg, "dict_get", 2, 3, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_dict()) {
        if (a.size() > 2) return a[2];
        throw std::runtime_error("RUN083: dict_get 期望字典参数");
    }
    auto& d = a[0].as_dict();
    for (auto& kv : d.entries) {
        if (kv.first.equals(a[1])) return kv.second;
    }
    return a.size() > 2 ? a[2] : Value::make_null();
});
rt(reg, "dict_set", 3, 3, "struct", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_dict()) throw std::runtime_error("RUN084: dict_set 期望字典参数");
    auto& d = a[0].as_dict();
    if (d.is_frozen) throw std::runtime_error("RUN084: dict_set 不能修改冻结字典");
    bool found = false;
    for (auto& kv : d.entries) {
        if (kv.first.equals(a[1])) { kv.second = a[2]; found = true; break; }
    }
    if (!found) d.entries.push_back({a[1], a[2]});
    return a[0];
});
rt(reg, "dict_keys", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_dict()) throw std::runtime_error("RUN085: dict_keys 期望字典参数");
    auto& d = a[0].as_dict();
    std::vector<Value> keys;
    for (auto& kv : d.entries) keys.push_back(kv.first);
    return Value::make_array(std::move(keys));
});
rt(reg, "dict_values", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_dict()) throw std::runtime_error("RUN086: dict_values 期望字典参数");
    auto& d = a[0].as_dict();
    std::vector<Value> vals;
    for (auto& kv : d.entries) vals.push_back(kv.second);
    return Value::make_array(std::move(vals));
});
rt(reg, "dict_contains_key", 2, 2, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_dict()) throw std::runtime_error("RUN087: dict_contains_key 期望字典参数");
    for (auto& kv : a[0].as_dict().entries) if (kv.first.equals(a[1])) return Value::make_bool(true);
    return Value::make_bool(false);
});
rt(reg, "dict_remove", 2, 2, "struct", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_dict()) throw std::runtime_error("RUN088: dict_remove 期望字典参数");
    auto& d = a[0].as_dict();
    if (d.is_frozen) throw std::runtime_error("RUN088: dict_remove 不能修改冻结字典");
    for (auto it = d.entries.begin(); it != d.entries.end(); ++it) {
        if (it->first.equals(a[1])) { d.entries.erase(it); break; }
    }
    return a[0];
});
rt(reg, "dict_size", 1, 1, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_dict()) throw std::runtime_error("RUN089: dict_size 期望字典参数");
    return Value::make_int(static_cast<int64_t>(a[0].as_dict().entries.size()));
});
rt(reg, "dict_merge", 2, -1, "struct", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    DictData result;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].is_dict()) {
            for (auto& kv : a[i].as_dict().entries) {
                bool found = false;
                for (auto& e : result.entries) if (e.first.equals(kv.first)) { e.second = kv.second; found = true; break; }
                if (!found) result.entries.push_back(kv);
            }
        }
    }
    return Value::make_dict(std::make_shared<DictData>(std::move(result)));
});
rt(reg, "dict_from_pairs", 1, 1, "struct", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_array()) throw std::runtime_error("RUN090: dict_from_pairs 期望数组参数");
    DictData dd;
    auto& pairs = a[0].as_array();
    for (auto& p : pairs) {
        if (p.is_tuple() && p.as_tuple().size() >= 2) dd.entries.push_back({p.as_tuple()[0], p.as_tuple()[1]});
    }
    return Value::make_dict(std::make_shared<DictData>(std::move(dd)));
});
rt(reg, "dict_to_pairs", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_dict()) throw std::runtime_error("RUN091: dict_to_pairs 期望字典参数");
    auto& d = a[0].as_dict();
    std::vector<Value> pairs;
    for (auto& kv : d.entries) {
        std::vector<Value> pair = {kv.first, kv.second};
        pairs.push_back(Value::make_tuple(std::move(pair)));
    }
    return Value::make_array(std::move(pairs));
});
rt(reg, "dict_invert", 1, 1, "struct", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_dict()) throw std::runtime_error("RUN027: dict_invert 期望字典参数");
    DictData result;
    for (auto& kv : a[0].as_dict().entries) {
        for (auto& existing : result.entries) {
            if (existing.first.equals(kv.second))
                throw std::runtime_error("RUN364: dict_invert 存在重复值，无法反转");
        }
        result.entries.push_back({kv.second, kv.first});
    }
    return Value::make_dict(std::make_shared<DictData>(std::move(result)));
});
rt(reg, "dict_filter_keys", 2, 2, "struct", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_dict()) throw std::runtime_error("RUN028: dict_filter_keys 期望字典参数");
    if (!a[1].is_array()) throw std::runtime_error("RUN092: dict_filter_keys 第二个参数期望数组");
    DictData result;
    auto& keys = a[1].as_array();
    for (auto& kv : a[0].as_dict().entries) {
        for (auto& k : keys) if (k.equals(kv.first)) { result.entries.push_back(kv); break; }
    }
    return Value::make_dict(std::make_shared<DictData>(std::move(result)));
});
rt(reg, "dict_map_values", 2, 2, "struct", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_dict()) throw std::runtime_error("RUN029: dict_map_values 期望字典参数");
    if (!a[1].is_dict()) throw std::runtime_error("RUN093: dict_map_values 第二个参数期望字典");
    auto& mapping = a[1].as_dict();
    DictData result;
    for (auto& kv : a[0].as_dict().entries) {
        bool found = false;
        for (auto& e : mapping.entries) if (e.first.equals(kv.first)) { result.entries.push_back({kv.first, e.second}); found = true; break; }
        if (!found) result.entries.push_back(kv);
    }
    return Value::make_dict(std::make_shared<DictData>(std::move(result)));
});
rt(reg, "dict_clear", 1, 1, "struct", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_dict()) throw std::runtime_error("RUN090: dict_clear 期望字典参数");
    auto& d = a[0].as_dict();
    if (d.is_frozen) throw std::runtime_error("RUN090: dict_clear 不能修改冻结字典");
    d.entries.clear();
    return a[0];
});

// ===== set_ 工具（10个）=====
rt(reg, "set_create", 0, -1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    auto sd = std::make_shared<SetData>();
    for (auto& v : a) {
        bool found = false;
        for (auto& existing : sd->elements) if (existing.equals(v)) { found = true; break; }
        if (!found) sd->elements.push_back(v);
    }
    return Value::make_set(std::move(sd));
});
rt(reg, "set_add", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_set() && !a[0].is_array()) throw std::runtime_error("RUN113: set_add 期望集合参数");
    if (a[0].is_set()) {
        auto& s = a[0].as_set();
        if (s.is_frozen) throw std::runtime_error("RUN113: set_add 不能修改冻结集合");
        bool found = false;
        for (auto& v : s.elements) if (v.equals(a[1])) { found = true; break; }
        if (!found) s.elements.push_back(a[1]);
        return a[0];
    }
    auto& arr = a[0].as_array();
    bool found = false;
    for (auto& v : arr) if (v.equals(a[1])) { found = true; break; }
    if (!found) arr.push_back(a[1]);
    return a[0];
});
rt(reg, "set_contains", 2, 2, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_set() && !a[0].is_array()) throw std::runtime_error("RUN114: set_contains 期望集合参数");
    const std::vector<Value>* elems = nullptr;
    std::vector<Value> tmp;
    if (a[0].is_set()) { elems = &a[0].as_set().elements; }
    else { tmp = a[0].as_array(); elems = &tmp; }
    for (auto& v : *elems) if (v.equals(a[1])) return Value::make_bool(true);
    return Value::make_bool(false);
});
rt(reg, "set_remove", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_set() && !a[0].is_array()) throw std::runtime_error("RUN115: set_remove 期望集合参数");
    if (a[0].is_set()) {
        auto& s = a[0].as_set();
        if (s.is_frozen) throw std::runtime_error("RUN115: set_remove 不能修改冻结集合");
        s.elements.erase(std::remove_if(s.elements.begin(), s.elements.end(), [&](const Value& v) { return v.equals(a[1]); }), s.elements.end());
        return a[0];
    }
    auto& arr = a[0].as_array();
    arr.erase(std::remove_if(arr.begin(), arr.end(), [&](const Value& v) { return v.equals(a[1]); }), arr.end());
    return a[0];
});
rt(reg, "set_size", 1, 1, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_set() && !a[0].is_array()) throw std::runtime_error("RUN116: set_size 期望集合参数");
    if (a[0].is_set()) return Value::make_int(static_cast<int64_t>(a[0].as_set().elements.size()));
    return Value::make_int(static_cast<int64_t>(a[0].as_array().size()));
});
rt(reg, "set_union", 2, -1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    auto sd = std::make_shared<SetData>();
    for (size_t i = 0; i < a.size(); ++i) {
        const std::vector<Value>* elems = nullptr;
        std::vector<Value> tmp;
        if (a[i].is_set()) { elems = &a[i].as_set().elements; }
        else if (a[i].is_array()) { tmp = a[i].as_array(); elems = &tmp; }
        else continue;
        for (auto& v : *elems) {
            bool found = false;
            for (auto& existing : sd->elements) if (existing.equals(v)) { found = true; break; }
            if (!found) sd->elements.push_back(v);
        }
    }
    return Value::make_set(std::move(sd));
});
rt(reg, "set_intersect", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_set() && !a[0].is_array()) throw std::runtime_error("RUN117: set_intersect 期望集合参数");
    if (!a[1].is_set() && !a[1].is_array()) throw std::runtime_error("RUN117: set_intersect 第二个参数也期望集合");
    auto get_elems = [](const Value& v) -> std::vector<Value> {
        if (v.is_set()) return v.as_set().elements;
        return v.as_array();
    };
    auto a1 = get_elems(a[0]); auto a2 = get_elems(a[1]);
    auto sd = std::make_shared<SetData>();
    for (auto& v : a1) {
        for (auto& w : a2) if (v.equals(w)) {
            bool already = false;
            for (auto& existing : sd->elements) if (existing.equals(v)) { already = true; break; }
            if (!already) sd->elements.push_back(v);
            break;
        }
    }
    return Value::make_set(std::move(sd));
});
rt(reg, "set_difference", 2, 2, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_set() && !a[0].is_array()) throw std::runtime_error("RUN118: set_difference 期望集合参数");
    if (!a[1].is_set() && !a[1].is_array()) throw std::runtime_error("RUN118: set_difference 第二个参数也期望集合");
    auto get_elems = [](const Value& v) -> std::vector<Value> {
        if (v.is_set()) return v.as_set().elements;
        return v.as_array();
    };
    auto a1 = get_elems(a[0]); auto a2 = get_elems(a[1]);
    auto sd = std::make_shared<SetData>();
    for (auto& v : a1) {
        bool found = false;
        for (auto& w : a2) if (v.equals(w)) { found = true; break; }
        if (!found) sd->elements.push_back(v);
    }
    return Value::make_set(std::move(sd));
});
rt(reg, "set_is_subset", 2, 2, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_set() && !a[0].is_array()) throw std::runtime_error("RUN119: set_is_subset 期望集合参数");
    if (!a[1].is_set() && !a[1].is_array()) throw std::runtime_error("RUN119: set_is_subset 第二个参数也期望集合");
    auto get_elems = [](const Value& v) -> std::vector<Value> {
        if (v.is_set()) return v.as_set().elements;
        return v.as_array();
    };
    auto a1 = get_elems(a[0]); auto a2 = get_elems(a[1]);
    for (auto& v : a1) {
        bool found = false;
        for (auto& w : a2) if (v.equals(w)) { found = true; break; }
        if (!found) return Value::make_bool(false);
    }
    return Value::make_bool(true);
});
rt(reg, "set_to_array", 1, 1, "array", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_set() && !a[0].is_array()) throw std::runtime_error("RUN323: set_to_array 期望集合/数组参数");
    if (a[0].is_set()) return Value::make_array(a[0].as_set().elements);
    return Value::make_array(a[0].as_array());
});

// ===== time_ 工具（5个）=====
rt(reg, "time_now_ms", 0, 0, "int", [](std::vector<Value>&, std::ostream*, std::istream*, const SourceRange&) -> Value {
    auto tp = std::chrono::system_clock::now();
    return Value::make_int(std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count());
});
rt(reg, "time_format", 1, 1, "string", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int()) throw std::runtime_error("RUN223: time_format 期望整数参数");
    auto ms = a[0].as_int();
    if (ms < -62135596800000LL || ms > 253402300799999LL)
        throw std::runtime_error("RUN366: time_format 时间戳超出可表示范围");
    auto tp = std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::ostringstream oss;
    {
        static std::mutex time_mtx;
        std::lock_guard<std::mutex> lock(time_mtx);
        const char* ts = std::ctime(&t);
        if (!ts) throw std::runtime_error("RUN366: time_format 时间戳无效，无法格式化");
        oss << ts;
    }
    std::string s = oss.str();
    if (!s.empty() && s.back() == '\n') s.pop_back();
    return Value::make_string(s);
});
rt(reg, "time_duration_ms", 2, 2, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() || !a[1].is_int()) throw std::runtime_error("RUN224: time_duration_ms 期望整数参数");
    int64_t t0 = a[0].as_int(), t1 = a[1].as_int();
    if ((t1 > 0 && t0 < INT64_MIN + t1) || (t1 < 0 && t0 > INT64_MAX + t1))
        throw std::runtime_error("RUN359: time_duration_ms 减法溢出");
    return Value::make_int(t1 - t0);
});
rt(reg, "time_sleep_ms", 1, 1, "void", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int()) throw std::runtime_error("RUN225: time_sleep_ms 期望整数参数");
    int64_t ms = a[0].as_int();
    if (ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
    return Value::make_null();
});
rt(reg, "time_epoch", 0, 0, "int", [](std::vector<Value>&, std::ostream*, std::istream*, const SourceRange&) -> Value {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return Value::make_int(ms);
});

// ===== util_ 工具（10个）=====
rt(reg, "util_uuid", 0, 0, "string", [](std::vector<Value>&, std::ostream*, std::istream*, const SourceRange&) -> Value {
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t a, b;
    {
        std::lock_guard<std::mutex> lk(g_rng_mtx);
        a = dist(g_shared_rng);
        b = dist(g_shared_rng);
    }
    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(8) << (a & 0xFFFFFFFFULL) << "-"
        << std::setw(4) << ((a >> 32) & 0xFFFFULL) << "-"
        << std::setw(4) << ((a >> 48) & 0xFFFFULL) << "-"
        << std::setw(4) << (b & 0xFFFFULL) << "-"
        << std::setw(12) << (b >> 16);
    return Value::make_string(oss.str());
});
rt(reg, "util_random_int", 2, 2, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() || !a[1].is_int()) throw std::runtime_error("RUN042: util_random_int 期望整数参数");
    int64_t lo = a[0].as_int(), hi = a[1].as_int();
    if (lo > hi) std::swap(lo, hi);
    uint64_t udiff = static_cast<uint64_t>(hi) - static_cast<uint64_t>(lo);
    if (udiff == UINT64_MAX) throw std::runtime_error("RUN348: util_random_int 范围过大");
    uint64_t range = udiff + 1;
    uint64_t r;
    {
        std::lock_guard<std::mutex> lk(g_rng_mtx);
        uint64_t limit = UINT64_MAX - (UINT64_MAX % range);
        do { r = g_shared_rng(); } while (r >= limit);
        r %= range;
    }
    return Value::make_int(static_cast<int64_t>(static_cast<uint64_t>(lo) + r));
});
rt(reg, "util_random_float", 0, 2, "float", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    static std::uniform_real_distribution<double> dist(0.0, 1.0);
    if (a.size() >= 1 && !a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN324: util_random_float 第一参数期望数值");
    if (a.size() >= 2 && !a[1].is_int() && !a[1].is_float()) throw std::runtime_error("RUN325: util_random_float 第二参数期望数值");
    double lo = a.size() >= 1 ? a[0].as_number() : 0.0;
    double hi = a.size() >= 2 ? a[1].as_number() : 1.0;
    double rnd_val;
    {
        std::lock_guard<std::mutex> lk(g_rng_mtx);
        rnd_val = dist(g_shared_rng);
    }
    return Value::make_float(lo + rnd_val * (hi - lo));
});
rt(reg, "util_random_seed", 1, 1, "void", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int()) throw std::runtime_error("RUN226: util_random_seed 期望整数参数");
    {
        std::lock_guard<std::mutex> lk(g_rng_mtx);
        g_shared_rng.seed(static_cast<uint64_t>(a[0].as_int()));
    }
    return Value::make_null();
});
rt(reg, "util_clamp_val", 3, 3, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN227: util_clamp_val 期望数值参数");
    if (!a[1].is_int() && !a[1].is_float()) throw std::runtime_error("RUN326: util_clamp_val 第二参数期望数值");
    if (!a[2].is_int() && !a[2].is_float()) throw std::runtime_error("RUN327: util_clamp_val 第三参数期望数值");
    double v = a[0].as_number(), lo = a[1].as_number(), hi = a[2].as_number();
    if (v < lo) return a[1];
    if (v > hi) return a[2];
    return a[0];
});
rt(reg, "util_range_check", 3, 3, "bool", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() && !a[0].is_float()) throw std::runtime_error("RUN228: util_range_check 期望数值参数");
    if (!a[1].is_int() && !a[1].is_float()) throw std::runtime_error("RUN328: util_range_check 第二参数期望数值");
    if (!a[2].is_int() && !a[2].is_float()) throw std::runtime_error("RUN329: util_range_check 第三参数期望数值");
    double v = a[0].as_number(), lo = a[1].as_number(), hi = a[2].as_number();
    return Value::make_bool(v >= lo && v <= hi);
});
rt(reg, "util_default_val", 2, 2, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (a[0].is_null()) return a[1];
    return a[0];
});
rt(reg, "util_identity", 1, 1, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    return a[0];
});
rt(reg, "util_const_fn", 2, 2, "any", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    return a[1];
});
rt(reg, "util_compose", 3, 3, "int", [](std::vector<Value>& a, std::ostream*, std::istream*, const SourceRange&) -> Value {
    if (!a[0].is_int() || !a[1].is_int() || !a[2].is_int()) throw std::runtime_error("RUN043: util_compose 期望整数参数");
    int64_t x = a[0].as_int(), y = a[1].as_int(), z = a[2].as_int();
    if (x != 0 && y != 0) {
        if (x == INT64_MIN) throw std::runtime_error("RUN360: util_compose 乘法溢出");
        int64_t ax = x < 0 ? -x : x;
        int64_t ay = y < 0 ? -y : y;
        if (ay > 0) { if (ax > INT64_MAX / ay) throw std::runtime_error("RUN360: util_compose 乘法溢出"); }
    }
    int64_t prod = x * y;
    if ((z > 0 && prod > INT64_MAX - z) || (z < 0 && prod < INT64_MIN - z))
        throw std::runtime_error("RUN361: util_compose 加法溢出");
    return Value::make_int(prod + z);
});

} // register_all_tools

} // namespace next11