// 阶段接口抽象类 - 对齐 design 4.1 四阶段流水线
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <iostream>
#include "common/token.hpp"
#include "common/ast.hpp"
#include "common/symbol_table.hpp"
#include "common/diagnostic.hpp"

namespace next11 {

// ===== ILexer 词法分析接口 =====
struct LexResult {
    std::vector<Token> tokens;
    std::vector<Diagnostic> errors;
};

class ILexer {
public:
    virtual ~ILexer() = default;
    virtual LexResult tokenize(std::string_view source, std::string_view filename) = 0;
};

// ===== IParser 语法分析接口 =====
struct ParseResult {
    std::unique_ptr<Program> ast;
    std::vector<Diagnostic> errors;
};

class IParser {
public:
    virtual ~IParser() = default;
    virtual ParseResult parse(const std::vector<Token>& tokens, std::string_view filename) = 0;
};

// ===== ISemanticAnalyzer 语义分析接口 =====
struct SemantResult {
    std::unique_ptr<Program> annotated_ast;
    std::shared_ptr<SymbolTable> symbols;
    std::vector<Diagnostic> errors;
};

class ISemanticAnalyzer {
public:
    virtual ~ISemanticAnalyzer() = default;
    virtual SemantResult analyze(std::unique_ptr<Program> ast, std::string_view filename) = 0;
};

// ===== IInterpreter 解释执行接口 =====
struct ExecResult {
    int exit_code = 0;
    std::vector<Diagnostic> errors;
};

class IInterpreter {
public:
    virtual ~IInterpreter() = default;
    virtual ExecResult execute(Program& ast, SymbolTable& symbols,
                               std::ostream& out, std::ostream& err, std::istream& in,
                               std::string_view filename) = 0;
};

} // namespace next11