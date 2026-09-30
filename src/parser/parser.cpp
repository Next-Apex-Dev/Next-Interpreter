// 语法分析器实现
#include "parser.hpp"
#include "common/registry.hpp"
#include <cctype>

namespace next11 {

const Token& Parser::peek(size_t off) const {
    if (_tokens.empty()) { static Token eof; eof.kind = TokenKind::Eof; return eof; }
    if (_pos + off >= _tokens.size()) return _tokens.back();
    return _tokens[_pos + off];
}

bool Parser::at_end() const {
    return current().kind == TokenKind::Eof;
}

Token Parser::advance() {
    if (_tokens.empty()) { Token t; t.kind = TokenKind::Eof; return t; }
    Token t = current();
    if (_pos < _tokens.size() - 1) ++_pos;
    return t;
}

bool Parser::check(TokenKind k) const { return current().kind == k; }
bool Parser::check_keyword(const std::string& kw) const {
    return current().kind == TokenKind::Keyword && current().lexeme == kw;
}
bool Parser::check_op(const std::string& op) const {
    return current().kind == TokenKind::Operator && current().lexeme == op;
}
bool Parser::check_delim(const std::string& d) const {
    if (current().kind == TokenKind::Delimiter && current().lexeme == d) return true;
    if (d == ":" && current().kind == TokenKind::TypeAnnot && current().lexeme == d) return true;
    return false;
}
bool Parser::match(TokenKind k) { if (check(k)) { advance(); return true; } return false; }
bool Parser::match_keyword(const std::string& kw) { if (check_keyword(kw)) { advance(); return true; } return false; }
bool Parser::match_op(const std::string& op) { if (check_op(op)) { advance(); return true; } return false; }
bool Parser::match_delim(const std::string& d) { if (check_delim(d)) { advance(); return true; } return false; }

void Parser::error(ErrorCategory cat, std::string code, const Token& t, std::string desc,
                   std::optional<std::string> fix) {
    _errors.emplace_back(cat, std::move(code), t.file, t.line, t.col, std::move(desc), std::move(fix));
}

void Parser::sync_to_statement() {
    while (!at_end() && !check_delim(";") && !check_delim("}") &&
           !check_keyword("let") && !check_keyword("fn") && !check_keyword("def") && !check_keyword("if") &&
           !check_keyword("while") && !check_keyword("for") && !check_keyword("return") &&
           !check_keyword("struct") && !check_keyword("enum") && !check_keyword("type")) {
        advance();
    }
    if (check_delim(";")) advance();
}

void Parser::sync_to_decl() {
    while (!at_end() && !check_keyword("let") && !check_keyword("fn") && !check_keyword("def") &&
           !check_keyword("struct") && !check_keyword("enum") && !check_keyword("type")) {
        advance();
    }
}

SourceRange Parser::range_from(int l, int c) {
    SourceRange r;
    r.line = l; r.col = c; r.end_line = current().line; r.end_col = current().col; r.file = _file;
    return r;
}

ParseResult Parser::parse(const std::vector<Token>& tokens, std::string_view filename) {
    _tokens = tokens;
    _pos = 0;
    _file = std::string(filename);
    _errors.clear();

    auto program = std::make_unique<Program>();

    program->loc.file = _file;

    while (!at_end()) {
        if (check_delim(";")) { advance(); continue; }
        auto d = parse_decl();
        if (d) program->decls.push_back(std::move(d));
        else { advance(); }
    }

    ParseResult result;
    result.ast = std::move(program);
    result.errors = std::move(_errors);
    return result;
}

std::unique_ptr<Decl> Parser::parse_decl() {
    // 装饰器前缀：@decorator fn/def ...
    if (check_op("@")) {
        // 解析装饰器列表，然后解析函数定义
        // 先收集装饰器，再调用 parse_fn_def
        std::vector<std::pair<std::string, std::vector<std::unique_ptr<Expr>>>> saved_decorators;
        while (check_op("@")) {
            int dl = current().line, dc = current().col;
            advance(); // @
            if (!check(TokenKind::Identifier)) {
                error(ErrorCategory::Syn, "SYN035", current(), "装饰器期望标识符",
                      "使用 '@name' 或 '@name(...)' 形式");
                sync_to_decl();
                return nullptr;
            }
            std::string decorator_name = advance().lexeme;
            std::vector<std::unique_ptr<Expr>> decorator_args;
            // 检查是否有参数列表
            if (check_delim("(")) {
                advance(); // (
                // 解析参数表达式
                if (!check_delim(")")) {
                    while (!at_end()) {
                        decorator_args.push_back(parse_expression());
                        if (!match_delim(",")) break;
                    }
                }
                if (!match_delim(")")) {
                    error(ErrorCategory::Syn, "SYN009", current(), "期望 ')' 结束装饰器参数");
                }
            }
            saved_decorators.emplace_back(decorator_name, std::move(decorator_args));
        }
        // 现在应该是 fn 或 def
        if (check_keyword("fn") || check_keyword("def")) {
            auto fn = parse_fn_def();
            if (fn) {
                // 将装饰器添加到函数定义中（从外到内，所以不需要反转）
                fn->decorators = std::move(saved_decorators);
            }
            return fn;
        } else {
            error(ErrorCategory::Syn, "SYN036", current(), "装饰器后期望函数定义",
                  "使用 '@decorator fn name()' 或 '@decorator def name()' 形式");
            sync_to_decl();
            return nullptr;
        }
    }
    if (check_keyword("let")) return parse_let_decl();
    if (check_keyword("async") && (peek(1).kind == TokenKind::Keyword &&
                                   (peek(1).lexeme == "def" || peek(1).lexeme == "fn"))) {
        return parse_async_fn();
    }
    if (check_keyword("fn")) return parse_fn_def();
    if (check_keyword("def")) return parse_fn_def();
    if (check_keyword("struct")) return parse_struct_def();
    if (check_keyword("class")) return parse_class_def();
    if (check_keyword("enum")) return parse_enum_def();
    if (check_keyword("type") && peek(1).kind == TokenKind::Identifier) return parse_type_alias();

    if (check_keyword("from")) return parse_import_stmt();
    if (check_keyword("import")) return parse_import_stmt();

    // REPL 模式：允许顶层表达式语句
    if (_repl_mode) {
        int el = current().line, ec = current().col;
        auto expr = parse_expression();
        if (expr) {
            match_delim(";");
            auto decl = std::make_unique<LetDecl>();
            decl->name = "__repl_expr_" + std::to_string(_repl_expr_counter++);
            decl->init = std::move(expr);
            decl->declared_type.valid = false;
            decl->loc.line = el;
            decl->loc.col = ec;
            return decl;
        }
    }

    error(ErrorCategory::Syn, "SYN005", current(),
          std::string("期望声明关键字，实际 '") + current().lexeme + "'",
          "在顶层只能声明 let/fn/def/struct/enum/type/import");
    sync_to_decl();
    return nullptr;
}

TypeRef Parser::parse_type_ref() {
    TypeRef tr;
    tr.valid = false;
    if (check(TokenKind::Keyword) &&
        (current().lexeme == "int" || current().lexeme == "float" ||
         current().lexeme == "string" || current().lexeme == "bool" ||
         current().lexeme == "void")) {
        tr.name = current().lexeme;
        tr.valid = true;
        advance();
    } else if (check_delim("(")) {
        advance();
        tr.is_tuple = true;
        tr.valid = true;
        if (!check_delim(")")) {
            while (!at_end() && !check_delim(")")) {
                tr.tuple_elems.push_back(parse_type_ref());
                if (!match_delim(",")) break;
            }
        }
        if (!match_delim(")")) {
            error(ErrorCategory::Syn, "SYN009", current(), "期望 ')' 关闭元组类型");
        }
    } else if (check(TokenKind::Identifier)) {
        tr.name = current().lexeme;
        tr.valid = true;
        advance();
        if (match_op("<")) {
            while (!at_end() && !check_op(">")) {
                tr.type_args.push_back(parse_type_ref());
                if (!match_delim(",")) break;
            }
            if (!match_op(">")) {
                error(ErrorCategory::Syn, "SYN006", current(), "期望 '>' 关闭泛型参数列表");
            }
        }
    } else {
        error(ErrorCategory::Syn, "SYN004", current(), "期望类型注解");
        return tr;
    }
    while (check_delim("[") && peek(1).kind == TokenKind::Delimiter && peek(1).lexeme == "]") {
        advance(); advance();
        TypeRef outer;
        outer.is_array = true;
        outer.elem = std::make_shared<TypeRef>(tr);
        outer.valid = true;
        tr = outer;
    }
    return tr;
}

std::vector<Param> Parser::parse_params() {
    std::vector<Param> params;
    if (!match_delim("(")) {
        error(ErrorCategory::Syn, "SYN007", current(), "期望 '(' 开始参数列表");
        return params;
    }
    if (!check_delim(")")) {
        while (!at_end()) {
            Param p;
            if (!check(TokenKind::Identifier)) {
                error(ErrorCategory::Syn, "SYN008", current(), "期望参数名");
                break;
            }
            p.name = advance().lexeme;
            if (!match_delim(":")) {
                error(ErrorCategory::Syn, "SYN004", current(), "参数缺少类型注解");
            }
            p.type = parse_type_ref();
            params.push_back(std::move(p));
            if (!match_delim(",")) break;
        }
    }
    if (!match_delim(")")) {
        error(ErrorCategory::Syn, "SYN009", current(), "期望 ')' 结束参数列表");
    }
    return params;
}

std::unique_ptr<LetDecl> Parser::parse_let_decl() {
    int l = current().line, c = current().col;
    advance(); // let

    auto decl = std::make_unique<LetDecl>();
    decl->loc = range_from(l, c);

    // 解构: [ ... ] 或 { ... } 或 ( ... )
    if (check_delim("[")) {
        decl->is_destructure = true;
        decl->destr_pattern = parse_destructure_pattern();
        if (!match_op("=")) {
            error(ErrorCategory::Syn, "SYN010", current(), "解构声明期望 '='");
            return decl;
        }
        decl->init = parse_expression();
        if (!match_delim(";")) {
            error(ErrorCategory::Syn, "SYN001", current(), "期望 ';'", "在语句末尾添加 ';'");
        }
        return decl;
    }
    if (check_delim("{")) {
        decl->is_destructure = true;
        decl->destr_pattern = parse_destructure_pattern();
        if (!match_op("=")) {
            error(ErrorCategory::Syn, "SYN010", current(), "解构声明期望 '='");
            return decl;
        }
        decl->init = parse_expression();
        if (!match_delim(";")) {
            error(ErrorCategory::Syn, "SYN001", current(), "期望 ';'", "在语句末尾添加 ';'");
        }
        return decl;
    }
    if (check_delim("(")) {
        decl->is_destructure = true;
        decl->destr_pattern = parse_destructure_pattern();
        if (!match_op("=")) {
            error(ErrorCategory::Syn, "SYN010", current(), "解构声明期望 '='");
            return decl;
        }
        decl->init = parse_expression();
        if (!match_delim(";")) {
            error(ErrorCategory::Syn, "SYN001", current(), "期望 ';'", "在语句末尾添加 ';'");
        }
        return decl;
    }

    if (!check(TokenKind::Identifier)) {
        error(ErrorCategory::Syn, "SYN003", current(), "期望变量名", "使用合法标识符作为变量名");
        sync_to_statement();
        return decl;
    }
    decl->name = advance().lexeme;

    if (!match_delim(":")) {
        error(ErrorCategory::Syn, "SYN004", current(), "变量声明缺少类型注解");
    } else {
        decl->declared_type = parse_type_ref();
    }

    if (match_op("=")) {
        decl->init = parse_expression();
    }
    if (!match_delim(";")) {
        error(ErrorCategory::Syn, "SYN001", current(), "期望 ';'", "在语句末尾添加 ';'");
    }
    return decl;
}

std::unique_ptr<FnDef> Parser::parse_fn_def() {
    int l = current().line, c = current().col;
    bool is_def = check_keyword("def");
    advance(); // fn 或 def

    auto fn = std::make_unique<FnDef>();
    fn->loc = range_from(l, c);


    if (!check(TokenKind::Identifier)) {
        error(ErrorCategory::Syn, "SYN011", current(), "期望函数名");
        sync_to_decl();
        return fn;
    }
    fn->name = advance().lexeme;

    // 泛型参数
    if (check_op("<")) {
        advance();
        while (!at_end() && !check_op(">")) {
            if (check(TokenKind::Identifier)) {
                fn->type_params.push_back(advance().lexeme);
            } else {
                error(ErrorCategory::Syn, "SYN012", current(), "期望类型参数名");
                break;
            }
            if (!match_delim(",")) break;
        }
        if (!match_op(">")) {
            error(ErrorCategory::Syn, "SYN006", current(), "期望 '>' 关闭泛型参数");
        }
    }

    fn->params = parse_params();

    // def 可省略返回类型，默认 void
    if (is_def) {
        if (match_op("->")) {
            fn->return_type = parse_type_ref();
        } else {
            fn->return_type.name = "void";
            fn->return_type.valid = true;
        }
    } else {
        if (!match_op("->")) {
            error(ErrorCategory::Syn, "SYN013", current(), "期望 '->' 指定返回类型");
        } else {
            fn->return_type = parse_type_ref();
        }
    }

    fn->body = parse_block();
    return fn;
}

std::unique_ptr<AsyncFnDef> Parser::parse_async_fn() {
    int l = current().line, c = current().col;
    advance(); // async

    // 期望 def 或 fn
    if (!check_keyword("def") && !check_keyword("fn")) {
        error(ErrorCategory::Syn, "SYN030", current(),
              "async 后期望 'def' 或 'fn'",
              "使用 'async def' 或 'async fn' 定义异步函数");
        sync_to_decl();
        auto fn = std::make_unique<AsyncFnDef>();
        fn->loc = range_from(l, c);
        return fn;
    }
    advance(); // def 或 fn

    auto fn = std::make_unique<AsyncFnDef>();
    fn->loc = range_from(l, c);
    fn->is_async = true;

    if (!check(TokenKind::Identifier)) {
        error(ErrorCategory::Syn, "SYN011", current(), "期望异步函数名");
        sync_to_decl();
        return fn;
    }
    fn->name = advance().lexeme;

    // 泛型参数
    if (check_op("<")) {
        advance();
        while (!at_end() && !check_op(">")) {
            if (check(TokenKind::Identifier)) {
                fn->type_params.push_back(advance().lexeme);
            } else {
                error(ErrorCategory::Syn, "SYN012", current(), "期望类型参数名");
                break;
            }
            if (!match_delim(",")) break;
        }
        if (!match_op(">")) {
            error(ErrorCategory::Syn, "SYN006", current(), "期望 '>' 关闭泛型参数");
        }
    }

    fn->params = parse_params();

    // 返回类型：可选 -> Type，默认 void
    if (match_op("->")) {
        fn->return_type = parse_type_ref();
    } else {
        fn->return_type.name = "void";
        fn->return_type.valid = true;
    }

    fn->body = parse_block();
    return fn;
}

std::unique_ptr<StructDef> Parser::parse_struct_def() {
    int l = current().line, c = current().col;
    advance(); // struct

    auto sd = std::make_unique<StructDef>();
    sd->loc = range_from(l, c);

    if (!check(TokenKind::Identifier)) {
        error(ErrorCategory::Syn, "SYN014", current(), "期望结构体名");
        sync_to_decl();
        return sd;
    }
    sd->name = advance().lexeme;

    if (check_op("<")) {
        advance();
        while (!at_end() && !check_op(">")) {
            if (check(TokenKind::Identifier)) sd->type_params.push_back(advance().lexeme);
            if (!match_delim(",")) break;
        }
        match_op(">");
    }

    if (!match_delim("{")) {
        error(ErrorCategory::Syn, "SYN002", current(), "期望 '{' 开始结构体定义");
        return sd;
    }
    while (!at_end() && !check_delim("}")) {
        Param f;
        if (!check(TokenKind::Identifier)) {
            error(ErrorCategory::Syn, "SYN015", current(), "期望字段名");
            advance();
            continue;
        }
        f.name = advance().lexeme;
        if (!match_delim(":")) {
            error(ErrorCategory::Syn, "SYN004", current(), "字段缺少类型注解");
        }
        f.type = parse_type_ref();
        sd->fields.push_back(std::move(f));
        match_delim(",");
        match_delim(";");
    }
    if (!match_delim("}")) {
        error(ErrorCategory::Syn, "SYN002", current(), "期望 '}' 结束结构体定义");
    }
    return sd;
}

std::unique_ptr<EnumDef> Parser::parse_enum_def() {
    int l = current().line, c = current().col;
    advance(); // enum

    auto ed = std::make_unique<EnumDef>();
    ed->loc = range_from(l, c);

    if (!check(TokenKind::Identifier)) {
        error(ErrorCategory::Syn, "SYN016", current(), "期望枚举名");
        sync_to_decl();
        return ed;
    }
    ed->name = advance().lexeme;

    if (!match_delim("{")) {
        error(ErrorCategory::Syn, "SYN002", current(), "期望 '{' 开始枚举定义");
        return ed;
    }
    while (!at_end() && !check_delim("}")) {
        if (check(TokenKind::Identifier)) {
            ed->variants.push_back(advance().lexeme);
        } else {
            error(ErrorCategory::Syn, "SYN017", current(), "期望枚举变体名");
            advance();
        }
        match_delim(",");
    }
    if (!match_delim("}")) {
        error(ErrorCategory::Syn, "SYN002", current(), "期望 '}' 结束枚举定义");
    }
    return ed;
}

std::unique_ptr<ClassDef> Parser::parse_class_def() {
    int l = current().line, c = current().col;
    advance(); // class

    auto cd = std::make_unique<ClassDef>();
    cd->loc = range_from(l, c);

    if (!check(TokenKind::Identifier)) {
        error(ErrorCategory::Syn, "SYN018", current(), "期望类名");
        sync_to_decl();
        return cd;
    }
    cd->name = advance().lexeme;

    // 继承: extends BaseClass 或 extends (Base1, Base2)
    if (check_keyword("extends")) {
        advance(); // extends
        if (match_delim("(")) {
            while (!at_end() && !check_delim(")")) {
                if (check(TokenKind::Identifier)) cd->base_classes.push_back(advance().lexeme);
                if (!match_delim(",")) break;
            }
            match_delim(")");
        } else if (check(TokenKind::Identifier)) {
            cd->base_class = advance().lexeme;
            cd->base_classes.push_back(cd->base_class);
        }
    }

    if (!match_delim("{")) {
        error(ErrorCategory::Syn, "SYN002", current(), "期望 '{' 开始类定义");
        return cd;
    }
    while (!at_end() && !check_delim("}")) {
        // 访问修饰符前缀: private/protected/public
        int member_access = 0;
        if (check_keyword("private")) { member_access = 2; advance(); }
        else if (check_keyword("protected")) { member_access = 1; advance(); }
        else if (check_keyword("public")) { member_access = 0; advance(); }
        // 方法: def name() { ... }
        if (check_keyword("def") || check_keyword("fn")) {
            auto fn = parse_fn_def();
            if (fn) {
                fn->access_level = member_access;
                cd->methods.push_back(std::move(fn));
            }
            continue;
        }
        // 字段: let name: Type
        if (check_keyword("let")) {
            auto ld = parse_let_decl();
            if (ld && !ld->name.empty()) {
                Param f;
                f.name = ld->name;
                f.type = ld->declared_type;
                f.access_level = member_access;
                cd->fields.push_back(std::move(f));
                if (ld->init.has_value() && ld->init.value()) {
                    cd->field_inits[ld->name] = std::move(ld->init.value());
                }
            }
            continue;
        }
        error(ErrorCategory::Syn, "SYN015", current(), "期望字段或方法");
        advance();
    }
    if (!match_delim("}")) {
        error(ErrorCategory::Syn, "SYN002", current(), "期望 '}' 结束类定义");
    }
    return cd;
}

std::unique_ptr<TypeAlias> Parser::parse_type_alias() {
    int l = current().line, c = current().col;
    advance(); // type

    auto ta = std::make_unique<TypeAlias>();
    ta->loc = range_from(l, c);

    if (!check(TokenKind::Identifier)) {
        error(ErrorCategory::Syn, "SYN018", current(), "期望类型别名名");
        sync_to_decl();
        return ta;
    }
    ta->name = advance().lexeme;

    if (check_op("<")) {
        advance();
        while (!at_end() && !check_op(">")) {
            if (check(TokenKind::Identifier)) ta->type_params.push_back(advance().lexeme);
            if (!match_delim(",")) break;
        }
        match_op(">");
    }

    if (!match_op("=")) {
        error(ErrorCategory::Syn, "SYN010", current(), "类型别名期望 '='");
    }
    ta->aliased = parse_type_ref();
    if (!match_delim(";")) {
        error(ErrorCategory::Syn, "SYN001", current(), "期望 ';'", "在语句末尾添加 ';'");
    }
    return ta;
}

std::unique_ptr<Stmt> Parser::parse_stmt() {
    // 查询 StmtRegistry 验证语句关键字是否已注册
    if (check(TokenKind::Keyword)) {
        auto& sr = StmtRegistry::instance();
        std::string kw = current().lexeme;
        if (sr.has(kw)) {
            // 已注册的语句关键字，分派到对应解析方法
            if (kw == "let") return parse_let_decl();
            if (kw == "fn" || kw == "def") return parse_fn_def();
            if (kw == "if") return parse_if_stmt();
            if (kw == "while") return parse_while_stmt();
            if (kw == "for") return parse_for_stmt();
            if (kw == "return") return parse_return_stmt();
            if (kw == "break") {
                int l = current().line, c = current().col;
                advance();
                if (!match_delim(";")) error(ErrorCategory::Syn, "SYN001", current(), "期望 ';'", "在语句末尾添加 ';'");
                auto bs = std::make_unique<BreakStmt>();
                bs->loc = range_from(l, c);
                return bs;
            }
            if (kw == "continue") {
                int l = current().line, c = current().col;
                advance();
                if (!match_delim(";")) error(ErrorCategory::Syn, "SYN001", current(), "期望 ';'", "在语句末尾添加 ';'");
                auto cs = std::make_unique<ContinueStmt>();
                cs->loc = range_from(l, c);
                return cs;
            }
            if (kw == "with") {
                return parse_with_stmt();
            }
            if (kw == "async" && peek(1).kind == TokenKind::Keyword &&
                peek(1).lexeme == "with") {
                return parse_with_stmt();
            }
            if (kw == "try") {
                return parse_try_stmt();
            }
            if (kw == "throw") {
                return parse_throw_expr();
            }
            if (kw == "del") {
                return parse_del_stmt();
            }
        }
    }
    if (check_delim("{")) {
        int l = current().line, c = current().col;
        auto block = parse_block();
        auto es = std::make_unique<ExprStmt>();
        es->loc = range_from(l, c);
        es->expr = std::move(block);
        return es;
    }

    // 表达式语句
    int l = current().line, c = current().col;
    auto expr = parse_expression();
    if (!match_delim(";")) {
        error(ErrorCategory::Syn, "SYN001", current(), "期望 ';'", "在语句末尾添加 ';'");
    }
    auto es = std::make_unique<ExprStmt>();
    es->loc = range_from(l, c);
    es->expr = std::move(expr);
    return es;
}

std::unique_ptr<Expr> Parser::parse_block() {
    int l = current().line, c = current().col;
    if (!match_delim("{")) {
        error(ErrorCategory::Syn, "SYN002", current(), "期望 '{' 开始块");
        return std::make_unique<Block>();
    }
    auto block = std::make_unique<Block>();
    block->loc = range_from(l, c);
    while (!at_end() && !check_delim("}")) {
        auto s = parse_stmt();
        if (s) block->stmts.push_back(std::move(s));
        else { advance(); }
    }
    if (!match_delim("}")) {
        error(ErrorCategory::Syn, "SYN002", current(), "期望 '}' 结束块");
    }
    return block;
}

std::unique_ptr<IfStmt> Parser::parse_if_stmt() {
    int l = current().line, c = current().col;
    advance(); // if
    auto stmt = std::make_unique<IfStmt>();
    stmt->loc = range_from(l, c);
    bool saved = _allow_struct_lit;
    _allow_struct_lit = false;
    bool saved_gen = _allow_generic_args;
    _allow_generic_args = false;
    stmt->cond = parse_expression();
    _allow_struct_lit = saved;
    _allow_generic_args = saved_gen;
    stmt->then_block = parse_block();
    if (match_keyword("else")) {
        if (check_keyword("if")) {
            auto inner = parse_if_stmt();
            auto wrap = std::make_unique<Block>();
            wrap->stmts.push_back(std::move(inner));
            stmt->else_block = std::move(wrap);
        } else {
            stmt->else_block = parse_block();
        }
    }
    return stmt;
}

std::unique_ptr<WhileStmt> Parser::parse_while_stmt() {
    int l = current().line, c = current().col;
    advance(); // while
    auto stmt = std::make_unique<WhileStmt>();
    stmt->loc = range_from(l, c);
    bool saved = _allow_struct_lit;
    _allow_struct_lit = false;
    bool saved_gen = _allow_generic_args;
    _allow_generic_args = false;
    stmt->cond = parse_expression();
    _allow_struct_lit = saved;
    _allow_generic_args = saved_gen;
    stmt->body = parse_block();
    return stmt;
}

std::unique_ptr<ForStmt> Parser::parse_for_stmt() {
    int l = current().line, c = current().col;
    advance(); // for
    auto stmt = std::make_unique<ForStmt>();
    stmt->loc = range_from(l, c);
    match_delim("("); // 可选括号
    if (!check(TokenKind::Identifier)) {
        error(ErrorCategory::Syn, "SYN019", current(), "for 循环期望变量名");
        return stmt;
    }
    stmt->var_name = advance().lexeme;
    if (!match_keyword("in")) {
        error(ErrorCategory::Syn, "SYN020", current(), "期望 'in'");
    }
    bool saved = _allow_struct_lit;
    _allow_struct_lit = false;
    bool saved_gen = _allow_generic_args;
    _allow_generic_args = false;
    stmt->iterable = parse_expression();
    _allow_struct_lit = saved;
    _allow_generic_args = saved_gen;
    match_delim(")"); // 可选括号
    stmt->body = parse_block();
    return stmt;
}

std::unique_ptr<ReturnStmt> Parser::parse_return_stmt() {
    int l = current().line, c = current().col;
    advance(); // return
    auto stmt = std::make_unique<ReturnStmt>();
    stmt->loc = range_from(l, c);
    if (!check_delim(";")) {
        stmt->value = parse_expression();
    }
    if (!match_delim(";")) {
        error(ErrorCategory::Syn, "SYN001", current(), "期望 ';'", "在语句末尾添加 ';'");
    }
    return stmt;
}

std::unique_ptr<WithStmt> Parser::parse_with_stmt() {
    int l = current().line, c = current().col;
    auto stmt = std::make_unique<WithStmt>();
    stmt->loc = range_from(l, c);

    // 检测 async with
    if (check_keyword("async")) {
        stmt->is_async = true;
        advance(); // async
        if (!check_keyword("with")) {
            error(ErrorCategory::Syn, "SYN031", current(),
                  "async 后期望 'with'",
                  "使用 'async with' 形式");
            return stmt;
        }
    }
    advance(); // with

    // 解析上下文表达式
    bool saved = _allow_struct_lit;
    _allow_struct_lit = false;
    stmt->context_expr = parse_expression();
    _allow_struct_lit = saved;

    // 可选 as name
    if (match_keyword("as")) {
        if (!check(TokenKind::Identifier)) {
            error(ErrorCategory::Syn, "SYN032", current(), "with as 后期望标识符");
        } else {
            stmt->as_name = advance().lexeme;
        }
    }

    // 解析 with 块
    stmt->with_block = parse_block();
    return stmt;
}

std::unique_ptr<Stmt> Parser::parse_try_stmt() {
    int l = current().line, c = current().col;
    advance(); // try
    auto stmt = std::make_unique<TryStmt>();
    stmt->loc = range_from(l, c);

    // try 块
    stmt->try_block = parse_block();

    // catch 块（可选）：catch (E1, E2) as e { ... }
    if (match_keyword("catch")) {
        // 多类型列表，用括号包裹
        if (match_delim("(")) {
            while (!at_end() && !check_delim(")")) {
                if (check(TokenKind::Identifier)) {
                    stmt->catch_types.push_back(advance().lexeme);
                } else {
                    error(ErrorCategory::Syn, "SYN033", current(),
                          "catch 期望异常类型名",
                          "在 catch (...) 中使用标识符作为异常类型");
                    advance();
                }
                if (!match_delim(",")) break;
            }
            if (!match_delim(")")) {
                error(ErrorCategory::Syn, "SYN009", current(),
                      "期望 ')' 结束 catch 类型列表");
            }
        }
        // as e 绑定异常变量（as 为上下文关键字，可能被词法化为标识符）
        if (check_keyword("as") || (check(TokenKind::Identifier) && current().lexeme == "as")) {
            advance(); // as
            if (!check(TokenKind::Identifier)) {
                error(ErrorCategory::Syn, "SYN034", current(),
                      "catch as 后期望标识符",
                      "使用 'catch (...) as 变量名' 形式");
            } else {
                stmt->catch_var = advance().lexeme;
            }
        }
        // catch 块
        stmt->catch_block = parse_block();
    }

    // finally 块（可选）
    if (match_keyword("finally")) {
        stmt->finally_block = parse_block();
    }

    return stmt;
}

std::unique_ptr<Stmt> Parser::parse_throw_expr() {
    int l = current().line, c = current().col;
    advance(); // throw
    auto stmt = std::make_unique<ThrowStmt>();
    stmt->loc = range_from(l, c);
    // throw 后跟可选表达式（throw; 表示重新抛出）
    if (!check_delim(";")) {
        stmt->expr = parse_expression();
    }
    if (!match_delim(";")) {
        error(ErrorCategory::Syn, "SYN001", current(), "期望 ';'", "在语句末尾添加 ';'");
    }
    return stmt;
}

std::unique_ptr<Stmt> Parser::parse_del_stmt() {
    int l = current().line, c = current().col;
    advance(); // del
    auto stmt = std::make_unique<DelStmt>();
    stmt->loc = range_from(l, c);
    // del 后跟索引表达式（dict[key]）或成员表达式（obj.attr）
    auto target = parse_postfix();
    if (!target || (target->node_kind != NodeKind::IndexExpr && target->node_kind != NodeKind::MemberExpr)) {
        error(ErrorCategory::Syn, "SYN029", current(), "del 期望索引表达式或成员表达式");
    }
    stmt->target = std::move(target);
    if (!match_delim(";")) {
        error(ErrorCategory::Syn, "SYN001", current(), "期望 ';'", "在语句末尾添加 ';'");
    }
    return stmt;
}

std::unique_ptr<Expr> Parser::parse_expression() {
    return parse_assignment();
}

std::unique_ptr<Expr> Parser::parse_assignment() {
    auto lhs = parse_pipe();
    if (check_op("=")) {
        int l = current().line, c = current().col;
        advance();
        auto rhs = parse_assignment();
        auto node = std::make_unique<AssignExpr>();
        node->loc = range_from(l, c);
        node->target = std::move(lhs);
        node->value = std::move(rhs);
        return node;
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parse_pipe() {
    auto lhs = parse_logical_or();
    // 优先匹配 |> 管道运算符
    while (check_op("|>")) {
        int l = current().line, c = current().col;
        advance();
        auto rhs = parse_logical_or();
        auto node = std::make_unique<PipeExpr>();
        node->loc = range_from(l, c);
        node->lhs = std::move(lhs);
        if (rhs && rhs->node_kind == NodeKind::CallExpr) {
            node->call = std::unique_ptr<CallExpr>(static_cast<CallExpr*>(rhs.release()));
        } else if (rhs && rhs->node_kind == NodeKind::IdentExpr) {
            // 管道右侧为函数名，包装为无参调用
            auto ce = std::make_unique<CallExpr>();
            ce->callee = std::move(rhs);
            node->call = std::move(ce);
        } else {
            error(ErrorCategory::Syn, "PIP001", current(),
                  "管道右侧必须为函数调用");
            auto ce = std::make_unique<CallExpr>();
            ce->callee = std::move(rhs);
            node->call = std::move(ce);
        }
        lhs = std::move(node);
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parse_logical_or() {
    auto lhs = parse_logical_and();
    // 优先匹配 || 逻辑或
    while (check_op("||")) {
        int l = current().line, c = current().col;
        std::string op = advance().lexeme;
        auto rhs = parse_logical_and();
        auto node = std::make_unique<BinaryExpr>();
        node->loc = range_from(l, c);
        node->op = op; node->lhs = std::move(lhs); node->rhs = std::move(rhs);
        lhs = std::move(node);
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parse_set_union() {
    auto lhs = parse_set_symdiff();
    while (check_op("|")) {
        int l = current().line, c = current().col;
        std::string op = advance().lexeme;
        auto rhs = parse_set_symdiff();
        auto node = std::make_unique<BinaryExpr>();
        node->loc = range_from(l, c);
        node->op = op; node->lhs = std::move(lhs); node->rhs = std::move(rhs);
        lhs = std::move(node);
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parse_set_intersection() {
    auto lhs = parse_comparison();
    while (check_op("&")) {
        int l = current().line, c = current().col;
        std::string op = advance().lexeme;
        auto rhs = parse_comparison();
        auto node = std::make_unique<BinaryExpr>();
        node->loc = range_from(l, c);
        node->op = op; node->lhs = std::move(lhs); node->rhs = std::move(rhs);
        lhs = std::move(node);
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parse_set_symdiff() {
    auto lhs = parse_set_intersection();
    while (check_op("^")) {
        int l = current().line, c = current().col;
        std::string op = advance().lexeme;
        auto rhs = parse_set_intersection();
        auto node = std::make_unique<BinaryExpr>();
        node->loc = range_from(l, c);
        node->op = op; node->lhs = std::move(lhs); node->rhs = std::move(rhs);
        lhs = std::move(node);
    }
    return lhs;
}


std::unique_ptr<Expr> Parser::parse_logical_and() {
    auto lhs = parse_set_union();
    while (check_op("&&")) {
        int l = current().line, c = current().col;
        std::string op = advance().lexeme;
        auto rhs = parse_set_union();
        auto node = std::make_unique<BinaryExpr>();
        node->loc = range_from(l, c);
        node->op = op; node->lhs = std::move(lhs); node->rhs = std::move(rhs);
        lhs = std::move(node);
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parse_comparison() {
    auto lhs = parse_additive();
    while (check_op("==") || check_op("!=") || check_op("<") ||
           check_op("<=") || check_op(">") || check_op(">=") ||
           check_keyword("in")) {
        int l = current().line, c = current().col;
        std::string op = advance().lexeme;
        auto rhs = parse_additive();
        auto node = std::make_unique<BinaryExpr>();
        node->loc = range_from(l, c);
        node->op = op; node->lhs = std::move(lhs); node->rhs = std::move(rhs);
        lhs = std::move(node);
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parse_additive() {
    auto lhs = parse_multiplicative();
    while (check_op("+") || check_op("-")) {
        int l = current().line, c = current().col;
        std::string op = advance().lexeme;
        auto rhs = parse_multiplicative();
        auto node = std::make_unique<BinaryExpr>();
        node->loc = range_from(l, c);
        node->op = op; node->lhs = std::move(lhs); node->rhs = std::move(rhs);
        lhs = std::move(node);
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parse_multiplicative() {
    auto lhs = parse_unary();
    while (check_op("*") || check_op("/") || check_op("%")) {
        int l = current().line, c = current().col;
        std::string op = advance().lexeme;
        auto rhs = parse_unary();
        auto node = std::make_unique<BinaryExpr>();
        node->loc = range_from(l, c);
        node->op = op; node->lhs = std::move(lhs); node->rhs = std::move(rhs);
        lhs = std::move(node);
    }
    return lhs;
}

std::unique_ptr<Expr> Parser::parse_unary() {
    if (check_op("-") || check_op("!")) {
        int l = current().line, c = current().col;
        std::string op = advance().lexeme;
        auto operand = parse_unary();
        auto node = std::make_unique<UnaryExpr>();
        node->loc = range_from(l, c);
        node->op = op; node->operand = std::move(operand);
        return node;
    }
    if (check_keyword("await")) {
        return parse_await_expr();
    }
    if (check_keyword("yield")) {
        return parse_yield_expr();
    }
    return parse_postfix();
}

std::unique_ptr<Expr> Parser::parse_postfix() {
    auto expr = parse_primary();
    while (true) {
        if (check_delim("[")) {
            int l = current().line, c = current().col;
            advance();
            auto idx = parse_expression();
            if (!match_delim("]")) {
                error(ErrorCategory::Syn, "SYN021", current(), "期望 ']'");
            }
            auto node = std::make_unique<IndexExpr>();
            node->loc = range_from(l, c);
            node->array = std::move(expr);
            node->index = std::move(idx);
            expr = std::move(node);
        } else if (check_delim(".")) {
            int l = current().line, c = current().col;
            advance();
            if (!check(TokenKind::Identifier)) {
                error(ErrorCategory::Syn, "SYN022", current(), "期望成员名");
                break;
            }
            std::string member = advance().lexeme;
            auto node = std::make_unique<MemberExpr>();
            node->loc = range_from(l, c);
            node->object = std::move(expr);
            node->member = member;
            if (check_delim("(")) {
                expr = parse_call(std::move(node));
            } else {
                expr = std::move(node);
            }
        } else if (check_delim("(")) {
            expr = parse_call(std::move(expr));
        } else if (check_delim("{") && expr && expr->node_kind == NodeKind::IdentExpr && _allow_struct_lit) {
            int l = current().line, c = current().col;
            std::string name = static_cast<IdentExpr*>(expr.get())->name;
            advance(); // {
            auto node = std::make_unique<StructLitExpr>();
            node->loc = range_from(l, c);
            node->struct_name = name;
            while (!at_end() && !check_delim("}")) {
                if (!check(TokenKind::Identifier)) {
                    error(ErrorCategory::Syn, "SYN028", current(), "结构体字面量期望字段名");
                    advance();
                    break;
                }
                std::string field = advance().lexeme;
                if (!match_delim(":")) {
                    error(ErrorCategory::Syn, "SYN004", current(), "字段缺少 ':'");
                }
                auto val = parse_expression();
                node->fields.emplace_back(field, std::move(val));
                if (!match_delim(",")) break;
            }
            if (!match_delim("}")) {
                error(ErrorCategory::Syn, "SYN002", current(), "期望 '}' 结束结构体字面量");
            }
            expr = std::move(node);
        } else {
            break;
        }
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_call(std::unique_ptr<Expr> callee) {
    int l = current().line, c = current().col;
    advance(); // (
    auto node = std::make_unique<CallExpr>();
    node->loc = range_from(l, c);
    node->callee = std::move(callee);
    if (!check_delim(")")) {
        while (!at_end()) {
            node->args.push_back(parse_expression());
            if (!match_delim(",")) break;
        }
    }
    if (!match_delim(")")) {
        error(ErrorCategory::Syn, "SYN009", current(), "期望 ')' 结束调用参数");
    }
    return node;
}

std::unique_ptr<Expr> Parser::parse_primary() {
    int l = current().line, c = current().col;
    const Token& t = current();

    if (t.kind == TokenKind::IntLiteral) {
        advance();
        auto node = std::make_unique<LiteralExpr>();
        node->loc = range_from(l, c);
        node->lit_kind = LiteralExpr::Int;
        try {
            node->int_val = std::stoll(t.lexeme);
        } catch (const std::exception&) {
            try {
                unsigned long long uval = std::stoull(t.lexeme);
                if (uval > static_cast<unsigned long long>(INT64_MAX))
                    error(ErrorCategory::Syn, "SYN040", t, "整数字面量超出 int64 范围: " + t.lexeme);
                node->int_val = static_cast<int64_t>(uval);
            } catch (const std::exception&) {
                error(ErrorCategory::Syn, "SYN040", t, "整数字面量解析失败: " + t.lexeme);
                node->int_val = 0;
            }
        }
        return node;
    }
    if (t.kind == TokenKind::FloatLiteral) {
        advance();
        auto node = std::make_unique<LiteralExpr>();
        node->loc = range_from(l, c);
        node->lit_kind = LiteralExpr::Float;
        try {
            node->float_val = std::stod(t.lexeme);
        } catch (const std::exception&) {
            error(ErrorCategory::Syn, "SYN041", t, "浮点数字面量解析失败: " + t.lexeme);
            node->float_val = 0.0;
        }
        return node;
    }
    if (t.kind == TokenKind::StringLiteral) {
        advance();
        auto node = std::make_unique<LiteralExpr>();
        node->loc = range_from(l, c);
        node->lit_kind = LiteralExpr::String;
        node->str_val = t.lexeme;
        return node;
    }
    if (t.kind == TokenKind::BoolLiteral) {
        advance();
        auto node = std::make_unique<LiteralExpr>();
        node->loc = range_from(l, c);
        node->lit_kind = LiteralExpr::Bool;
        node->bool_val = (t.lexeme == "true");
        return node;
    }
    if (t.kind == TokenKind::ComplexLiteral) {
        advance();
        auto node = std::make_unique<LiteralExpr>();
        node->loc = range_from(l, c);
        node->lit_kind = LiteralExpr::Complex;
        // 解析复数字面量：格式为 real+imagi 或 real-imagi
        std::string lex = t.lexeme;
        size_t plus_pos = lex.find('+');
        size_t minus_pos = lex.find('-', 1); // 从位置1开始找，跳过可能的负号
        size_t i_pos = lex.find('i');
        if (plus_pos != std::string::npos) {
            // real+imagi 格式
            std::string real_str = lex.substr(0, plus_pos);
            std::string imag_str = lex.substr(plus_pos + 1, i_pos - plus_pos - 1);
            try {
                node->complex_real = std::stod(real_str);
                node->complex_imag = std::stod(imag_str);
            } catch (...) {
                error(ErrorCategory::Syn, "SYN042", t, "复数字面量解析失败: " + t.lexeme);
                node->complex_real = 0.0;
                node->complex_imag = 0.0;
            }
        } else if (minus_pos != std::string::npos) {
            // real-imagi 格式
            std::string real_str = lex.substr(0, minus_pos);
            std::string imag_str = lex.substr(minus_pos + 1, i_pos - minus_pos - 1);
            try {
                node->complex_real = std::stod(real_str);
                node->complex_imag = -std::stod(imag_str);
            } catch (...) {
                error(ErrorCategory::Syn, "SYN042", t, "复数字面量解析失败: " + t.lexeme);
                node->complex_real = 0.0;
                node->complex_imag = 0.0;
            }
        } else {
            // 纯虚数：如 4i
            std::string imag_str = lex.substr(0, i_pos);
            try {
                node->complex_real = 0.0;
                node->complex_imag = std::stod(imag_str);
            } catch (...) {
                error(ErrorCategory::Syn, "SYN042", t, "复数字面量解析失败: " + t.lexeme);
                node->complex_real = 0.0;
                node->complex_imag = 0.0;
            }
        }
        return node;
    }
    if (t.kind == TokenKind::Identifier ||
        (t.kind == TokenKind::Keyword && t.lexeme == "type")) {
        advance();
        auto node = std::make_unique<IdentExpr>();
        node->loc = range_from(l, c);
        node->name = t.lexeme;
        // 泛型实参标注
        if (_allow_generic_args && check_op("<") && peek(1).kind == TokenKind::Identifier) {
            advance();
            while (!at_end() && !check_op(">")) {
                // 仅跳过，类型实参记录在 CallExpr 中处理
                if (!match_delim(",")) {
                    if (!at_end() && !check_op(">")) advance();
                }
            }
            match_op(">");
        }
        return node;
    }
    if (check_delim("(")) {
        advance();
        auto inner = parse_expression();
        // 元组：(expr, expr, ...)
        if (check_delim(",")) {
            auto node = std::make_unique<TupleExpr>();
            node->loc = range_from(l, c);
            if (inner) node->elements.push_back(std::move(inner));
            while (match_delim(",")) {
                if (check_delim(")")) break;
                node->elements.push_back(parse_expression());
            }
            if (!match_delim(")")) {
                error(ErrorCategory::Syn, "SYN009", current(), "期望 ')'");
            }
            return node;
        }
        if (!match_delim(")")) {
            error(ErrorCategory::Syn, "SYN009", current(), "期望 ')'");
        }
        auto node = std::make_unique<GroupExpr>();
        node->loc = range_from(l, c);
        node->inner = std::move(inner);
        return node;
    }
    if (check_delim("[")) {
        advance();
        auto node = std::make_unique<ArrayExpr>();
        node->loc = range_from(l, c);
        if (!check_delim("]")) {
            while (!at_end()) {
                node->elements.push_back(parse_expression());
                if (!match_delim(",")) break;
            }
        }
        if (!match_delim("]")) {
            error(ErrorCategory::Syn, "SYN021", current(), "期望 ']'");
        }
        return node;
    }
    if (check_keyword("match")) {
        return parse_match_expr();
    }
    if (check_keyword("super")) {
        advance();
        auto node = std::make_unique<SuperExpr>();
        node->loc = range_from(l, c);
        return node;
    }
    if (check_delim("{")) {
        // 结构体字面量/字典/集合/块：尝试结构体字面量 Name{...} 由 postfix 处理，这里仅块/字典/集合
        int blk_l = current().line, blk_c = current().col;
        advance(); // {
        // 检查是否是空字典 {}
        if (check_delim("}")) {
            advance();
            auto dict = std::make_unique<DictExpr>();
            dict->loc = range_from(blk_l, blk_c);
            return dict;
        }
        // 尝试解析第一个元素
        auto first = parse_expression();
        if (!first) {
            // 解析失败，报错并返回空块
            error(ErrorCategory::Syn, "SYN043", current(), "块/字典/集合内表达式解析失败");
            auto block = std::make_unique<Block>();
            block->loc = range_from(blk_l, blk_c);
            match_delim("}");
            return block;
        }
        // 检查是否是字典（有 : 分隔符）
        if (match_delim(":")) {
            auto dict = std::make_unique<DictExpr>();
            dict->loc = range_from(blk_l, blk_c);
            // 第一个键值对
            dict->pairs.emplace_back(std::move(first), parse_expression());
            dict->pair_is_unpack.push_back(false);
            // 解析剩余键值对
            while (match_delim(",")) {
                if (check_delim("}")) break;
                auto key = parse_expression();
                if (!match_delim(":")) {
                    error(ErrorCategory::Syn, "SYN004", current(), "字典期望 ':' 分隔键值");
                    break;
                }
                auto val = parse_expression();
                dict->pairs.emplace_back(std::move(key), std::move(val));
                dict->pair_is_unpack.push_back(false);
            }
            if (!match_delim("}")) {
                error(ErrorCategory::Syn, "SYN002", current(), "期望 '}' 结束字典字面量");
            }
            return dict;
        } else {
            // 集合
            auto set = std::make_unique<SetExpr>();
            set->loc = range_from(blk_l, blk_c);
            set->elements.push_back(std::move(first));
            set->element_is_unpack.push_back(false);
            while (match_delim(",")) {
                if (check_delim("}")) break;
                set->elements.push_back(parse_expression());
                set->element_is_unpack.push_back(false);
            }
            if (!match_delim("}")) {
                error(ErrorCategory::Syn, "SYN002", current(), "期望 '}' 结束集合字面量");
            }
            return set;
        }
    }

    error(ErrorCategory::Syn, "SYN023", t,
          std::string("意外的 Token '") + t.lexeme + "'",
          "检查语法结构是否正确，参考语言文档");
    advance();
    auto node = std::make_unique<LiteralExpr>();
    node->loc = range_from(l, c);
    node->lit_kind = LiteralExpr::Int;
    return node;
}

std::unique_ptr<Expr> Parser::parse_await_expr() {
    int l = current().line, c = current().col;
    advance(); // await
    auto node = std::make_unique<AwaitExpr>();
    node->loc = range_from(l, c);
    // await 后续为一元表达式（避免与赋值/管道冲突）
    node->await_expr = parse_unary();
    return node;
}

std::unique_ptr<Expr> Parser::parse_yield_expr() {
    int l = current().line, c = current().col;
    advance(); // yield
    auto node = std::make_unique<YieldExpr>();
    node->loc = range_from(l, c);
    // yield 后续为可选表达式（yield; 或 yield value;）
    // 不检查分号，因为分号由语句解析器处理
    if (!check_delim(";") && !check_delim("}") && !at_end()) {
        node->value = parse_expression();
    }
    return node;
}

std::unique_ptr<MatchExpr> Parser::parse_match_expr() {
    int l = current().line, c = current().col;
    advance(); // match
    auto node = std::make_unique<MatchExpr>();
    node->loc = range_from(l, c);
    bool saved_allow_struct_lit = _allow_struct_lit;
    _allow_struct_lit = false;
    node->scrutinee = parse_expression();
    _allow_struct_lit = saved_allow_struct_lit;
    if (!match_delim("{")) {
        error(ErrorCategory::Syn, "SYN002", current(), "match 表达式期望 '{'");
        return node;
    }
    while (!at_end() && !check_delim("}")) {
        node->arms.push_back(parse_match_arm());
        match_delim(",");
    }
    if (!match_delim("}")) {
        error(ErrorCategory::Syn, "SYN002", current(), "期望 '}' 结束 match");
    }
    return node;
}

MatchArm Parser::parse_match_arm() {
    MatchArm arm;

    // 通配符 _ 作为标识符处理
    if (check(TokenKind::Identifier) && current().lexeme == "_") {
        advance();
        arm.is_wildcard = true;
        auto pat = std::make_unique<IdentExpr>();
        pat->name = "_";
        arm.pattern = std::move(pat);
    } else if (check(TokenKind::IntLiteral) || check(TokenKind::FloatLiteral) ||
               check(TokenKind::StringLiteral) || check(TokenKind::BoolLiteral)) {
        arm.pattern = parse_primary();
    } else if (check(TokenKind::Identifier)) {
        // 变量绑定模式
        arm.is_var_bind = true;
        arm.var_name = current().lexeme;
        arm.pattern = parse_primary();
    } else {
        error(ErrorCategory::Syn, "SYN024", current(), "期望 match 模式");
        advance();
    }

    // 守卫条件
    if (check_keyword("if")) {
        advance();
        arm.guard = parse_expression();
    }

    if (!match_op("=>")) {
        error(ErrorCategory::Syn, "SYN025", current(), "期望 '=>'");
    }

    arm.result = parse_expression();
    return arm;
}

DestructurePattern Parser::parse_destructure_pattern() {
    DestructurePattern dp;
    if (match_delim("[")) {
        dp.is_array = true;
        while (!at_end() && !check_delim("]")) {
            if (check(TokenKind::Identifier) && current().lexeme == "_") {
                advance();
                DestructurePattern sub;
                sub.is_wildcard = true;
                dp.elements.push_back(sub);
            } else if (check_delim("[")) {
                dp.elements.push_back(parse_destructure_pattern());

            } else if (check(TokenKind::Identifier)) {
                DestructurePattern sub;
                sub.is_leaf = true;
                sub.var_name = advance().lexeme;
                dp.elements.push_back(sub);
            } else {
                error(ErrorCategory::Syn, "SYN026", current(), "解构模式期望变量名或子模式");
                advance();
            }
            if (!match_delim(",")) break;
        }
        if (!match_delim("]")) {
            error(ErrorCategory::Syn, "SYN021", current(), "期望 ']'");
        }
    } else if (match_delim("{")) {
        dp.is_struct = true;
        while (!at_end() && !check_delim("}")) {
            if (check(TokenKind::Identifier)) {
                std::string name = advance().lexeme;
                DestructurePattern sub;
                sub.is_leaf = true;
                sub.var_name = name;
                dp.fields.emplace_back(name, sub);
            } else {
                error(ErrorCategory::Syn, "SYN027", current(), "结构体解构期望字段名");
                advance();
            }
            if (!match_delim(",")) break;
        }
        if (!match_delim("}")) {
            error(ErrorCategory::Syn, "SYN002", current(), "期望 '}'");
        }
    } else if (match_delim("(")) {
        dp.is_tuple = true;
        while (!at_end() && !check_delim(")")) {
            if (check(TokenKind::Identifier) && current().lexeme == "_") {
                advance();
                DestructurePattern sub;
                sub.is_wildcard = true;
                dp.elements.push_back(sub);
            } else if (check_delim("(") || check_delim("[")) {
                dp.elements.push_back(parse_destructure_pattern());
            } else if (check(TokenKind::Identifier)) {
                DestructurePattern sub;
                sub.is_leaf = true;
                sub.var_name = advance().lexeme;
                dp.elements.push_back(sub);
            } else {
                error(ErrorCategory::Syn, "SYN026", current(), "解构模式期望变量名或子模式");
                advance();
            }
            if (!match_delim(",")) break;
        }
        if (!match_delim(")")) {
            error(ErrorCategory::Syn, "SYN009", current(), "期望 ')'");
        }
    }
    return dp;
}

std::unique_ptr<Decl> Parser::parse_import_stmt() {
    int l = current().line, c = current().col;
    
    // 检查是否为 from import
    if (check_keyword("from")) {
        // from mod import name 语法
        advance(); // from
        auto stmt = std::make_unique<ImportStmt>();
        stmt->loc = range_from(l, c);
        stmt->is_from_import = true;

        // 解析模块路径
        if (!check(TokenKind::Identifier)) {
            error(ErrorCategory::Syn, "SYN023", current(), "from 后期望模块名");
            sync_to_statement();
            return stmt;
        }
        while (true) {
            stmt->module_path.push_back(advance().lexeme);
            if (!match_delim(".")) break;
            if (!check(TokenKind::Identifier)) {
                error(ErrorCategory::Syn, "SYN023", current(), "期望子模块名");
                break;
            }
        }

        // 期望 import 关键字
        if (!match_keyword("import")) {
            error(ErrorCategory::Syn, "SYN024", current(), "期望 'import' 关键字");
            sync_to_statement();
            return stmt;
        }

        // 解析导入的名称
        if (match_op("*")) {
            // from mod import *

            stmt->is_wildcard = true;
        } else if (check(TokenKind::Identifier)) {
            while (true) {
                std::string name = advance().lexeme;
                // 检查是否有 as alias
                if (match_keyword("as")) {
                    if (!check(TokenKind::Identifier)) {
                        error(ErrorCategory::Syn, "SYN025", current(), "as 后期望别名");
                    } else {
                        std::string alias = advance().lexeme;
                        stmt->imported_names.push_back(name + " as " + alias);
                    }
                } else {
                    stmt->imported_names.push_back(name);
                }
                if (!match_delim(",")) break;
                if (!check(TokenKind::Identifier)) {
                    error(ErrorCategory::Syn, "SYN026", current(), "期望导入的名称");
                    break;
                }
            }
        } else {
            error(ErrorCategory::Syn, "SYN026", current(), "import 后期望导入的名称或 '*'");
        }

        if (!match_delim(";")) {
            error(ErrorCategory::Syn, "SYN001", current(), "期望 ';'", "在语句末尾添加 ';'");
        }
        return stmt;
    } else {
        // import mod 或 import mod as alias 语法
        advance(); // import
        auto stmt = std::make_unique<ImportStmt>();
        stmt->loc = range_from(l, c);

        // 解析模块路径
        if (!check(TokenKind::Identifier)) {
            error(ErrorCategory::Syn, "SYN023", current(), "import 后期望模块名");
            sync_to_statement();
            return stmt;
        }
        while (true) {
            stmt->module_path.push_back(advance().lexeme);
            if (!match_delim(".")) break;
            if (!check(TokenKind::Identifier)) {
                error(ErrorCategory::Syn, "SYN023", current(), "期望子模块名");
                break;
            }
        }

        // 检查 alias
        if (match_keyword("as")) {
            if (!check(TokenKind::Identifier)) {
                error(ErrorCategory::Syn, "SYN025", current(), "as 后期望别名");
            } else {
                stmt->alias = advance().lexeme;
            }
        }

        if (!match_delim(";")) {
            error(ErrorCategory::Syn, "SYN001", current(), "期望 ';'", "在语句末尾添加 ';'");
        }
        return stmt;
    }
}

} // namespace next11