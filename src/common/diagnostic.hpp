// 诊断信息模块 - 对齐 spec 6.6 六要素规范
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <sstream>
#include <algorithm>

namespace next11 {

enum class ErrorCategory {
    Lex, Syn, Semant, Run, Cli, Generic, Match, Pipe, Destructure
};

inline std::string category_prefix(ErrorCategory c) {
    switch (c) {
        case ErrorCategory::Lex: return "LEX";
        case ErrorCategory::Syn: return "SYN";
        case ErrorCategory::Semant: return "SEM";
        case ErrorCategory::Run: return "RUN";
        case ErrorCategory::Cli: return "CLI";
        case ErrorCategory::Generic: return "GEN";
        case ErrorCategory::Match: return "MAT";
        case ErrorCategory::Pipe: return "PIP";
        case ErrorCategory::Destructure: return "DST";
    }
    return "ERR";
}

inline std::string category_name(ErrorCategory c) {
    switch (c) {
        case ErrorCategory::Lex: return "词法错误";
        case ErrorCategory::Syn: return "语法错误";
        case ErrorCategory::Semant: return "语义错误";
        case ErrorCategory::Run: return "运行期错误";
        case ErrorCategory::Cli: return "命令行错误";
        case ErrorCategory::Generic: return "泛型错误";
        case ErrorCategory::Match: return "模式匹配错误";
        case ErrorCategory::Pipe: return "管道错误";
        case ErrorCategory::Destructure: return "解构错误";
    }
    return "错误";
}

class Diagnostic {
public:
    ErrorCategory category;
    std::string code;
    std::string file;
    int line;
    int col;
    std::string desc;
    std::optional<std::string> fix;

    Diagnostic(ErrorCategory cat, std::string code_, std::string file_,
               int line_, int col_, std::string desc_,
               std::optional<std::string> fix_ = std::nullopt)
        : category(cat), code(std::move(code_)), file(std::move(file_)),
          line(line_), col(col_), desc(std::move(desc_)), fix(std::move(fix_)) {}

    std::string format() const {
        std::ostringstream oss;
        oss << category_name(category) << " " << code << ": " << desc
            << " 位于 " << file << ":" << line << ":" << col;
        if (fix.has_value()) {
            oss << "\n  修复建议: " << *fix;
        }
        return oss.str();
    }
};

class ErrorCollector {
public:
    std::vector<Diagnostic> _errors;

    void add(Diagnostic d) { _errors.push_back(std::move(d)); }
    bool empty() const { return _errors.empty(); }
    const std::vector<Diagnostic>& all() const { return _errors; }
    std::vector<Diagnostic>& all_mut() { return _errors; }
};

} // namespace next11