// 语义分析器实现
#include "semantic_analyzer.hpp"
#include "common/registry.hpp"
#include <functional>

namespace next11 {

void SemanticAnalyzer::error(ErrorCategory cat, std::string code, const SourceRange& loc,
                             std::string desc, std::optional<std::string> fix) {
    _errors.emplace_back(cat, std::move(code), loc.file, loc.line, loc.col,
                         std::move(desc), std::move(fix));
}

SemantResult SemanticAnalyzer::analyze(std::unique_ptr<Program> ast, std::string_view filename) {
    _symbols = std::make_shared<SymbolTable>();
    _file = std::string(filename);
    _errors.clear();

    if (ast) {
        register_decls(*ast);
        for (auto& d : ast->decls) {
            if (d) check_decl(*d);
        }
    }

    SemantResult result;
    result.annotated_ast = std::move(ast);
    result.symbols = _symbols;
    result.errors = std::move(_errors);
    return result;
}

void SemanticAnalyzer::register_decls(Program& prog) {
    for (auto& d : prog.decls) {
        if (!d) continue;
        SymbolEntry entry;
        entry.decl_loc = {d->loc.file, d->loc.line, d->loc.col};

        if (auto* fn = dynamic_cast<FnDef*>(d.get())) {
            entry.name = fn->name;
            entry.category = SymbolCategory::Function;
            entry.type_params = fn->type_params;
            entry.params = fn->params;
            entry.return_type = fn->return_type;
            entry.type = Type::make_unknown();
            if (!_symbols->declare(fn->name, entry)) {

                error(ErrorCategory::Semant, "SEM003", fn->loc,
                      std::string("重复声明 '") + fn->name + "'");
            }
        } else if (auto* sd = dynamic_cast<StructDef*>(d.get())) {
            entry.name = sd->name;
            entry.category = SymbolCategory::Struct;
            entry.type_params = sd->type_params;
            entry.struct_fields = sd->fields;
            entry.type = Type::make_unknown();
            if (!_symbols->declare(sd->name, entry)) {
                error(ErrorCategory::Semant, "SEM003", sd->loc,
                      std::string("重复声明 '") + sd->name + "'");
            }
        } else if (auto* ed = dynamic_cast<EnumDef*>(d.get())) {
            entry.name = ed->name;
            entry.category = SymbolCategory::Enum;
            entry.enum_variants = ed->variants;
            entry.type = Type::make_unknown();
            if (!_symbols->declare(ed->name, entry)) {
                error(ErrorCategory::Semant, "SEM003", ed->loc,
                      std::string("重复声明 '") + ed->name + "'");
            }
            for (auto& v : ed->variants) {
                _enum_variants[v] = ed->name;
            }
        } else if (auto* ta = dynamic_cast<TypeAlias*>(d.get())) {
            entry.name = ta->name;
            entry.category = SymbolCategory::TypeAlias;
            entry.type = type_from_ref(ta->aliased);
            if (!_symbols->declare(ta->name, entry)) {
                error(ErrorCategory::Semant, "SEM003", ta->loc,
                      std::string("重复声明 '") + ta->name + "'");
            }
        } else if (auto* imp = dynamic_cast<ImportStmt*>(d.get())) {
            // 处理 import 语句：注册模块名或导入的别名到符号表
            if (imp->is_from_import) {
                // from mod import ... 语法
                if (imp->is_wildcard) {
                    // from mod import * - 通配符导入在解释器处理
                } else {
                    // 选择性导入：注册导入的名称/别名
                    for (const auto& name_spec : imp->imported_names) {
                        size_t as_pos = name_spec.find(" as ");
                        std::string alias;
                        if (as_pos != std::string::npos) {
                            alias = name_spec.substr(as_pos + 4);
                        } else {
                            alias = name_spec;
                        }
                        if (!alias.empty()) {
                            entry.name = alias;
                            entry.category = SymbolCategory::Variable;
                            if (!_symbols->declare(alias, entry)) {
                                error(ErrorCategory::Semant, "SEM003", imp->loc,
                                      std::string("重复声明 '") + alias + "'");
                            }
                        }
                    }
                }
            } else {
                // import mod 或 import mod as alias 语法
                std::string module_name;
                if (!imp->alias.empty()) {
                    module_name = imp->alias;
                } else {
                    module_name = imp->module_path.empty() ? "" : imp->module_path.back();
                }
                if (!module_name.empty()) {
                    entry.name = module_name;
                    entry.category = SymbolCategory::Module;
                    if (!_symbols->declare(module_name, entry)) {
                        error(ErrorCategory::Semant, "SEM003", imp->loc,
                              std::string("重复声明 '") + module_name + "'");
                    }
                }
            }
        }
    }
}

void SemanticAnalyzer::check_decl(Decl& d) {
    if (auto* fn = dynamic_cast<FnDef*>(&d)) { check_fn(*fn); return; }
    if (auto* let = dynamic_cast<LetDecl*>(&d)) { check_let(*let); return; }
    if (auto* imp = dynamic_cast<ImportStmt*>(&d)) { check_import(*imp); return; }
    if (auto* sd = dynamic_cast<StructDef*>(&d)) {
        for (auto& f : sd->fields) {
            auto t = resolve_type_ref(f.type);
            if (t->kind == TypeKind::Unknown) {
                error(ErrorCategory::Semant, "SEM001", sd->loc,
                      std::string("结构体字段 '") + f.name + "' 类型未定义");
            }
        }
        return;
    }
}

std::shared_ptr<Type> SemanticAnalyzer::resolve_type_ref(const TypeRef& ref) {
    if (!ref.valid) return Type::make_unknown();
    if (ref.name == "any") return Type::make_unknown();
    if (ref.is_tuple) {
        auto t = std::make_shared<Type>();
        t->kind = TypeKind::Tuple;
        for (auto& e : ref.tuple_elems) t->tuple_elems.push_back(resolve_type_ref(e));
        return t;
    }
    if (ref.is_array) {
        return Type::make_array(ref.elem ? resolve_type_ref(*ref.elem) : Type::make_unknown());
    }
    // 查询 TypeRegistry 解析基础类型
    auto* ti = TypeRegistry::instance().lookup(ref.name);
    if (ti && ti->type) {
        return ti->type;
    }
    // 查找类型别名/struct/enum
    auto entry = _symbols->lookup(ref.name);
    if (entry) {
        if (entry->category == SymbolCategory::TypeAlias && entry->type) {
            return entry->type;
        }
        if (entry->category == SymbolCategory::Struct) {
            auto t = std::make_shared<StructType>(ref.name);
            for (auto& f : entry->struct_fields) {
                t->fields.emplace_back(f.name, resolve_type_ref(f.type));
            }
            return t;
        }
        if (entry->category == SymbolCategory::Enum) {
            auto t = std::make_shared<Type>();
            t->kind = TypeKind::Enum;
            t->enum_name = ref.name;
            t->enum_variants = entry->enum_variants;
            return t;
        }
        if (entry->category == SymbolCategory::TypeParam) {
            return Type::make_type_param(ref.name);
        }
    }
    return std::make_shared<StructType>(ref.name);
}

bool SemanticAnalyzer::check_compatible(const std::shared_ptr<Type>& expected,
                                        const std::shared_ptr<Type>& actual) {
    if (is_compatible(expected, actual)) return true;
    // 查询 ConversionRegistry 是否有隐式转换
    if (expected && actual) {
        std::string exp_name = expected->to_string();
        std::string act_name = actual->to_string();
        if (ConversionRegistry::instance().can_convert(act_name, exp_name)) {
            auto* ci = ConversionRegistry::instance().lookup(act_name, exp_name);
            if (ci && ci->is_implicit) return true;
        }
    }
    return false;
}

std::shared_ptr<Type> SemanticAnalyzer::instantiate_type(
    const std::shared_ptr<Type>& t,
    const std::map<std::string, std::shared_ptr<Type>>& bindings) {
    if (!t) return t;
    if (t->kind == TypeKind::TypeParam) {
        auto it = bindings.find(t->struct_name);
        if (it != bindings.end()) return it->second;
        return t;
    }
    auto result = t->clone();
    if (t->get_elem()) result->set_elem(instantiate_type(t->get_elem(), bindings));
    result->tuple_elems.clear();
    for (auto& e : t->tuple_elems) if (e) result->tuple_elems.push_back(instantiate_type(e, bindings));
    if (t->kind == TypeKind::Struct) {
        auto& src_fields = t->get_fields();
        auto& dst_fields = result->get_fields_mut();
        dst_fields.clear();
        for (auto& f : src_fields) if (f.second) dst_fields.emplace_back(f.first, instantiate_type(f.second, bindings));
    }
    result->func_params.clear();
    for (auto& p : t->func_params) if (p) result->func_params.push_back(instantiate_type(p, bindings));
    if (t->func_return) result->func_return = instantiate_type(t->func_return, bindings);
    return result;
}

void SemanticAnalyzer::check_fn(FnDef& d) {
    _symbols->enter_scope();
    auto saved_ret = _current_return_type;
    auto saved_tp = _current_type_params;
    _current_return_type = resolve_type_ref(d.return_type);
    _current_type_params = d.type_params;
    for (auto& p : d.params) {
        SymbolEntry pe;
        pe.name = p.name;
        pe.category = SymbolCategory::Variable;
        pe.type = resolve_type_ref(p.type);
        pe.decl_loc = {d.loc.file, d.loc.line, d.loc.col};
        _symbols->declare(p.name, pe);
    }
    for (auto& tp : d.type_params) {
        SymbolEntry te;
        te.name = tp;
        te.category = SymbolCategory::TypeParam;
        te.type = Type::make_type_param(tp);
        _symbols->declare(tp, te);
    }
    if (d.body) check_expr(*d.body);
    _current_return_type = saved_ret;
    _current_type_params = saved_tp;
    _symbols->exit_scope();
}

void SemanticAnalyzer::check_let(LetDecl& d) {
    if (d.is_destructure) {
        std::shared_ptr<Type> src_type;
        if (d.init.has_value() && d.init.value()) {
            check_expr(*d.init.value());
            src_type = infer_expr(*d.init.value());
        }
        // 静态类型校验
        if (src_type && src_type->kind != TypeKind::Unknown) {
            if (d.destr_pattern.is_array) {
                if (src_type->kind != TypeKind::Array) {
                    error(ErrorCategory::Destructure, "DST003", d.loc,
                          std::string("解构类型不兼容：期望数组 实际 ") + src_type->to_string());
                }
            } else if (d.destr_pattern.is_struct) {
                if (src_type->kind != TypeKind::Struct) {
                    error(ErrorCategory::Destructure, "DST003", d.loc,
                          std::string("解构类型不兼容：期望结构体 实际 ") + src_type->to_string());
                } else {
                    for (auto& f : d.destr_pattern.fields) {
                        bool found = false;
                        if (src_type->kind == TypeKind::Struct) {
                            for (auto& sf : src_type->get_fields()) {
                                if (sf.first == f.first) { found = true; break; }
                            }
                        }
                        if (!found) {
                            error(ErrorCategory::Destructure, "DST002", d.loc,
                                  std::string("结构体字段 '") + f.first + "' 不存在");
                        }
                    }
                }
            } else if (d.destr_pattern.is_tuple) {
                if (src_type->kind != TypeKind::Tuple) {
                    error(ErrorCategory::Destructure, "DST003", d.loc,
                          std::string("解构类型不兼容：期望元组 实际 ") + src_type->to_string());
                }
            }
        }
        // 声明解构模式中的变量
        std::function<void(const DestructurePattern&)> declare_pat = [&](const DestructurePattern& pat) {
            if (pat.is_array) {
                for (auto& sub : pat.elements) {
                    if (sub.is_leaf) {
                        SymbolEntry e; e.name = sub.var_name;
                        e.category = SymbolCategory::Variable;
                        e.type = Type::make_unknown();
                        e.decl_loc = {d.loc.file, d.loc.line, d.loc.col};
                        _symbols->declare(sub.var_name, e);
                    } else declare_pat(sub);
                }
            } else if (pat.is_struct) {
                for (auto& f : pat.fields) {
                    if (f.second.is_leaf) {
                        SymbolEntry e; e.name = f.second.var_name;
                        e.category = SymbolCategory::Variable;
                        e.type = Type::make_unknown();
                        e.decl_loc = {d.loc.file, d.loc.line, d.loc.col};
                        _symbols->declare(f.second.var_name, e);
                    } else declare_pat(f.second);
                }
            } else if (pat.is_tuple) {
                for (auto& sub : pat.elements) {
                    if (sub.is_leaf) {
                        SymbolEntry e; e.name = sub.var_name;
                        e.category = SymbolCategory::Variable;
                        e.type = Type::make_unknown();
                        e.decl_loc = {d.loc.file, d.loc.line, d.loc.col};
                        _symbols->declare(sub.var_name, e);
                    } else declare_pat(sub);
                }
            }
        };
        declare_pat(d.destr_pattern);
        return;
    }
    SymbolEntry entry;
    entry.name = d.name;
    entry.category = SymbolCategory::Variable;
    entry.type = resolve_type_ref(d.declared_type);
    entry.decl_loc = {d.loc.file, d.loc.line, d.loc.col};
    if (!_symbols->declare(d.name, entry)) {

        error(ErrorCategory::Semant, "SEM003", d.loc,
              std::string("重复声明 '") + d.name + "'",
              "重命名该标识符或删除重复声明");
    }
    if (d.init.has_value() && d.init.value()) {
        check_expr(*d.init.value());
        auto init_type = infer_expr(*d.init.value());
        if (init_type && entry.type && entry.type->kind != TypeKind::Unknown) {
            if (!check_compatible(entry.type, init_type)) {
                error(ErrorCategory::Semant, "SEM001", d.loc,
                      std::string("类型不匹配：期望 ") + entry.type->to_string() +
                      " 实际 " + init_type->to_string(),
                      "检查变量声明类型或表达式类型是否一致");
            }
        }
    }
}

void SemanticAnalyzer::check_stmt(Stmt& s) {
    if (auto* let = dynamic_cast<LetDecl*>(&s)) { check_let(*let); return; }
    if (auto* es = dynamic_cast<ExprStmt*>(&s)) { if (es->expr) check_expr(*es->expr); return; }
    if (auto* ifs = dynamic_cast<IfStmt*>(&s)) { check_if(*ifs); return; }
    if (auto* ws = dynamic_cast<WhileStmt*>(&s)) { check_while(*ws); return; }
    if (auto* fs = dynamic_cast<ForStmt*>(&s)) { check_for(*fs); return; }
    if (auto* rs = dynamic_cast<ReturnStmt*>(&s)) { check_return(*rs); return; }
    if (auto* bs = dynamic_cast<BreakStmt*>(&s)) { check_break(*bs); return; }
    if (auto* cs = dynamic_cast<ContinueStmt*>(&s)) { check_continue(*cs); return; }
    if (auto* block = dynamic_cast<Block*>(&s)) { check_block(*block); return; }
    if (auto* ws = dynamic_cast<WithStmt*>(&s)) {
        if (ws->context_expr) check_expr(*ws->context_expr);
        _symbols->enter_scope();
        if (!ws->as_name.empty()) {
            SymbolEntry se; se.name = ws->as_name; se.type = Type::make_unknown();
            _symbols->declare(ws->as_name, se);
        }
        if (ws->with_block) check_expr(*ws->with_block);
        _symbols->exit_scope();
        return;
    }
    if (auto* ts = dynamic_cast<TryStmt*>(&s)) {
        if (ts->try_block) check_expr(*ts->try_block);
        _symbols->enter_scope();
        if (!ts->catch_var.empty()) {
            SymbolEntry se; se.name = ts->catch_var; se.type = Type::make_unknown();
            _symbols->declare(ts->catch_var, se);
        }
        if (ts->catch_block) check_expr(*ts->catch_block);
        _symbols->exit_scope();
        if (ts->finally_block.has_value() && ts->finally_block.value()) check_expr(*ts->finally_block.value());
        return;
    }
    if (auto* ts = dynamic_cast<ThrowStmt*>(&s)) { if (ts->expr) check_expr(*ts->expr); return; }
    if (auto* ds = dynamic_cast<DelStmt*>(&s)) { if (ds->target) check_expr(*ds->target); return; }
}

void SemanticAnalyzer::check_block(Block& b) {
    _symbols->enter_scope();
    for (auto& s : b.stmts) {
        if (s) check_stmt(*s);
    }
    _symbols->exit_scope();
}

void SemanticAnalyzer::check_if(IfStmt& s) {
    if (s.cond) check_expr(*s.cond);
    if (s.then_block) check_expr(*s.then_block);
    if (s.else_block.has_value() && s.else_block.value()) {
        check_expr(*s.else_block.value());
    }
}

void SemanticAnalyzer::check_while(WhileStmt& s) {
    if (s.cond) check_expr(*s.cond);
    _loop_depth++;
    if (s.body) check_expr(*s.body);
    _loop_depth--;
}

void SemanticAnalyzer::check_for(ForStmt& s) {
    if (s.iterable) check_expr(*s.iterable);
    _symbols->enter_scope();
    SymbolEntry entry;
    entry.name = s.var_name;
    entry.category = SymbolCategory::Variable;
    entry.type = Type::make_unknown();
    _symbols->declare(s.var_name, entry);
    _loop_depth++;
    if (s.body) check_expr(*s.body);
    _loop_depth--;
    _symbols->exit_scope();
}

void SemanticAnalyzer::check_return(ReturnStmt& s) {
    if (s.value.has_value() && s.value.value()) {
        check_expr(*s.value.value());
        auto rt = infer_expr(*s.value.value());
        if (rt && _current_return_type && _current_return_type->kind != TypeKind::Unknown) {
            if (!check_compatible(_current_return_type, rt)) {
                error(ErrorCategory::Semant, "SEM001", s.loc,
                      std::string("返回类型不匹配：期望 ") + _current_return_type->to_string() +
                      " 实际 " + rt->to_string());
            }
        }
    }
}

void SemanticAnalyzer::check_break(BreakStmt& s) {
    if (_loop_depth <= 0) {
        error(ErrorCategory::Semant, "SEM010", s.loc,
              "break 语句不在循环体内",
              "将 break 移到 while/for 循环体内");
    }
}

void SemanticAnalyzer::check_continue(ContinueStmt& s) {
    if (_loop_depth <= 0) {
        error(ErrorCategory::Semant, "SEM011", s.loc,
              "continue 语句不在循环体内",
              "将 continue 移到 while/for 循环体内");
    }
}

void SemanticAnalyzer::check_expr(Expr& e) {
    switch (e.node_kind) {
        case NodeKind::Block: check_block(static_cast<Block&>(e)); return;

        case NodeKind::MatchExpr: check_match(static_cast<MatchExpr&>(e)); return;
        default: infer_expr(e); return;
    }
}

void SemanticAnalyzer::check_import(ImportStmt& imp) {
    // import 语句的语义检查
    // 模块路径的合法性检查在解释器中进行
    // 这里主要处理选择性导入的名称检查
    if (imp.is_from_import && !imp.is_wildcard) {
        for (const auto& name_spec : imp.imported_names) {
            size_t as_pos = name_spec.find(" as ");
            std::string alias;
            if (as_pos != std::string::npos) {
                alias = name_spec.substr(as_pos + 4);
            } else {
                alias = name_spec;
            }
            // 检查别名是否重复声明（在同一作用域内）
            // 注意：模块成员的合法性检查在解释器加载模块时进行
        }
    }
}

void SemanticAnalyzer::check_match(MatchExpr& e) {
    if (e.scrutinee) check_expr(*e.scrutinee);
    auto t = e.scrutinee ? infer_expr(*e.scrutinee) : Type::make_unknown();
    for (auto& arm : e.arms) {
        _symbols->enter_scope();
        if (arm.pattern && arm.pattern->node_kind == NodeKind::IdentExpr) {
            auto& pat = static_cast<IdentExpr&>(*arm.pattern);
            if (pat.name != "_") {
                SymbolEntry entry;
                entry.name = pat.name;
                entry.category = SymbolCategory::Variable;
                entry.type = t;
                _symbols->declare(pat.name, entry);
            }
        }
        if (arm.guard) check_expr(*arm.guard);
        if (arm.result) check_expr(*arm.result);
        _symbols->exit_scope();
    }
    // 穷尽性检查：枚举类型
    if (t && t->kind == TypeKind::Enum) {
        auto entry = _symbols->lookup(t->enum_name);
        if (entry && entry->category == SymbolCategory::Enum) {
            std::vector<std::string> covered;
            bool has_wildcard = false;
            for (auto& arm : e.arms) {
                if (arm.is_wildcard) { has_wildcard = true; continue; }
                if (arm.pattern && arm.pattern->node_kind == NodeKind::IdentExpr) {
                    covered.push_back(static_cast<IdentExpr*>(arm.pattern.get())->name);
                }
            }
            if (!has_wildcard) {
                std::string missing;
                for (auto& v : entry->enum_variants) {
                    bool found = false;
                    for (auto& c : covered) if (c == v) { found = true; break; }
                    if (!found) {
                        if (!missing.empty()) missing += ", ";
                        missing += v;
                    }
                }
                if (!missing.empty()) {
                    error(ErrorCategory::Match, "MAT002", e.loc,
                          std::string("match 分支未穷尽，缺少：") + missing);
                }
            }
        }
    }
}

std::shared_ptr<Type> SemanticAnalyzer::infer_expr(Expr& e) {
    switch (e.node_kind) {
        case NodeKind::LiteralExpr: {
            auto& lit = static_cast<LiteralExpr&>(e);
            switch (lit.lit_kind) {
                case LiteralExpr::Int: e.type = Type::make_int(); return e.type;
                case LiteralExpr::Float: e.type = Type::make_float(); return e.type;
                case LiteralExpr::String: e.type = Type::make_string(); return e.type;
                case LiteralExpr::Bool: e.type = Type::make_bool(); return e.type;
                case LiteralExpr::Complex: e.type = Type::make_unknown(); return e.type;
            }
            return Type::make_unknown();
        }
        case NodeKind::IdentExpr: {
            auto& id = static_cast<IdentExpr&>(e);
            auto entry = _symbols->lookup(id.name);
            if (!entry) {
                auto ev = _enum_variants.find(id.name);
                if (ev != _enum_variants.end()) {
                    auto t = std::make_shared<Type>();
                    t->kind = TypeKind::Enum;
                    t->enum_name = ev->second;
                    e.type = t;
                    return e.type;
                }
                // 检查是否为泛型类型参数作为值使用
                for (auto& tp : _current_type_params) {
                    if (tp == id.name) {
                        error(ErrorCategory::Semant, "SEM002", e.loc,
                              std::string("类型参数 '") + id.name + "' 不能作为值使用",
                              "类型参数仅可用于类型注解位置");
                        e.type = Type::make_unknown();
                        return e.type;
                    }
                }
                // 检查是否为已退出作用域的变量（SEM004 作用域越界访问）
                auto exited = _symbols->lookup_exited(id.name);
                if (exited) {
                    error(ErrorCategory::Semant, "SEM004", e.loc,
                          std::string("作用域越界访问 '") + id.name + "'",
                          "将声明移到使用点之前的作用域中");
                } else {
                    error(ErrorCategory::Semant, "SEM002", e.loc,
                          std::string("未声明标识符 '") + id.name + "'",
                          "声明该变量或检查拼写是否正确");
                }
                e.type = Type::make_unknown();
                return e.type;
            }
            e.type = entry->type;
            return e.type;
        }
        case NodeKind::BinaryExpr: {
            auto& bin = static_cast<BinaryExpr&>(e);
            if (bin.lhs) infer_expr(*bin.lhs);
            if (bin.rhs) infer_expr(*bin.rhs);
            if (bin.op == "==" || bin.op == "!=" || bin.op == "<" || bin.op == "<=" ||
                bin.op == ">" || bin.op == ">=" || bin.op == "&&" || bin.op == "||" ||
                bin.op == "in") {
                e.type = Type::make_bool();
            } else {
                e.type = bin.lhs ? bin.lhs->type : Type::make_unknown();
            }
            return e.type;
        }
        case NodeKind::UnaryExpr: {
            auto& un = static_cast<UnaryExpr&>(e);
            if (un.operand) infer_expr(*un.operand);
            if (un.op == "!") e.type = Type::make_bool();
            else e.type = un.operand ? un.operand->type : Type::make_unknown();
            return e.type;
        }
        case NodeKind::CallExpr: {
            auto& call = static_cast<CallExpr&>(e);
            for (auto& a : call.args) if (a) infer_expr(*a);
            if (call.callee && call.callee->node_kind == NodeKind::IdentExpr) {
                auto name = static_cast<IdentExpr*>(call.callee.get())->name;
                auto entry = _symbols->lookup(name);
                if (entry && entry->category == SymbolCategory::Function) {
                    if (call.args.size() != entry->params.size()) {
                        error(ErrorCategory::Semant, "SEM001", e.loc,
                              std::string("函数 '") + name + "' 参数数量不匹配：期望 " +
                              std::to_string(entry->params.size()) + " 实际 " +
                              std::to_string(call.args.size()));
                    }
                    // 泛型类型推导
                    std::map<std::string, std::shared_ptr<Type>> bindings;
                    if (!entry->type_params.empty()) {
                        if (!call.type_args.empty()) {
                            for (size_t i = 0; i < entry->type_params.size() && i < call.type_args.size(); ++i) {
                                bindings[entry->type_params[i]] = resolve_type_ref(call.type_args[i]);
                            }
                        } else {
                            for (size_t i = 0; i < call.args.size() && i < entry->params.size(); ++i) {
                                auto& ref = entry->params[i].type;
                                auto at = call.args[i] ? call.args[i]->type : nullptr;
                                if (!ref.is_array && at && ref.valid) {
                                    for (auto& tp : entry->type_params) {
                                        if (tp == ref.name) {
                                            if (bindings.count(tp) && !check_compatible(bindings[tp], at)) {
                                                error(ErrorCategory::Generic, "GEN001", e.loc,
                                                      std::string("无法推导类型参数 '") + tp + "'");
                                            }
                                            bindings[tp] = at;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    for (size_t i = 0; i < call.args.size() && i < entry->params.size(); ++i) {
                        auto& ref = entry->params[i].type;
                        std::shared_ptr<Type> param_type;
                        bool is_tp = false;
                        for (auto& tp : entry->type_params) {
                            if (tp == ref.name && !ref.is_array) { is_tp = true; break; }
                        }
                        if (is_tp) {
                            auto it = bindings.find(ref.name);
                            param_type = (it != bindings.end()) ? it->second : Type::make_type_param(ref.name);
                        } else {
                            param_type = resolve_type_ref(ref);
                            if (!bindings.empty()) param_type = instantiate_type(param_type, bindings);
                        }
                        auto arg_type = call.args[i] ? call.args[i]->type : nullptr;
                        if (param_type && arg_type &&
                            param_type->kind != TypeKind::Unknown &&
                            arg_type->kind != TypeKind::Unknown &&
                            param_type->kind != TypeKind::TypeParam) {
                            if (!check_compatible(param_type, arg_type)) {
                                error(ErrorCategory::Semant, "SEM001", e.loc,
                                      std::string("函数 '") + name + "' 参数 " +
                                      std::to_string(i+1) + " 类型不匹配：期望 " +
                                      param_type->to_string() + " 实际 " +
                                      arg_type->to_string());
                            }
                        }
                    }
                    // 显式类型参数数量校验
                    if (!entry->type_params.empty() && !call.type_args.empty()) {
                        if (call.type_args.size() != entry->type_params.size()) {
                            error(ErrorCategory::Generic, "GEN002", e.loc,
                                  std::string("类型参数数量不匹配：期望 ") +
                                  std::to_string(entry->type_params.size()) + " 实际 " +
                                  std::to_string(call.type_args.size()));
                        }
                    }
                    auto& ret_ref = entry->return_type;
                    std::shared_ptr<Type> ret_type;
                    bool ret_is_tp = false;
                    for (auto& tp : entry->type_params) {
                        if (tp == ret_ref.name && !ret_ref.is_array) { ret_is_tp = true; break; }
                    }
                    if (ret_is_tp) {
                        auto it = bindings.find(ret_ref.name);
                        ret_type = (it != bindings.end()) ? it->second : Type::make_type_param(ret_ref.name);
                    } else {
                        ret_type = resolve_type_ref(ret_ref);
                        if (!bindings.empty()) ret_type = instantiate_type(ret_type, bindings);
                    }
                    e.type = ret_type;
                    return e.type;
                }
            }
            e.type = Type::make_unknown();
            return e.type;
        }
        case NodeKind::PipeExpr: {
            auto& pe = static_cast<PipeExpr&>(e);
            if (pe.lhs) infer_expr(*pe.lhs);
            if (pe.call && pe.call->callee &&
                pe.call->callee->node_kind == NodeKind::IdentExpr) {
                auto name = static_cast<IdentExpr*>(pe.call->callee.get())->name;
                auto entry = _symbols->lookup(name);
                if (entry && entry->category == SymbolCategory::Function) {
                    size_t expected = entry->params.size();
                    size_t actual = 1 + pe.call->args.size();
                    if (actual != expected) {
                        error(ErrorCategory::Semant, "SEM001", e.loc,
                              std::string("函数 '") + name + "' 参数数量不匹配：期望 " +
                              std::to_string(expected) + " 实际 " +
                              std::to_string(actual));
                    }
                    if (expected > 0 && pe.lhs && pe.lhs->type) {
                        auto param_type = resolve_type_ref(entry->params[0].type);
                        if (!check_compatible(param_type, pe.lhs->type)) {
                            error(ErrorCategory::Pipe, "PIP002", e.loc,
                                  std::string("管道类型不匹配：左侧 ") +
                                  (pe.lhs->type ? pe.lhs->type->to_string() : "?") +
                                  " 函数首参 " + param_type->to_string());
                        }
                    }
                    e.type = resolve_type_ref(entry->return_type);
                    return e.type;
                }
            }
            if (pe.call) infer_expr(*pe.call);
            e.type = pe.call ? pe.call->type : Type::make_unknown();
            return e.type;
        }
        case NodeKind::AssignExpr: {
            auto& ae = static_cast<AssignExpr&>(e);
            if (ae.target) infer_expr(*ae.target);
            if (ae.value) infer_expr(*ae.value);
            if (ae.target && ae.value && ae.target->type && ae.value->type) {
                if (!check_compatible(ae.target->type, ae.value->type)) {
                    error(ErrorCategory::Semant, "SEM001", e.loc,
                          std::string("类型不匹配：期望 ") + ae.target->type->to_string() +
                          " 实际 " + ae.value->type->to_string());
                }
            }
            e.type = ae.target ? ae.target->type : Type::make_unknown();
            return e.type;
        }
        case NodeKind::ArrayExpr: {
            auto& arr = static_cast<ArrayExpr&>(e);
            for (auto& el : arr.elements) if (el) infer_expr(*el);
            auto elem_type = (!arr.elements.empty() && arr.elements[0])
                ? arr.elements[0]->type : Type::make_unknown();
            e.type = Type::make_array(elem_type);
            return e.type;
        }
        case NodeKind::TupleExpr: {
            auto& tup = static_cast<TupleExpr&>(e);
            auto t = std::make_shared<Type>();
            t->kind = TypeKind::Tuple;
            for (auto& el : tup.elements) {
                if (el) t->tuple_elems.push_back(infer_expr(*el));
                else t->tuple_elems.push_back(Type::make_unknown());
            }
            e.type = t;
            return e.type;
        }
        case NodeKind::IndexExpr: {
            auto& ie = static_cast<IndexExpr&>(e);
            if (ie.array) infer_expr(*ie.array);
            if (ie.index) infer_expr(*ie.index);
            if (ie.array && ie.array->type && ie.array->type->kind == TypeKind::Array) {
                e.type = ie.array->type->get_elem();
            } else {
                e.type = Type::make_unknown();
            }
            return e.type;
        }
        case NodeKind::MemberExpr: {
            auto& me = static_cast<MemberExpr&>(e);
            if (me.object) infer_expr(*me.object);
            if (me.object && me.object->type && me.object->type->kind == TypeKind::Struct) {
                for (auto& f : me.object->type->get_fields()) {
                    if (f.first == me.member) { e.type = f.second; return e.type; }
                }
            }
            e.type = Type::make_unknown();
            return e.type;
        }
        case NodeKind::StructLitExpr: {
            auto& sl = static_cast<StructLitExpr&>(e);
            for (auto& f : sl.fields) if (f.second) infer_expr(*f.second);
            auto entry = _symbols->lookup(sl.struct_name);
            if (entry && entry->category == SymbolCategory::Struct) {
                auto t = std::make_shared<StructType>(sl.struct_name);
                for (auto& f : entry->struct_fields) {
                    t->fields.emplace_back(f.name, resolve_type_ref(f.type));
                }
                e.type = t;
            } else {
                e.type = Type::make_unknown();
            }
            return e.type;
        }
        case NodeKind::GroupExpr: {
            auto& ge = static_cast<GroupExpr&>(e);
            if (ge.inner) e.type = infer_expr(*ge.inner);
            return e.type;
        }
        case NodeKind::MatchExpr: {
            check_match(static_cast<MatchExpr&>(e));
            return e.type;
        }
        default:
            e.type = Type::make_unknown();
            return e.type;
    }
}

} // namespace next11