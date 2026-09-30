// 语法分析器接口与实现
#pragma once
#include <string>
#include <vector>
#include <memory>
#include "common/token.hpp"
#include "common/ast.hpp"
#include "common/diagnostic.hpp"
#include "common/interfaces.hpp"

namespace next11 {

class Parser : public IParser {
public:
    ParseResult parse(const std::vector<Token>& tokens, std::string_view filename) override;

private:
    std::vector<Token> _tokens;
    size_t _pos = 0;
    std::string _file;
    std::vector<Diagnostic> _errors;
    bool _allow_struct_lit = true;
    bool _allow_generic_args = true;
    bool _repl_mode = false;
    int _repl_expr_counter = 0;

public:
    void set_repl_mode(bool v) { _repl_mode = v; }
private:

    const Token& peek(size_t off = 0) const;
    const Token& current() const { return peek(0); }
    bool at_end() const;
    Token advance();
    bool check(TokenKind k) const;
    bool check_keyword(const std::string& kw) const;
    bool check_op(const std::string& op) const;
    bool check_delim(const std::string& d) const;
    bool match(TokenKind k);
    bool match_keyword(const std::string& kw);
    bool match_op(const std::string& op);
    bool match_delim(const std::string& d);
    void error(ErrorCategory cat, std::string code, const Token& t, std::string desc,
               std::optional<std::string> fix = std::nullopt);
    void sync_to_statement();
    void sync_to_decl();

    SourceRange range_from(int l, int c);

    // 声明
    std::unique_ptr<Decl> parse_decl();
    std::unique_ptr<LetDecl> parse_let_decl();
    std::unique_ptr<FnDef> parse_fn_def();
    std::unique_ptr<AsyncFnDef> parse_async_fn();
    std::unique_ptr<StructDef> parse_struct_def();
    std::unique_ptr<ClassDef> parse_class_def();
    std::unique_ptr<EnumDef> parse_enum_def();
    std::unique_ptr<TypeAlias> parse_type_alias();

    // 类型
    TypeRef parse_type_ref();
    std::vector<Param> parse_params();

    // 语句
    std::unique_ptr<Stmt> parse_stmt();
    std::unique_ptr<Expr> parse_block();
    std::unique_ptr<IfStmt> parse_if_stmt();
    std::unique_ptr<WhileStmt> parse_while_stmt();
    std::unique_ptr<ForStmt> parse_for_stmt();
    std::unique_ptr<ReturnStmt> parse_return_stmt();
    std::unique_ptr<WithStmt> parse_with_stmt();
    std::unique_ptr<Stmt> parse_try_stmt();
    std::unique_ptr<Stmt> parse_throw_expr();
    std::unique_ptr<Stmt> parse_del_stmt();
    std::unique_ptr<Decl> parse_import_stmt();

    // 表达式优先级
    std::unique_ptr<Expr> parse_expression();
    std::unique_ptr<Expr> parse_assignment();
    std::unique_ptr<Expr> parse_pipe();
    std::unique_ptr<Expr> parse_set_union();
    std::unique_ptr<Expr> parse_set_intersection();
    std::unique_ptr<Expr> parse_set_symdiff();
    std::unique_ptr<Expr> parse_logical_or();
    std::unique_ptr<Expr> parse_logical_and();
    std::unique_ptr<Expr> parse_comparison();
    std::unique_ptr<Expr> parse_additive();
    std::unique_ptr<Expr> parse_multiplicative();
    std::unique_ptr<Expr> parse_unary();
    std::unique_ptr<Expr> parse_postfix();
    std::unique_ptr<Expr> parse_call(std::unique_ptr<Expr> callee);
    std::unique_ptr<Expr> parse_primary();
    std::unique_ptr<Expr> parse_await_expr();
    std::unique_ptr<Expr> parse_yield_expr();

    // match
    std::unique_ptr<MatchExpr> parse_match_expr();
    MatchArm parse_match_arm();

    // 解构
    DestructurePattern parse_destructure_pattern();

    // 辅助
    std::unique_ptr<Expr> finish_call(std::unique_ptr<Expr> callee, int l, int c);
};

} // namespace next11