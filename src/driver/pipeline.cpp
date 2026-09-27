// 阶段流水线编排实现
#include "pipeline.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "semantic/semantic_analyzer.hpp"
#include "interp/interpreter.hpp"
#include "common/source_file.hpp"
#include <fstream>
#include <sstream>

namespace next11 {

void Pipeline::print_tokens(const std::vector<Token>& tokens) {
    for (auto& t : tokens) {
        std::cout << token_kind_name(t.kind) << "('" << t.lexeme << "') "
                  << t.line << ":" << t.col << "\n";
    }
}

static const char* node_kind_name(NodeKind k) {
    switch (k) {
        case NodeKind::Program: return "Program";
        case NodeKind::LetDecl: return "LetDecl";
        case NodeKind::FnDef: return "FnDef";
        case NodeKind::StructDef: return "StructDef";
        case NodeKind::EnumDef: return "EnumDef";
        case NodeKind::TypeAlias: return "TypeAlias";
        case NodeKind::ExprStmt: return "ExprStmt";
        case NodeKind::IfStmt: return "IfStmt";
        case NodeKind::WhileStmt: return "WhileStmt";
        case NodeKind::ForStmt: return "ForStmt";
        case NodeKind::ReturnStmt: return "ReturnStmt";
        case NodeKind::BreakStmt: return "BreakStmt";
        case NodeKind::ContinueStmt: return "ContinueStmt";
        case NodeKind::Block: return "Block";
        case NodeKind::BinaryExpr: return "BinaryExpr";
        case NodeKind::UnaryExpr: return "UnaryExpr";
        case NodeKind::LiteralExpr: return "LiteralExpr";
        case NodeKind::IdentExpr: return "IdentExpr";
        case NodeKind::CallExpr: return "CallExpr";
        case NodeKind::PipeExpr: return "PipeExpr";
        case NodeKind::MatchExpr: return "MatchExpr";
        case NodeKind::DestructureExpr: return "DestructureExpr";
        case NodeKind::AssignExpr: return "AssignExpr";
        case NodeKind::MemberExpr: return "MemberExpr";
        case NodeKind::IndexExpr: return "IndexExpr";
        case NodeKind::ArrayExpr: return "ArrayExpr";
        case NodeKind::TupleExpr: return "TupleExpr";
        case NodeKind::StructLitExpr: return "StructLitExpr";
        case NodeKind::GroupExpr: return "GroupExpr";
        case NodeKind::TypeRef: return "TypeRef";
        case NodeKind::WithStmt: return "WithStmt";
        case NodeKind::OptionalChainExpr: return "OptionalChainExpr";
        case NodeKind::GenExpExpr: return "GenExpExpr";
        case NodeKind::AwaitExpr: return "AwaitExpr";
        case NodeKind::AsyncFnDef: return "AsyncFnDef";
        case NodeKind::SpreadExpr: return "SpreadExpr";
        case NodeKind::SuperExpr: return "SuperExpr";
        case NodeKind::TypeHintExpr: return "TypeHintExpr";
        case NodeKind::ClassDef: return "ClassDef";
        case NodeKind::TryStmt: return "TryStmt";
        case NodeKind::ThrowStmt: return "ThrowStmt";
        case NodeKind::ImportStmt: return "ImportStmt";
        case NodeKind::YieldExpr: return "YieldExpr";
        case NodeKind::DictExpr: return "DictExpr";
        case NodeKind::SetExpr: return "SetExpr";
        case NodeKind::DelStmt: return "DelStmt";
    }
    return "Unknown";
}

void Pipeline::print_ast(const Program& prog, int indent) {
    auto pad = std::string(indent * 2, ' ');
    std::cout << pad << "Program\n";
    for (auto& d : prog.decls) {
        if (!d) continue;
        std::cout << pad << "  " << node_kind_name(d->node_kind);
        if (auto* fn = dynamic_cast<FnDef*>(d.get())) {
            std::cout << " name=" << fn->name;
            if (!fn->type_params.empty()) {
                std::cout << "<";
                for (size_t i = 0; i < fn->type_params.size(); ++i) {
                    if (i) std::cout << ",";
                    std::cout << fn->type_params[i];
                }
                std::cout << ">";
            }
            std::cout << " params=(";
            for (size_t i = 0; i < fn->params.size(); ++i) {
                if (i) std::cout << ", ";
                std::cout << fn->params[i].name << ":" << fn->params[i].type.name;
            }
            std::cout << ")";
        } else if (auto* let = dynamic_cast<LetDecl*>(d.get())) {
            std::cout << " name=" << let->name << ":" << let->declared_type.name;
        } else if (auto* sd = dynamic_cast<StructDef*>(d.get())) {
            std::cout << " name=" << sd->name << " fields={";
            for (size_t i = 0; i < sd->fields.size(); ++i) {
                if (i) std::cout << ", ";
                std::cout << sd->fields[i].name << ":" << sd->fields[i].type.name;
            }
            std::cout << "}";
        } else if (auto* ed = dynamic_cast<EnumDef*>(d.get())) {
            std::cout << " name=" << ed->name << " variants={";
            for (size_t i = 0; i < ed->variants.size(); ++i) {
                if (i) std::cout << ", ";
                std::cout << ed->variants[i];
            }
            std::cout << "}";
        } else if (auto* ta = dynamic_cast<TypeAlias*>(d.get())) {
            std::cout << " name=" << ta->name << " =" << ta->aliased.name;
        }
        std::cout << "\n";
    }
}

void Pipeline::print_symbols(const SymbolTable& sym) {
    for (auto& s : sym._scopes) {
        std::cout << "Scope " << s.level << ":\n";
        for (auto& kv : s.symbols) {
            std::cout << "  " << kv.first << " : "
                      << (kv.second.type ? kv.second.type->to_string() : "?") << "\n";
        }
    }
}

int Pipeline::run_file(const PipelineOptions& opts) {
    ErrorCollector errors;
    auto sf = SourceFile::load(opts.source_file, errors);
    if (!sf) {
        for (auto& e : errors.all()) _logger.error(e.format());
        return 1;
    }

    // 版本声明校验
    std::string content = sf->_content;
    if (content.rfind("// next 1.1", 0) == 0) {
        // 版本匹配
    } else if (content.rfind("// next ", 0) == 0) {
        // 提取版本号
        size_t start = 8;
        size_t end = content.find('\n', start);
        std::string ver = content.substr(start, end - start);
        _logger.error("版本不匹配：源文件声明 next " + ver + "，解释器支持 next 1.1");
        return 1;
    }

    // 词法分析
    Lexer lexer;
    auto lex_result = lexer.tokenize(content, sf->_filename);
    if (!lex_result.errors.empty()) {
        for (auto& e : lex_result.errors) _logger.error(e.format());
    }
    if (opts.debug || opts.stop_at == Stage::Lex) {
        print_tokens(lex_result.tokens);
        if (opts.stop_at == Stage::Lex) return lex_result.errors.empty() ? 0 : 1;
    }

    // 语法分析
    Parser parser;
    auto parse_result = parser.parse(lex_result.tokens, sf->_filename);
    if (!parse_result.errors.empty()) {
        for (auto& e : parse_result.errors) _logger.error(e.format());
    }
    if (opts.debug || opts.stop_at == Stage::Parse) {
        if (parse_result.ast) print_ast(*parse_result.ast);
        if (opts.stop_at == Stage::Parse) return parse_result.errors.empty() ? 0 : 2;
    }

    // 语义分析
    SemanticAnalyzer semant;
    auto semant_result = semant.analyze(std::move(parse_result.ast), sf->_filename);
    if (!semant_result.errors.empty()) {
        for (auto& e : semant_result.errors) _logger.error(e.format());
    }
    if (opts.debug || opts.stop_at == Stage::Semant) {
        if (semant_result.symbols) print_symbols(*semant_result.symbols);
        if (opts.stop_at == Stage::Semant) return semant_result.errors.empty() ? 0 : 3;
    }

    // 严格门禁：语义错误非 0 时禁止执行
    if (!semant_result.errors.empty()) return 3;
    if (!lex_result.errors.empty()) return 1;
    if (!parse_result.errors.empty()) return 2;

    // 执行
    if (!semant_result.annotated_ast || !semant_result.symbols) {
        _logger.error("内部错误：语义分析完成但未生成 AST 或符号表");
        return 5;
    }
    Interpreter interp;
    auto exec_result = interp.execute(*semant_result.annotated_ast,
                                      *semant_result.symbols,
                                      std::cout, std::cerr, std::cin, sf->_filename);
    for (auto& e : exec_result.errors) _logger.error(e.format());
    return exec_result.exit_code;
}

int Pipeline::run(const PipelineOptions& opts) {
    if (opts.repl_mode) return 0; // REPL 由 ReplDriver 处理
    if (opts.source_file.empty()) {
        _logger.error("未指定源文件");
        return 1;
    }
    return run_file(opts);
}

std::pair<std::string, bool> Pipeline::eval_line(std::string_view code) {
    Lexer lexer;
    auto lex_result = lexer.tokenize(code, "<repl>");
    if (!lex_result.errors.empty()) {
        std::string r;
        for (auto& e : lex_result.errors) r += e.format() + "\n";
        return {r, false};
    }

    Parser parser;
    parser.set_repl_mode(true);
    auto parse_result = parser.parse(lex_result.tokens, "<repl>");
    if (!parse_result.errors.empty()) {
        std::string r;
        for (auto& e : parse_result.errors) r += e.format() + "\n";
        return {r, false};
    }

    SemanticAnalyzer semant;
    auto semant_result = semant.analyze(std::move(parse_result.ast), "<repl>");
    if (!semant_result.errors.empty()) {
        std::string r;
        for (auto& e : semant_result.errors) r += e.format() + "\n";
        return {r, false};
    }

    if (!semant_result.annotated_ast || !semant_result.symbols) return {"内部错误：语义分析完成但未生成 AST 或符号表\n", false};
    Interpreter interp;
    std::ostringstream out;
    auto exec_result = interp.execute(*semant_result.annotated_ast,
                                      *semant_result.symbols,
                                      out, std::cerr, std::cin, "<repl>");
    std::string r = out.str();
    bool success = exec_result.errors.empty();
    for (auto& e : exec_result.errors) r += e.format() + "\n";
    return {r, success};
}

} // namespace next11