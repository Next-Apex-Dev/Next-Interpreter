// 语义分析器接口与实现
#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include "common/ast.hpp"
#include "common/symbol_table.hpp"
#include "common/diagnostic.hpp"
#include "common/interfaces.hpp"

namespace next11 {

class SemanticAnalyzer : public ISemanticAnalyzer {
public:
    SemantResult analyze(std::unique_ptr<Program> ast, std::string_view filename) override;

private:
    std::shared_ptr<SymbolTable> _symbols;
    std::vector<Diagnostic> _errors;
    std::string _file;
    std::unordered_map<std::string, std::string> _enum_variants;
    std::shared_ptr<Type> _current_return_type;
    std::vector<std::string> _current_type_params;
    int _loop_depth = 0;

    void error(ErrorCategory cat, std::string code, const SourceRange& loc,
               std::string desc, std::optional<std::string> fix = std::nullopt);

    void register_decls(Program& prog);
    void check_decl(Decl& d);
    void check_stmt(Stmt& s);
    void check_expr(Expr& e);
    std::shared_ptr<Type> infer_expr(Expr& e);
    std::shared_ptr<Type> resolve_type_ref(const TypeRef& ref);
    std::shared_ptr<Type> instantiate_type(const std::shared_ptr<Type>& t,
                                           const std::map<std::string, std::shared_ptr<Type>>& bindings);

    void check_block(Block& b);
    void check_let(LetDecl& d);
    void check_fn(FnDef& d);
    void check_import(ImportStmt& imp);
    void check_if(IfStmt& s);
    void check_while(WhileStmt& s);
    void check_for(ForStmt& s);
    void check_return(ReturnStmt& s);
    void check_break(BreakStmt& s);
    void check_continue(ContinueStmt& s);
    void check_match(MatchExpr& e);
    bool check_compatible(const std::shared_ptr<Type>& expected, const std::shared_ptr<Type>& actual);
};

} // namespace next11
