// 解释执行器实现
#include "interpreter.hpp"
#include "c3_linearizer.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "semantic/semantic_analyzer.hpp"
#include <cmath>
#include <sstream>
#include <algorithm>
#include <fstream>
#include "stdlib/gui.hpp"
#include "event_loop.hpp"
#ifdef _WIN32
#include <windows.h>
#endif

namespace next11 {

void Interpreter::record_error(std::string code, const SourceRange& loc, std::string desc,
                                std::optional<std::string> fix, ErrorCategory cat) {
    _errors.emplace_back(cat, std::move(code), loc.file, loc.line, loc.col,
                         std::move(desc), std::move(fix));
}

void Interpreter::runtime_error(std::string code, const SourceRange& loc, std::string desc,
                                std::optional<std::string> fix, ErrorCategory cat) {

    auto exc_data = std::make_shared<ExceptionData>();
    exc_data->type_name = "Exception";
    exc_data->message = code + ": " + desc;
    throw Next11Exception(Value::make_exception(exc_data));
}

ExecResult Interpreter::execute(Program& ast, SymbolTable& symbols,
                                 std::ostream& out, std::ostream& err, std::istream& in,
                                 std::string_view filename) {

    _out = &out;
    _err = &err;
    _in = &in;
    _file = std::string(filename);
    _errors.clear();
    _recursion_depth = 0;
    _loop_count = 0;
    _memory_used = 0;
    _symbols = &symbols;
    _env = Environment{};
    _fn_map.clear();
    _exception_mro.clear();
    // 设置全局类表指针（供 isinstance/issubtype MRO 查找使用）
    g_class_table.table = &_class_objects;
    // 创建事件循环实例（供 async/await 协作式调度使用）
    EventLoop ev_loop;
    _event_loop = &ev_loop;
    // 设置 GUI 回调调度器
    GuiSystem::instance().set_interpreter(this);
    GuiSystem::instance().set_dispatcher([this](const std::string& fn) {
        try {
            call_function(fn, {}, SourceRange{0,0,0,0,_file});
        } catch (const std::exception& ex) {
            std::cerr << "[GUI回调错误] " << ex.what() << "\n";
        } catch (...) {
            std::cerr << "[GUI回调错误] 未知异常\n";
        }
    });
    _sys_path.clear();
    _sys_path.push_back(".");  // 当前目录
    // 添加源文件所在目录到搜索路径
    if (!filename.empty()) {
        std::string filename_str(filename);
        size_t pos = filename_str.find_last_of("/\\");
        if (pos != std::string::npos) {
            _sys_path.push_back(filename_str.substr(0, pos));
        }
    }
    _sys_modules.clear();
    _loading_modules.clear();
    _module_asts.clear();
    init_exception_classes();

    // 建立函数名到定义的映射
    for (auto& d : ast.decls) {
        if (auto* fn = dynamic_cast<FnDef*>(d.get())) {
            _fn_map[fn->name] = fn;
        }
    }

    // 执行顶层声明
    try {
        // 建立类定义表（OOP 4.2）——类属性初始化表达式求值可能抛 Next11Exception，必须在 try 块内
        _class_objects.clear();
        for (auto& d : ast.decls) {
            if (auto* cd = dynamic_cast<ClassDef*>(d.get())) {
                create_class(*cd);
            }
        }

        // 建立枚举变体到枚举名的映射
        for (auto& d : ast.decls) {
            if (auto* ed = dynamic_cast<EnumDef*>(d.get())) {
                for (auto& v : ed->variants) {
                    _enum_variants[v] = ed->name;
                }
            }
        }

        for (auto& d : ast.decls) {
            if (!d) continue;
            if (auto* let = dynamic_cast<LetDecl*>(d.get())) {
                if (let->is_destructure) {
                    if (let->init.has_value() && let->init.value()) {
                        Value src = eval(*let->init.value());
                        exec_destructure(let->destr_pattern, src, let->loc);
                    }
                } else {
                    Value v;
                    if (let->init.has_value() && let->init.value()) {
                        v = eval(*let->init.value());
                    } else {
                        v = fn_default_value(let->declared_type);
                    }
                    declare_var(let->name, v);
                }
            } else if (auto* imp = dynamic_cast<ImportStmt*>(d.get())) {
                exec_stmt(*imp);
            } else if (auto* fn = dynamic_cast<FnDef*>(d.get())) {
                exec_stmt(*fn);
            }
        }
    } catch (const Next11Exception& ne) {
        if (ne.exc_value.is_exception()) {
            auto& exc = ne.exc_value.as_exception();
            _errors.emplace_back(ErrorCategory::Run, "RUN", _file, 0, 0,
                exc.type_name + ": " + exc.message, std::nullopt);
        } else {
            _errors.emplace_back(ErrorCategory::Run, "RUN", _file, 0, 0,
                "未捕获异常", std::nullopt);
        }
    } catch (const std::exception& e) {
        _errors.emplace_back(ErrorCategory::Run, "RUN", _file, 0, 0,
            e.what(), std::nullopt);
    }

    // 查找并执行 main 函数（如果存在）
    auto main_entry = symbols.lookup("main");
    if (main_entry && main_entry->category == SymbolCategory::Function) {
        try {
            call_function("main", {}, SourceRange{0,0,0,0,_file});
        } catch (const Next11Exception& ne) {
            if (ne.exc_value.is_exception()) {
                auto& exc = ne.exc_value.as_exception();
                _errors.emplace_back(ErrorCategory::Run, "RUN", _file, 0, 0,
                    exc.type_name + ": " + exc.message, std::nullopt);
            } else {
                _errors.emplace_back(ErrorCategory::Run, "RUN", _file, 0, 0,
                    "未捕获异常", std::nullopt);
            }
        } catch (const std::runtime_error& e) {
            _errors.emplace_back(ErrorCategory::Run, "RUN", _file, 0, 0,
                e.what(), std::nullopt);
        } catch (const std::exception& e) {
            _errors.emplace_back(ErrorCategory::Run, "RUN", _file, 0, 0,
                e.what(), std::nullopt);
        }
    }

    _event_loop = nullptr;
    g_class_table.table = nullptr;
    ExecResult result;
    result.errors = std::move(_errors);
    result.exit_code = result.errors.empty() ? 0 : 4;
    return result;
}

Value Interpreter::fn_default_value(const TypeRef& tr) {
    if (!tr.valid) return Value::make_null();
    if (tr.is_array) return Value::make_array({});
    if (tr.name == "int") return Value::make_int(0);
    if (tr.name == "float") return Value::make_float(0.0);
    if (tr.name == "string") return Value::make_string("");
    if (tr.name == "bool") return Value::make_bool(false);
    return Value::make_null();
}

void Interpreter::declare_var(const std::string& name, Value val) {

    _env.declare(name, std::move(val));
}

Value* Interpreter::lookup_var(const std::string& name) {
    return _env.lookup(name);
}

void Interpreter::assign_var(const std::string& name, Value val) {
    _env.assign(name, std::move(val));
}

Value Interpreter::eval(Expr& e) {
    switch (e.node_kind) {
        case NodeKind::LiteralExpr: {
            auto& lit = static_cast<LiteralExpr&>(e);
            switch (lit.lit_kind) {
                case LiteralExpr::Int: return Value::make_int(lit.int_val);
                case LiteralExpr::Float: return Value::make_float(lit.float_val);
                case LiteralExpr::String: {
                    auto v = Value::make_string(lit.str_val);
                    track_memory(value_memory(v), e.loc);
                    return v;
                }
                case LiteralExpr::Bool: return Value::make_bool(lit.bool_val);
                case LiteralExpr::Complex: return Value::make_complex(lit.complex_real, lit.complex_imag);
            }
            return Value::make_null();
        }
        case NodeKind::IdentExpr: {
            auto& id = static_cast<IdentExpr&>(e);
            auto* v = lookup_var(id.name);
            if (v) return *v;
            // 检查是否为函数
            auto fn_it = _fn_map.find(id.name);
            if (fn_it != _fn_map.end() && fn_it->second) {
                return Value::make_fnref(FnRef(id.name));
            }
            // 检查是否为类名
            if (_class_objects.count(id.name) > 0) {
                return Value::make_class(std::make_shared<ClassObjectData>(_class_objects[id.name]));
            }
            // 检查是否为枚举变体
            auto ev = _enum_variants.find(id.name);
            if (ev != _enum_variants.end()) {
                return Value::make_string(id.name);
            }
            runtime_error("RUN005", e.loc, std::string("未声明标识符 '") + id.name + "'");
            return Value::make_null();
        }
        case NodeKind::BinaryExpr: return eval_binary(static_cast<BinaryExpr&>(e));
        case NodeKind::UnaryExpr: return eval_unary(static_cast<UnaryExpr&>(e));
        case NodeKind::CallExpr: return eval_call(static_cast<CallExpr&>(e));
        case NodeKind::MatchExpr: return eval_match(static_cast<MatchExpr&>(e));
        case NodeKind::PipeExpr: return eval_pipe(static_cast<PipeExpr&>(e));
        case NodeKind::AssignExpr: {
            auto& ae = static_cast<AssignExpr&>(e);
            Value v = eval(*ae.value);
            if (ae.target->node_kind == NodeKind::IdentExpr) {
                assign_var(static_cast<IdentExpr*>(ae.target.get())->name, v);
            } else if (ae.target->node_kind == NodeKind::MemberExpr) {
                // OOP 4.3：实例属性赋值 obj.attr = value
                auto& me = static_cast<MemberExpr&>(*ae.target);
                Value obj = eval(*me.object);
                if (obj.is_struct() && _class_objects.count(obj.as_struct().struct_name) > 0) {
                    auto& cls = _class_objects[obj.as_struct().struct_name];
                    // 4.4 访问控制：沿 MRO 查找字段定义处的访问级别
                    {
                        const auto& target_mro = cls.mro;
                        for (const auto& mro_name : target_mro) {
                            auto mit = _class_objects.find(mro_name);
                            if (mit == _class_objects.end()) continue;
                            auto fit = mit->second.field_access.find(me.member);
                            if (fit != mit->second.field_access.end()) {
                                check_access_control(me.member, fit->second,
                                                     is_inside_class_access(mro_name), e.loc);
                                break;
                            }
                        }
                    }
                    // 4.3 描述符 __set__ 协议：先查类 MRO 是否有该属性的描述符
                    Value cls_attr = lookup_class_attr(obj.as_struct().struct_name, me.member);
                    if (!cls_attr.is_null() && assign_descriptor(cls_attr, obj, v, e.loc)) {
                        return v;
                    }
                    // 4.4 __slots__ 限制：若类有 __slots__ 且属性不在 slots 中，抛 RUN018
                    if (!cls.slots.empty()) {
                        bool in_slots = false;
                        for (const auto& s : cls.slots) {
                            if (s == me.member) { in_slots = true; break; }
                        }
                        if (!in_slots) {
                            runtime_error("RUN018", e.loc,
                                std::string("属性 '") + me.member + "' 不在 __slots__ 中");
                            return v;
                        }
                    }
                    // 4.4 __setattr__ fallback
                    Value setattr_hook = lookup_class_attr(obj.as_struct().struct_name, "__setattr__");
                    if (!setattr_hook.is_null()) {
                        std::vector<Value> sa_args;
                        sa_args.push_back(obj);
                        sa_args.push_back(Value::make_string(me.member));
                        sa_args.push_back(v);
                        std::string fn_name = setattr_hook.is_fnref() ? setattr_hook.as_fnref().name : "__setattr__";
                        call_function(fn_name, std::move(sa_args), e.loc);
                        return v;
                    }
                    obj.as_struct().fields[me.member] = v;
                    // StructInstance 按值存储在 variant 中，需写回原变量
                    if (me.object->node_kind == NodeKind::IdentExpr) {
                        assign_var(static_cast<IdentExpr*>(me.object.get())->name, obj);
                    }
                }
                // OOP: 类属性赋值 ClassName.attr = value
                if (obj.is_class()) {
                    std::string cls_name = obj.as_class().name;
                    auto cit = _class_objects.find(cls_name);
                    if (cit != _class_objects.end()) {
                        cit->second.dict[me.member] = v;
                    }
                }
                // 普通结构体成员赋值（非 class 实例）：shared_ptr 共享 StructInstance，直接修改即可
                if (obj.is_struct() && _class_objects.count(obj.as_struct().struct_name) == 0) {
                    obj.as_struct().fields[me.member] = v;
                }
            } else if (ae.target->node_kind == NodeKind::IndexExpr) {
                // 字典/数组索引赋值: dict[key] = value, arr[idx] = value
                auto& ie = static_cast<IndexExpr&>(*ae.target);
                Value container = eval(*ie.array);
                Value key = eval(*ie.index);
                if (container.is_dict()) {
                    // 字典赋值：shared_ptr 共享 DictData，直接修改即可
                    auto& dict_data = container.as_dict();
                    bool found = false;
                    for (auto& kv : dict_data.entries) {
                        if (kv.first.equals(key)) {
                            kv.second = v;
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        dict_data.entries.emplace_back(key, v);
                    }
                } else if (container.is_array()) {
                    // 数组赋值：ArrayData 使用 shared_ptr，直接修改即生效
                    if (!key.is_int()) runtime_error("RUN150", e.loc, "数组索引必须是整数，得到: " + key.type_name());
                    int64_t i = key.as_int();
                    if (i < 0 || i >= static_cast<int64_t>(container.as_array().size())) {
                        runtime_error("RUN002", e.loc,
                            "数组越界赋值：下标 " + std::to_string(i) + " 超出长度 " + std::to_string(container.as_array().size()));
                    } else {
                        container.as_array()[static_cast<size_t>(i)] = v;
                    }
                } else {
                    runtime_error("RUN005", e.loc, "对非数组/字典进行索引赋值");
                }
            }
            return v;
        }
        case NodeKind::ArrayExpr: return eval_array(static_cast<ArrayExpr&>(e));
        case NodeKind::DictExpr: return eval_dict(static_cast<DictExpr&>(e));
        case NodeKind::SetExpr: return eval_set(static_cast<SetExpr&>(e));
        case NodeKind::TupleExpr: return eval_tuple(static_cast<TupleExpr&>(e));
        case NodeKind::IndexExpr: return eval_index(static_cast<IndexExpr&>(e));
        case NodeKind::MemberExpr: return eval_member(static_cast<MemberExpr&>(e));
        case NodeKind::StructLitExpr: {
            auto& sl = static_cast<StructLitExpr&>(e);
            StructInstance si;
            si.struct_name = sl.struct_name;
            for (auto& f : sl.fields) {
                if (f.second) si.fields[f.first] = eval(*f.second);
            }
            auto result = Value::make_struct(std::move(si));
            track_memory(value_memory(result), e.loc);
            return result;
        }
        case NodeKind::GroupExpr: {
            auto& ge = static_cast<GroupExpr&>(e);
            return ge.inner ? eval(*ge.inner) : Value::make_null();
        }
        case NodeKind::AwaitExpr: {
            auto& ae = static_cast<AwaitExpr&>(e);
            Value val = ae.await_expr ? eval(*ae.await_expr) : Value::make_null();
            if (val.is_coroutine()) {
                auto& coro = val.as_coroutine();
                while (true) {
                    if (_event_loop) {
                        static_cast<EventLoop*>(_event_loop)->poll();
                    }
                    try {
                        if (AsyncRuntime::instance().check_done(&coro)) break;
                    } catch (const std::runtime_error& re) {
                        auto exc_data = std::make_shared<ExceptionData>();
                        exc_data->type_name = "Exception";
                        exc_data->message = re.what();
                        throw Next11Exception(Value::make_exception(exc_data));
                    }
                    std::this_thread::yield();
                }
                if (coro.result) return *coro.result;
                return Value::make_null();
            }
            if (_event_loop) {
                static_cast<EventLoop*>(_event_loop)->poll();
            }
            return val;
        }
        case NodeKind::YieldExpr: {
            auto& ye = static_cast<YieldExpr&>(e);
            if (_gen_active) {
                _gen_yield_count++;
                if (_gen_yield_count > _gen_target_count) {
                    Value yv = Value::make_null();
                    if (ye.value.has_value() && ye.value.value()) {
                        yv = eval(*ye.value.value());
                    }
                    throw YieldSignal(std::move(yv));
                }
                return Value::make_null();
            }
            if (ye.value.has_value() && ye.value.value()) {
                return eval(*ye.value.value());
            }
            return Value::make_null();
        }
        case NodeKind::SuperExpr: return eval_super(static_cast<SuperExpr&>(e), Value::make_null());
        case NodeKind::Block: {
            auto& b = static_cast<Block&>(e);
            _env.push_scope();
            try {
                Value last = Value::make_null();
                for (auto& s : b.stmts) {
                    if (!s) continue;
                    if (s->node_kind == NodeKind::ExprStmt) {
                        last = eval(*static_cast<ExprStmt*>(s.get())->expr);
                    } else {
                        auto sig = exec_stmt(*s);
                        if (sig == ExecSignal::Return) break;
                    }
                }
                _env.pop_scope();
                return last;
            } catch (...) {
                _env.pop_scope();
                throw;
            }
        }
        default: throw std::runtime_error("RUN336: 未知表达式节点类型");
    }
}

Value Interpreter::eval_binary(BinaryExpr& e) {
    // && 和 || 短路求值，不通过注册表（需要延迟求值右侧）
    if (e.op == "&&") {
        Value l = eval(*e.lhs);
        if (!l.truthy()) return Value::make_bool(false);
        Value r = eval(*e.rhs);
        return Value::make_bool(r.truthy());
    }
    if (e.op == "||") {
        Value l = eval(*e.lhs);
        if (l.truthy()) return Value::make_bool(true);
        Value r = eval(*e.rhs);
        return Value::make_bool(r.truthy());
    }

    Value l = eval(*e.lhs);
    Value r = eval(*e.rhs);

    // 除零检查（注册表 eval_fn 无法报告 runtime_error）
    if (e.op == "/" || e.op == "%") {
        if ((r.is_int() || r.is_float()) && r.as_number() == 0.0) {
            runtime_error("RUN001", e.loc, "除零错误", "检查除数是否为零，添加零值检查");
            return Value::make_null();
        }
    }

    // 复数大小比较检查（复数没有全序关系）
    if (e.op == "<" || e.op == ">" || e.op == "<=" || e.op == ">=") {
        if (l.is_complex() || r.is_complex()) {
            runtime_error("RUN020", e.loc, "复数不能使用大小比较 (<, >, <=, >=)",
                "使用 abs() 比较模长，或使用 == / != 比较相等");
            return Value::make_null();
        }
    }

    // 查询 OperatorRegistry
    auto* op_info = OperatorRegistry::instance().lookup(e.op);
    if (op_info && op_info->eval_fn) {
        return op_info->eval_fn(l, r);
    }
    runtime_error("RUN032", e.loc, std::string("未知运算符 '") + e.op + "'");
    return Value::make_null();
}

Value Interpreter::eval_unary(UnaryExpr& e) {
    Value v = eval(*e.operand);
    if (e.op == "-") {
        if (v.is_int()) {
            if (v.as_int() == INT64_MIN) throw std::runtime_error("RUN355: 一元负号溢出: -INT64_MIN 不可表示");
            return Value::make_int(-v.as_int());
        }
        if (v.is_float()) return Value::make_float(-v.as_float());
        if (v.is_complex()) { const auto& c = v.as_complex(); return Value::make_complex(-c.real, -c.imag); }
        throw std::runtime_error("RUN131: 一元 - 运算符不支持的类型: " + v.type_name());
    }
    if (e.op == "!") return Value::make_bool(!v.truthy());
    runtime_error("RUN132", e.loc, std::string("未知一元运算符 '") + e.op + "'");
    return Value::make_null();
}

Value Interpreter::eval_array(ArrayExpr& e) {
    std::vector<Value> elems;
    for (auto& el : e.elements) {
        if (el) elems.push_back(eval(*el));
    }
    auto result = Value::make_array(std::move(elems));
    track_memory(value_memory(result), e.loc);
    return result;
}

Value Interpreter::eval_dict(DictExpr& e) {
    auto dict_data = std::make_shared<DictData>();
    for (size_t i = 0; i < e.pairs.size(); ++i) {
        if (e.pairs[i].first && e.pairs[i].second) {
            Value key = eval(*e.pairs[i].first);
            Value val = eval(*e.pairs[i].second);
            dict_data->entries.emplace_back(key, val);
        }
    }
    auto result = Value::make_dict(std::move(dict_data));
    track_memory(value_memory(result), e.loc);
    return result;
}

Value Interpreter::eval_set(SetExpr& e) {
    auto set_data = std::make_shared<SetData>();
    for (size_t i = 0; i < e.elements.size(); ++i) {
        if (e.elements[i]) {
            Value elem = eval(*e.elements[i]);
            // 检查重复（集合去重）
            bool found = false;
            for (const auto& existing : set_data->elements) {
                if (existing.equals(elem)) { found = true; break; }
            }
            if (!found) set_data->elements.push_back(std::move(elem));
        }
    }
    auto result = Value::make_set(std::move(set_data));
    track_memory(value_memory(result), e.loc);
    return result;
}

Value Interpreter::eval_tuple(TupleExpr& e) {
    std::vector<Value> elems;
    for (auto& el : e.elements) {
        if (el) elems.push_back(eval(*el));
    }
    auto result = Value::make_tuple(std::move(elems));
    track_memory(value_memory(result), e.loc);
    return result;
}

int64_t Interpreter::value_memory(const Value& v) const {
    int64_t sz = 0;
    std::vector<const Value*> stack;
    std::unordered_set<const void*> visited;
    stack.push_back(&v);
    visited.insert(&v);
    while (!stack.empty()) {
        const Value* cur = stack.back();
        stack.pop_back();
        if (cur->is_string()) sz += static_cast<int64_t>(cur->as_string().capacity());
        if (cur->is_array()) {
            for (auto& e : cur->as_array()) {
                if (visited.insert(&e).second) { stack.push_back(&e); sz += 48; }
            }
        }
        if (cur->is_tuple()) {
            for (auto& e : cur->as_tuple()) {
                if (visited.insert(&e).second) { stack.push_back(&e); sz += 48; }
            }
        }
        if (cur->is_struct()) {
            for (auto& f : cur->as_struct().fields) {
                if (visited.insert(&f.second).second) { stack.push_back(&f.second); sz += 48; }
            }
        }
        if (stack.size() > 1000000) return sz;
    }
    return sz;
}

void Interpreter::track_memory(int64_t bytes, const SourceRange& loc) {
    _memory_used += bytes;
    if (_memory_used > MEMORY_LIMIT) {
        runtime_error("RUN005", loc,
            "内存使用超限（>" + std::to_string(MEMORY_LIMIT / 1024 / 1024) + "MB）");
    }
}

Value Interpreter::eval_index(IndexExpr& e) {
    Value arr = eval(*e.array);
    Value idx = eval(*e.index);
    if (arr.is_array()) {
        if (!idx.is_int()) runtime_error("RUN151", e.loc, "数组索引必须是整数，得到: " + idx.type_name());
        int64_t i = idx.as_int();
        if (i < 0 || i >= static_cast<int64_t>(arr.as_array().size())) {
            runtime_error("RUN002", e.loc,
                "数组越界访问：下标 " + std::to_string(i) + " 超出长度 " + std::to_string(arr.as_array().size()));
            return Value::make_null();
        }
        return arr.as_array()[static_cast<size_t>(i)];
    } else if (arr.is_dict()) {
        auto& d = arr.as_dict();
        for (const auto& kv : d.entries) {
            if (kv.first.equals(idx)) return kv.second;
        }
        runtime_error("RUN005", e.loc, "字典键不存在: " + idx.to_display());
        return Value::make_null();
    }
    runtime_error("RUN005", e.loc, "对非数组/字典进行索引");
    return Value::make_null();
}

Value Interpreter::eval_member(MemberExpr& e) {
    Value obj = eval(*e.object);
    // 异常成员访问
    if (obj.is_exception()) {
        auto& exc = obj.as_exception();
        if (e.member == "message") return Value::make_string(exc.message);
        if (e.member == "type_name") return Value::make_string(exc.type_name);
        if (e.member == "what") return Value::make_string(exc.message);
        runtime_error("RUN005", e.loc,
            std::string("异常无属性 '") + e.member + "'");
        return Value::make_null();
    }
    // 模块成员访问
    if (obj.is_module()) {
        auto& mod = obj.as_module();
        auto it = mod.dict.find(e.member);
        if (it != mod.dict.end()) {
            return it->second;
        }
        runtime_error("RUN005", e.loc,
            std::string("模块 '") + mod.name + "' 无成员 '" + e.member + "'");
        return Value::make_null();
    }
    // OOP 4.3：类实例属性访问
    if (obj.is_struct()) {
        const auto& si = obj.as_struct();
        // 判断是否为类实例（_class_objects 中存在同名类）
        if (_class_objects.count(si.struct_name) > 0) {
            // 1. 先查实例 dict
            auto it = si.fields.find(e.member);
            if (it != si.fields.end()) {
                // 4.4 访问控制：沿 MRO 查找字段定义处的访问级别
                {
                    const auto& target_mro = _class_objects[si.struct_name].mro;
                    for (const auto& mro_name : target_mro) {
                        auto mit = _class_objects.find(mro_name);
                        if (mit == _class_objects.end()) continue;
                        auto fit = mit->second.field_access.find(e.member);
                        if (fit != mit->second.field_access.end()) {
                            check_access_control(e.member, fit->second,
                                                 is_inside_class_access(mro_name), e.loc);
                            break;
                        }
                    }
                }
                // 4.3 描述符协议：若值为描述符（有 __get__），调用 __get__
                Value v = it->second;
                if (v.is_class() || v.is_struct()) {
                    Value desc_result = eval_descriptor(v, obj, Value::make_null(), e.loc);
                    if (!desc_result.is_null()) return desc_result;
                }
                return v;
            }
            // 2. 查类 MRO
            Value cls_attr = lookup_class_attr(si.struct_name, e.member);
            if (!cls_attr.is_null()) {
                // 方法 → 绑定 self
                if (cls_attr.is_fnref()) {
                    auto bm = std::make_shared<BoundMethodData>();
                    bm->instance = obj;
                    bm->method_name = cls_attr.as_fnref().name;
                    return Value::make_bound_method(bm);
                }
                // 类属性中存的是方法名映射
                auto& cls = _class_objects[si.struct_name];
                auto mit = cls.methods.find(e.member);
                if (mit != cls.methods.end()) {
                    auto bm = std::make_shared<BoundMethodData>();
                    bm->instance = obj;
                    bm->method_name = mit->second;
                    return Value::make_bound_method(bm);
                }
                // 4.3 描述符协议
                Value desc_result = eval_descriptor(cls_attr, obj, Value::make_null(), e.loc);
                if (!desc_result.is_null()) return desc_result;
                return cls_attr;
            }
            // 3. __getattr__ fallback
            Value getattr_hook = lookup_class_attr(si.struct_name, "__getattr__");
            if (!getattr_hook.is_null()) {
                std::vector<Value> ga_args;
                ga_args.push_back(obj);
                ga_args.push_back(Value::make_string(e.member));
                std::string fn_name = getattr_hook.is_fnref() ? getattr_hook.as_fnref().name : e.member;
                return call_function(fn_name, std::move(ga_args), e.loc);
            }
            runtime_error("RUN005", e.loc,
                std::string("类 '") + si.struct_name + "' 实例无属性 '" + e.member + "'");
            return Value::make_null();
        }
        // 普通结构体
        auto it2 = obj.as_struct().fields.find(e.member);
        if (it2 == obj.as_struct().fields.end()) {
            runtime_error("RUN005", e.loc, std::string("结构体字段 '") + e.member + "' 不存在");
            return Value::make_null();
        }
        return it2->second;
    }
    // OOP 4.3：类对象属性访问（静态属性/类方法）
    if (obj.is_class()) {
        const auto& cls = obj.as_class();
        Value attr = lookup_class_attr(cls.name, e.member);
        if (!attr.is_null()) return attr;
        runtime_error("RUN005", e.loc,
            std::string("类 '") + cls.name + "' 无属性 '" + e.member + "'");
        return Value::make_null();
    }
    runtime_error("RUN005", e.loc, "对非结构体/类对象进行成员访问");
    return Value::make_null();
}

namespace {
    // 检查 AST 节点中是否包含 yield 表达式
    bool contains_yield(Expr* e) {
        if (!e) return false;
        if (e->node_kind == NodeKind::YieldExpr) return true;
        if (auto* block = dynamic_cast<Block*>(e)) {
            for (auto& s : block->stmts) {
                if (!s) continue;
                if (s->node_kind == NodeKind::ExprStmt) {
                    auto* es = static_cast<ExprStmt*>(s.get());
                    if (contains_yield(es->expr.get())) return true;
                }
                if (s->node_kind == NodeKind::WhileStmt) {
                    auto* ws = static_cast<WhileStmt*>(s.get());
                    if (contains_yield(ws->cond.get()) || contains_yield(ws->body.get())) return true;
                }
                if (s->node_kind == NodeKind::IfStmt) {
                    auto* ifs = static_cast<IfStmt*>(s.get());
                    if (contains_yield(ifs->cond.get()) || contains_yield(ifs->then_block.get())) return true;
                    if (ifs->else_block.has_value() && ifs->else_block.value()) {
                        if (contains_yield(ifs->else_block.value().get())) return true;
                    }
                }
                if (s->node_kind == NodeKind::ForStmt) {
                    auto* fs = static_cast<ForStmt*>(s.get());
                    if (contains_yield(fs->iterable.get()) || contains_yield(fs->body.get())) return true;
                }
            }
        }
        if (auto* bin = dynamic_cast<BinaryExpr*>(e)) {
            if (contains_yield(bin->lhs.get()) || contains_yield(bin->rhs.get())) return true;
        }
        if (auto* unary = dynamic_cast<UnaryExpr*>(e)) {
            if (contains_yield(unary->operand.get())) return true;
        }
        if (auto* call = dynamic_cast<CallExpr*>(e)) {
            for (auto& arg : call->args) {
                if (contains_yield(arg.get())) return true;
            }
        }
        if (auto* ret = dynamic_cast<ReturnStmt*>(e)) {
            if (ret->value.has_value() && ret->value.value()) {
                if (contains_yield(ret->value.value().get())) return true;
            }
        }
        if (auto* ifs = dynamic_cast<IfStmt*>(e)) {
            if (contains_yield(ifs->cond.get())) return true;
            if (contains_yield(ifs->then_block.get())) return true;
            if (ifs->else_block.has_value() && ifs->else_block.value()) {
                if (contains_yield(ifs->else_block.value().get())) return true;
            }
        }
        if (auto* ws = dynamic_cast<WhileStmt*>(e)) {
            if (contains_yield(ws->cond.get()) || contains_yield(ws->body.get())) return true;
        }
        if (auto* fs = dynamic_cast<ForStmt*>(e)) {
            if (contains_yield(fs->iterable.get()) || contains_yield(fs->body.get())) return true;
        }
        return false;
    }
}

Value Interpreter::call_function(const std::string& name, std::vector<Value> args, const SourceRange& loc) {

    // 特殊处理：next 函数用于恢复生成器
    if (name == "next" && !args.empty() && args[0].is_generator()) {
        return resume_generator(args[0].as_generator(), loc);
    }
    
    // 特殊处理：异常类构造函数
    auto exc_it = _exception_mro.find(name);
    if (exc_it != _exception_mro.end()) {
        auto exc_data = std::make_shared<ExceptionData>();
        exc_data->type_name = name;
        if (!args.empty()) {
            exc_data->message = args[0].to_display();
        }
        return Value::make_exception(exc_data);
    }
    
    // 设置当前类（用于 super 与访问控制）：方法名格式为 ClassName.method
    auto dot_pos = name.find('.');
    std::string prev_class = _current_class;
    if (dot_pos != std::string::npos) {
        _current_class = name.substr(0, dot_pos);
    }
    // 真 RAII 守卫：所有 return/异常路径析构时自动恢复 _current_class
    struct CurrentClassGuard {
        Interpreter* self;
        std::string prev;
        CurrentClassGuard(Interpreter* s, std::string p) : self(s), prev(std::move(p)) {}
        ~CurrentClassGuard() { self->_current_class = prev; }
    } current_class_guard(this, prev_class);
    // 优先查内建函数注册表
    auto* bi = BuiltinRegistry::instance().lookup(name);
    if (bi) {
        int arity = static_cast<int>(args.size());
        if (arity < bi->min_arity || (bi->max_arity >= 0 && arity > bi->max_arity)) {
            runtime_error("RUN005", loc, std::string("内建函数 '") + name + "' 参数数量不匹配");
            return Value::make_null();
        }
        try {
            return bi->eval_fn(args, _out, _in, loc);
        } catch (const std::runtime_error& e) {
            auto exc_data = std::make_shared<ExceptionData>();
            exc_data->type_name = "Exception";
            exc_data->message = e.what();
            throw Next11Exception(Value::make_exception(exc_data));
        }
    }

    auto it_fn = _fn_map.find(name);
    if (it_fn != _fn_map.end() && it_fn->second && it_fn->second->body) {
        auto* fn = it_fn->second;
        // 检查是否为生成器函数（包含 yield）
        bool is_generator = contains_yield(fn->body.get());
        if (is_generator) {
            // 创建生成器对象,  对象
            auto gen_data = std::make_shared<GeneratorData>();
            gen_data->fn_def = fn;
            gen_data->name = name;
            gen_data->state = GeneratorData::Created;
            // 保存初始参数（用 new 分配，确保生命周期与生成器相同）
            auto* gen_env = new Environment();
            *gen_env = Environment::from_global(_env.global_scope());
            gen_env->push_scope();
            for (size_t i = 0; i < fn->params.size() && i < args.size(); ++i) {
                gen_env->declare(fn->params[i].name, args[i]);
            }
            gen_data->saved_env = static_cast<void*>(gen_env);
            gen_data->cleanup = [gen_env]() { delete gen_env; };
            return Value::make_generator(gen_data);
        }
        if (++_recursion_depth > MAX_RECURSION) {
            --_recursion_depth;
            runtime_error("RUN003", loc, "递归深度超限（>1000）", "检查递归是否有终止条件，或改用迭代实现");
            return Value::make_null();
        }
        CallFrame frame(name, _recursion_depth);
        auto saved_env = std::move(_env);
        _env = Environment::from_global(saved_env.global_scope());
        _env.push_scope();
        // 模块函数：注入模块变量到函数作用域
        if (dot_pos != std::string::npos) {
            // 尝试所有可能的模块前缀（支持多级 pkg.submod.fn）
            std::string remaining = name;
            std::string mod_name;
            size_t last_dot = std::string::npos;
            for (size_t i = 0; i < remaining.size(); ++i) {
                if (remaining[i] == '.') {
                    std::string prefix = remaining.substr(0, i);
                    auto mit = _sys_modules.find(prefix);
                    if (mit != _sys_modules.end() && mit->second.is_module()) {
                        mod_name = prefix;
                        last_dot = i;
                    }
                }
            }
            if (!mod_name.empty()) {
                auto mit = _sys_modules.find(mod_name);
                if (mit != _sys_modules.end() && mit->second.is_module()) {
                    for (auto& kv : mit->second.as_module().dict) {
                        declare_var(kv.first, kv.second);
                    }
                }
            }
        }
        // OOP: 方法调用时注入 self 和类名
        bool is_method_call = false;
        if (dot_pos != std::string::npos) {
            std::string cls_name = name.substr(0, dot_pos);
            // 如果是类方法（非模块函数），注入 self
            if (_class_objects.count(cls_name) > 0 && !args.empty()) {
                is_method_call = true;
                declare_var("self", args[0]);
                frame.bind_param("self", args[0]);
                // 同时注入类名变量，用于类属性访问
                auto cit = _class_objects.find(cls_name);
                if (cit != _class_objects.end()) {
                    declare_var(cls_name, Value::make_class(std::make_shared<ClassObjectData>(cit->second)));
                }
            }
        }
        // 绑定参数：方法调用时跳过 self（args[0] 是 self，params[0] 也是 self）
        size_t arg_start = is_method_call ? 1 : 0;
        size_t param_start = is_method_call ? 1 : 0;
        for (size_t i = 0; i + param_start < fn->params.size() && (arg_start + i) < args.size(); ++i) {
            frame.bind_param(fn->params[i + param_start].name, args[arg_start + i]);
            declare_var(fn->params[i + param_start].name, args[arg_start + i]);
        }
        _call_stack.push_back(std::move(frame));
        Value ret = Value::make_null();
        try {
            auto& body = *fn->body;
            if (body.node_kind == NodeKind::Block) {
                auto& block = static_cast<Block&>(body);
                for (auto& s : block.stmts) {
                    if (!s) continue;
                    auto sig = exec_stmt(*s);
                    if (sig == ExecSignal::Return) { ret = _return_value; break; }
                }
            } else {
                ret = eval(body);
            }
        } catch (const Next11Exception&) {
            if (!_call_stack.empty()) {
                _call_stack.back().set_return(Value::make_null());
                _call_stack.pop_back();
            }
            _env = std::move(saved_env);
            --_recursion_depth;
            throw;
        } catch (const YieldSignal&) {
            if (!_call_stack.empty()) {
                _call_stack.back().set_return(Value::make_null());
                _call_stack.pop_back();
            }
            _env = std::move(saved_env);
            --_recursion_depth;
            throw;
        } catch (const std::exception& e) {
            if (!_call_stack.empty()) {
                _call_stack.back().set_return(Value::make_null());
                _call_stack.pop_back();
            }
            _env = std::move(saved_env);
            --_recursion_depth;
            auto exc_data = std::make_shared<ExceptionData>();
            exc_data->type_name = "RuntimeError";
            exc_data->message = e.what();
            throw Next11Exception(Value::make_exception(exc_data));
        }
        if (!_call_stack.empty()) {
            _call_stack.back().set_return(ret);
            _call_stack.pop_back();
        }
        // 模块函数：持久化变量变更回模块 dict
        if (dot_pos != std::string::npos) {
            std::string mod_name;
            for (size_t i = 0; i < name.size(); ++i) {
                if (name[i] == '.') {
                    std::string prefix = name.substr(0, i);
                    auto mit = _sys_modules.find(prefix);
                    if (mit != _sys_modules.end() && mit->second.is_module()) {
                        mod_name = prefix;
                    }
                }
            }
            if (!mod_name.empty()) {
                auto mit = _sys_modules.find(mod_name);
                if (mit != _sys_modules.end() && mit->second.is_module()) {
                    auto& mod_dict = mit->second.as_module().dict;
                    for (auto& kv : mod_dict) {
                        Value* v = _env.lookup(kv.first);
                        if (v) kv.second = *v;
                    }
                }
            }
        }
        // 全局变量变更持久化：将修改后的全局 scope 写回
        saved_env.update_global(_env.global_scope());
        _env = std::move(saved_env);
        --_recursion_depth;
        return ret;
    }

    auto entry = _symbols->lookup(name);
    if (!entry || entry->category != SymbolCategory::Function) {
        runtime_error("RUN005", loc, std::string("未声明函数 '") + name + "'");
        return Value::make_null();
    }

    if (++_recursion_depth > MAX_RECURSION) {
        --_recursion_depth;
        runtime_error("RUN003", loc, "递归深度超限（>1000）", "检查递归是否有终止条件，或改用迭代实现");
        return Value::make_null();
    }

    // 创建 CallFrame，保存环境
    CallFrame frame(name, _recursion_depth);
    auto saved_env = std::move(_env);
    _env = Environment::from_global(saved_env.global_scope());
    _env.push_scope(); // 函数作用域

    // 绑定形参到 CallFrame
    for (size_t i = 0; i < entry->params.size() && i < args.size(); ++i) {
        frame.bind_param(entry->params[i].name, args[i]);
        declare_var(entry->params[i].name, args[i]);
    }
    _call_stack.push_back(std::move(frame));

    // 执行函数体
    Value ret = Value::make_null();
    try {
        auto it = _fn_map.find(name);
        if (it != _fn_map.end() && it->second && it->second->body) {
            auto& body = *it->second->body;
            if (body.node_kind == NodeKind::Block) {
                auto& block = static_cast<Block&>(body);
                for (auto& s : block.stmts) {
                    if (!s) continue;
                    auto sig = exec_stmt(*s);
                    if (sig == ExecSignal::Return) { ret = _return_value; break; }
                }
            } else {
                ret = eval(body);
            }
        }
    } catch (const Next11Exception&) {
        if (!_call_stack.empty()) {
            _call_stack.back().set_return(Value::make_null());
            _call_stack.pop_back();
        }
        _env = std::move(saved_env);
        --_recursion_depth;
        throw;
    } catch (const std::exception& e) {
        if (!_call_stack.empty()) {
            _call_stack.back().set_return(Value::make_null());
            _call_stack.pop_back();
        }
        _env = std::move(saved_env);
        --_recursion_depth;
        auto exc_data = std::make_shared<ExceptionData>();
        exc_data->type_name = "RuntimeError";
        exc_data->message = e.what();
        throw Next11Exception(Value::make_exception(exc_data));
    }

    // 弹出 Environment，释放 CallFrame
    if (!_call_stack.empty()) {
        _call_stack.back().set_return(ret);
        _call_stack.pop_back();
    }
    _env = std::move(saved_env);
    --_recursion_depth;

    return ret;
}

Value Interpreter::eval_call(CallExpr& e) {
    std::vector<Value> args;
    for (auto& a : e.args) {
        if (a) args.push_back(eval(*a));
    }
    if (e.callee && e.callee->node_kind == NodeKind::IdentExpr) {
        auto name = static_cast<IdentExpr*>(e.callee.get())->name;
        // 先查找变量（可能是模块导入的函数）
        auto* var_val = lookup_var(name);
        if (var_val && var_val->is_fnref()) {
            return call_function(var_val->as_fnref().name, std::move(args), e.loc);
        }
        // OOP 4.2：类名调用 → 实例化
        if (_class_objects.count(name) > 0) {
            return instantiate_class(name, std::move(args), e.loc);
        }
        return call_function(name, std::move(args), e.loc);
    }
    // OOP 4.4：super() 调用
    if (e.callee && e.callee->node_kind == NodeKind::SuperExpr) {
        auto& se = static_cast<SuperExpr&>(*e.callee);
        Value self = Value::make_null();
        if (!_call_stack.empty()) {
            auto& frame = _call_stack.back();
            auto it = frame.params.find("self");
            if (it != frame.params.end()) self = it->second;
        }
        std::string method = se.method_name.empty() ? "__init__" : se.method_name;
        return eval_super(_current_class, self, method, std::move(args), e.loc);
    }
    // OOP 4.4：super.method(args) 调用
    if (e.callee && e.callee->node_kind == NodeKind::MemberExpr) {
        auto& me = static_cast<MemberExpr&>(*e.callee);
        if (me.object && me.object->node_kind == NodeKind::SuperExpr) {
            Value self = Value::make_null();
            if (!_call_stack.empty()) {
                auto& frame = _call_stack.back();
                auto it = frame.params.find("self");
                if (it != frame.params.end()) self = it->second;
            }
            return eval_super(_current_class, self, me.member, std::move(args), e.loc);
        }
    }
    // 类对象 / bound method 调用
    if (e.callee) {
        Value v = eval(*e.callee);
        // OOP 4.2：类对象调用 → 实例化
        if (v.is_class()) {
            return instantiate_class(v.as_class().name, std::move(args), e.loc);
        }
        // OOP 4.5：bound method 调用
        if (v.is_bound_method()) {
            auto& bm = v.as_bound_method();
            // 4.4 访问控制：方法名形如 "Class.method"，沿 MRO 查方法访问级别
            {
                std::string full = bm.method_name;
                size_t dot = full.find('.');
                if (dot != std::string::npos) {
                    std::string owner = full.substr(0, dot);
                    std::string mname = full.substr(dot + 1);
                    auto oit = _class_objects.find(owner);
                    if (oit != _class_objects.end()) {
                        auto mait = oit->second.method_access.find(mname);
                        if (mait != oit->second.method_access.end()) {
                            check_access_control(mname, mait->second,
                                                 is_inside_class_access(owner), e.loc);
                        }
                    }
                }
            }
            std::vector<Value> full_args;
            full_args.push_back(bm.instance);
            for (auto& a : args) full_args.push_back(std::move(a));
            return call_function(bm.method_name, std::move(full_args), e.loc);
        }
        // 函数引用调用
        if (v.is_fnref()) {
            return call_function(v.as_fnref().name, std::move(args), e.loc);
        }
        runtime_error("RUN030", e.loc, "不能调用非函数值: " + v.type_name());
    }
    runtime_error("RUN033", e.loc, "调用表达式缺少被调用对象");
    return Value::make_null();
}

Value Interpreter::eval_pipe(PipeExpr& e) {
    Value lhs = eval(*e.lhs);
    if (!e.call) return lhs;
    std::vector<Value> args;
    args.push_back(lhs);
    for (auto& a : e.call->args) {
        if (a) args.push_back(eval(*a));
    }
    if (e.call->callee && e.call->callee->node_kind == NodeKind::IdentExpr) {
        auto name = static_cast<IdentExpr*>(e.call->callee.get())->name;
        return call_function(name, std::move(args), e.loc);
    }
    runtime_error("RUN005", e.loc, "管道右侧必须为命名函数调用");
    return Value::make_null();
}

bool Interpreter::match_pattern(Expr& pattern, const Value& val, MatchArm& arm) {
    if (arm.is_wildcard) return true;
    if (arm.is_var_bind) {
        if (pattern.node_kind == NodeKind::IdentExpr) {
            auto& id = static_cast<IdentExpr&>(pattern);
            // 枚举变体模式按字面量比较，而非变量绑定
            if (_enum_variants.count(id.name) > 0) {
                return Value::make_string(id.name).equals(val);
            }
            declare_var(id.name, val);
        }
        return true;
    }
    // 字面量模式
    Value pv = eval(pattern);
    return pv.equals(val);
}

Value Interpreter::eval_match(MatchExpr& e) {
    Value scrut = eval(*e.scrutinee);
    for (auto& arm : e.arms) {
        if (!arm.pattern) continue;
        _env.push_scope();
        try {
            bool matched = match_pattern(*arm.pattern, scrut, arm);
            if (matched && arm.guard) {
                Value gv = eval(*arm.guard);
                matched = gv.truthy();
            }
            Value result;
            if (matched) {
                result = arm.result ? eval(*arm.result) : Value::make_null();
                _env.pop_scope();
                return result;
            }
            _env.pop_scope();
        } catch (...) {
            _env.pop_scope();
            throw;
        }
    }
        runtime_error("MAT001", e.loc, "match 表达式无匹配分支", "添加通配分支 '_' 或处理所有可能情况", ErrorCategory::Match);
    return Value::make_null();
}

void Interpreter::exec_destructure(const DestructurePattern& pat, const Value& src, const SourceRange& loc) {
    if (pat.is_array) {
        if (!src.is_array()) { runtime_error("DST003", loc, "解构类型不兼容：期望数组", std::nullopt, ErrorCategory::Destructure); return; }
        if (pat.elements.size() != src.as_array().size()) {
            runtime_error("DST001", loc,
                "数组解构数量不匹配：期望 " + std::to_string(pat.elements.size()) +
                " 实际 " + std::to_string(src.as_array().size()), std::nullopt, ErrorCategory::Destructure);
        }
        for (size_t i = 0; i < pat.elements.size() && i < src.as_array().size(); ++i) {
            auto& sub = pat.elements[i];
            if (sub.is_wildcard) continue;
            if (sub.is_leaf) declare_var(sub.var_name, src.as_array()[i]);
            else exec_destructure(sub, src.as_array()[i], loc);
        }
    } else if (pat.is_struct) {
        if (!src.is_struct()) { runtime_error("DST003", loc, "解构类型不兼容：期望结构体", std::nullopt, ErrorCategory::Destructure); return; }
        for (auto& f : pat.fields) {
            auto it = src.as_struct().fields.find(f.first);
            if (it == src.as_struct().fields.end()) {
                runtime_error("DST002", loc, std::string("结构体字段 '") + f.first + "' 不存在", std::nullopt, ErrorCategory::Destructure);
                continue;
            }
            if (f.second.is_leaf) declare_var(f.second.var_name, it->second);
            else exec_destructure(f.second, it->second, loc);
        }
    } else if (pat.is_tuple) {
        if (!src.is_tuple()) { runtime_error("DST003", loc, "解构类型不兼容：期望元组", std::nullopt, ErrorCategory::Destructure); return; }
        if (pat.elements.size() != src.as_tuple().size()) {
            runtime_error("DST001", loc,
                "元组解构数量不匹配：期望 " + std::to_string(pat.elements.size()) +
                " 实际 " + std::to_string(src.as_tuple().size()), std::nullopt, ErrorCategory::Destructure);
        }
        for (size_t i = 0; i < pat.elements.size() && i < src.as_tuple().size(); ++i) {
            auto& sub = pat.elements[i];
            if (sub.is_wildcard) continue;
            if (sub.is_leaf) declare_var(sub.var_name, src.as_tuple()[i]);
            else exec_destructure(sub, src.as_tuple()[i], loc);
        }
    }
}

ExecSignal Interpreter::exec_block(Block& b) {
    _env.push_scope();
    try {
        for (auto& s : b.stmts) {
            if (!s) continue;
            auto sig = exec_stmt(*s);
            if (sig != ExecSignal::Normal) {
                _env.pop_scope();
                return sig;
            }
        }
        _env.pop_scope();
        return ExecSignal::Normal;
    } catch (...) {
        _env.pop_scope();
        throw;
    }
}

ExecSignal Interpreter::exec_stmt(Stmt& s) {

    switch (s.node_kind) {
        case NodeKind::LetDecl: {
            auto& let = static_cast<LetDecl&>(s);
            if (let.is_destructure) {
                if (let.init.has_value() && let.init.value()) {
                    Value src = eval(*let.init.value());
                    exec_destructure(let.destr_pattern, src, let.loc);
                }
            } else {
                Value v;
                if (let.init.has_value() && let.init.value()) {
                    v = eval(*let.init.value());
                } else {
                    v = fn_default_value(let.declared_type);
                }
                declare_var(let.name, v);
            }
            return ExecSignal::Normal;
        }
        case NodeKind::ExprStmt: {
            auto& es = static_cast<ExprStmt&>(s);
            if (es.expr) eval(*es.expr);
            return ExecSignal::Normal;
        }
        case NodeKind::IfStmt: {
            auto& ifs = static_cast<IfStmt&>(s);
            Value cond = eval(*ifs.cond);
            if (cond.truthy()) {
                if (ifs.then_block) {
                    if (ifs.then_block->node_kind == NodeKind::Block)
                        return exec_block(static_cast<Block&>(*ifs.then_block));
                    eval(*ifs.then_block);
                }
            } else if (ifs.else_block.has_value() && ifs.else_block.value()) {
                if (ifs.else_block.value()->node_kind == NodeKind::Block)
                    return exec_block(static_cast<Block&>(*ifs.else_block.value()));
                eval(*ifs.else_block.value());
            }
            return ExecSignal::Normal;
        }
        case NodeKind::WhileStmt: {
            auto& ws = static_cast<WhileStmt&>(s);
            _env.push_scope();
            try {
                while (true) {
                    Value cond = eval(*ws.cond);
                    if (!cond.truthy()) break;
                    if (++_loop_count > MAX_LOOP) {
                        _env.pop_scope();
                        runtime_error("RUN004", ws.loc, "循环次数超限（>10亿）");
                        return ExecSignal::Normal;
                    }
                    if (ws.body && ws.body->node_kind == NodeKind::Block) {
                        auto sig = exec_block(static_cast<Block&>(*ws.body));
                        if (sig == ExecSignal::Break) break;
                        if (sig == ExecSignal::Return) { _env.pop_scope(); return sig; }
                    }
                }
                _env.pop_scope();
                return ExecSignal::Normal;
            } catch (...) {
                _env.pop_scope();
                throw;
            }
        }
        case NodeKind::ForStmt: {
            auto& fs = static_cast<ForStmt&>(s);
            Value iter = eval(*fs.iterable);
            _env.push_scope();
            try {
                if (iter.is_array()) {
                    auto snapshot = iter.as_array();
                    for (auto& v : snapshot) {
                        if (++_loop_count > MAX_LOOP) {
                            _env.pop_scope();
                            runtime_error("RUN004", fs.loc, "循环次数超限（>10亿）");
                            return ExecSignal::Normal;
                        }
                        declare_var(fs.var_name, v);
                        if (fs.body && fs.body->node_kind == NodeKind::Block) {
                            auto sig = exec_block(static_cast<Block&>(*fs.body));
                            if (sig == ExecSignal::Break) break;
                            if (sig == ExecSignal::Return) { _env.pop_scope(); return sig; }
                        }
                    }
                } else if (iter.is_string()) {
                    for (char ch : iter.as_string()) {
                        if (++_loop_count > MAX_LOOP) {
                            _env.pop_scope();
                            runtime_error("RUN004", fs.loc, "循环次数超限（>10亿）");
                            return ExecSignal::Normal;
                        }
                        declare_var(fs.var_name, Value::make_string(std::string(1, ch)));
                        if (fs.body && fs.body->node_kind == NodeKind::Block) {
                            auto sig = exec_block(static_cast<Block&>(*fs.body));
                            if (sig == ExecSignal::Break) break;
                            if (sig == ExecSignal::Continue) continue;
                            if (sig == ExecSignal::Return) { _env.pop_scope(); return sig; }
                        }
                    }
                } else if (iter.is_tuple()) {
                    for (auto& v : iter.as_tuple()) {
                        if (++_loop_count > MAX_LOOP) {
                            _env.pop_scope();
                            runtime_error("RUN004", fs.loc, "循环次数超限（>10亿）");
                            return ExecSignal::Normal;
                        }
                        declare_var(fs.var_name, v);
                        if (fs.body && fs.body->node_kind == NodeKind::Block) {
                            auto sig = exec_block(static_cast<Block&>(*fs.body));
                            if (sig == ExecSignal::Break) break;
                            if (sig == ExecSignal::Return) { _env.pop_scope(); return sig; }
                        }
                    }
                } else if (iter.is_set()) {
                    auto snapshot = iter.as_set().elements;
                    for (auto& v : snapshot) {
                        if (++_loop_count > MAX_LOOP) {
                            _env.pop_scope();
                            runtime_error("RUN004", fs.loc, "循环次数超限（>10亿）");
                            return ExecSignal::Normal;
                        }
                        declare_var(fs.var_name, v);
                        if (fs.body && fs.body->node_kind == NodeKind::Block) {
                            auto sig = exec_block(static_cast<Block&>(*fs.body));
                            if (sig == ExecSignal::Break) break;
                            if (sig == ExecSignal::Continue) continue;
                            if (sig == ExecSignal::Return) { _env.pop_scope(); return sig; }
                        }
                    }
                } else if (iter.is_dict()) {
                    auto snapshot = iter.as_dict().entries;
                    for (auto& kv : snapshot) {
                        if (++_loop_count > MAX_LOOP) {
                            _env.pop_scope();
                            runtime_error("RUN004", fs.loc, "循环次数超限（>10亿）");
                            return ExecSignal::Normal;
                        }
                        declare_var(fs.var_name, kv.first);
                        if (fs.body && fs.body->node_kind == NodeKind::Block) {
                            auto sig = exec_block(static_cast<Block&>(*fs.body));
                            if (sig == ExecSignal::Break) break;
                            if (sig == ExecSignal::Continue) continue;
                            if (sig == ExecSignal::Return) { _env.pop_scope(); return sig; }
                        }
                    }
                } else if (iter.is_generator()) {
                    while (true) {
                        if (++_loop_count > MAX_LOOP) {
                            _env.pop_scope();
                            runtime_error("RUN004", fs.loc, "循环次数超限（>10亿）");
                            return ExecSignal::Normal;
                        }
                        std::vector<Value> next_args = {iter};
                        Value next_val;
                        try {
                            next_val = call_function("next", next_args, fs.loc);
                        } catch (const Next11Exception&) {
                            break;
                        } catch (const std::runtime_error&) {
                            break;
                        }
                        if (next_val.is_null()) break;
                        declare_var(fs.var_name, next_val);
                        if (fs.body && fs.body->node_kind == NodeKind::Block) {
                            auto sig = exec_block(static_cast<Block&>(*fs.body));
                            if (sig == ExecSignal::Break) break;
                            if (sig == ExecSignal::Continue) continue;
                            if (sig == ExecSignal::Return) { _env.pop_scope(); return sig; }
                        }
                    }
                } else {
                    _env.pop_scope();
                    runtime_error("RUN305", fs.loc, "for 循环不支持迭代类型: " + iter.type_name());
                    return ExecSignal::Normal;
                }
                _env.pop_scope();
                return ExecSignal::Normal;
            } catch (...) {
                _env.pop_scope();
                throw;
            }
        }
        case NodeKind::ReturnStmt: {
            auto& rs = static_cast<ReturnStmt&>(s);
            if (rs.value.has_value() && rs.value.value()) {
                _return_value = eval(*rs.value.value());
            } else {
                _return_value = Value::make_null();
            }
            return ExecSignal::Return;
        }
        case NodeKind::BreakStmt: return ExecSignal::Break;
        case NodeKind::ContinueStmt: return ExecSignal::Continue;
        case NodeKind::FnDef:
        case NodeKind::AsyncFnDef: {
            auto& fn = static_cast<FnDef&>(s);
            // 注册函数到函数映射表
            _fn_map[fn.name] = &fn;
            // 应用装饰器：从内到外
            if (!fn.decorators.empty()) {
                std::string current_name = fn.name;
                for (auto it = fn.decorators.rbegin(); it != fn.decorators.rend(); ++it) {
                    const auto& decorator_name = it->first;
                    const auto& decorator_args = it->second;
                    // 准备调用参数：第一个参数是被装饰的函数
                    std::vector<Value> args;
                    args.push_back(Value::make_fnref(FnRef(current_name)));
                    for (auto& arg_expr : decorator_args) {
                        args.push_back(eval(*arg_expr));
                    }
                    // 调用装饰器函数
                    Value result = call_function(decorator_name, args, fn.loc);
                    // 装饰器返回新的函数，更新引用
                    if (result.is_fnref()) {
                        current_name = result.as_fnref().name;
                    }
                }
                // 更新函数映射
                if (current_name != fn.name) {
                    _fn_map.erase(fn.name);
                    fn.name = current_name;
                    _fn_map[fn.name] = &fn;
                }
            }
            return ExecSignal::Normal;
        }
        case NodeKind::ClassDef: {
            auto& cd = static_cast<ClassDef&>(s);
            create_class(cd);
            return ExecSignal::Normal;
        }
        case NodeKind::ImportStmt: {
            auto& imp = static_cast<ImportStmt&>(s);


            if (imp.is_from_import) {
                // from mod import name
                Value module_val = load_module(imp.module_path, imp.loc);
                if (imp.is_wildcard) {
                    // from mod import *
                    import_all_from_module(module_val, imp.loc);
                } else {
                    // 导入指定名称
                    for (const auto& name_spec : imp.imported_names) {
                        // 解析 name 或 name as alias
                        size_t as_pos = name_spec.find(" as ");
                        std::string orig_name, alias;
                        if (as_pos != std::string::npos) {
                            orig_name = name_spec.substr(0, as_pos);
                            alias = name_spec.substr(as_pos + 4);
                        } else {
                            orig_name = name_spec;
                            alias = orig_name;
                        }
                        // 从模块中查找名称
                        if (module_val.is_module()) {
                            auto& mod = module_val.as_module();
                            auto it = mod.dict.find(orig_name);
                            if (it != mod.dict.end()) {
                                declare_var(alias, it->second);
                            } else {
                                runtime_error("SEM007", imp.loc,
                                    std::string("模块 '") + mod.name + "' 无导出名称 '" + orig_name + "'");
                            }
                        }
                    }
                }
            } else {
                // import mod 或 import mod as alias
                Value module_val = load_module(imp.module_path, imp.loc);
                std::string alias = imp.alias.empty() ? (imp.module_path.empty() ? "" : imp.module_path.back()) : imp.alias;
                declare_var(alias, module_val);
            }
            return ExecSignal::Normal;
        }

        // ===== 异常处理（3.2-3.3）=====
        case NodeKind::TryStmt: return exec_try(static_cast<TryStmt&>(s));
        case NodeKind::ThrowStmt: {
            auto& ts = static_cast<ThrowStmt&>(s);
            Value exc;
            if (ts.expr) {
                exc = eval(*ts.expr);
            }
            // 非 Exception 类型的值自动包装为 Exception
            if (!exc.is_exception()) {
                auto exc_data = std::make_shared<ExceptionData>();
                exc_data->type_name = "Exception";
                exc_data->message = exc.to_display();
                exc = Value::make_exception(exc_data);
            }
            // 异常路径才捕获栈追踪（零开销）
            exc.as_exception().stack_trace = capture_stack_trace();
            throw Next11Exception(exc);
        }
        case NodeKind::WithStmt: return exec_with(static_cast<WithStmt&>(s));
        case NodeKind::DelStmt: return exec_del(static_cast<DelStmt&>(s));

        default: runtime_error("RUN337", s.loc, "未知语句节点类型");
    }
    return {};
}

std::vector<StackFrame> Interpreter::capture_stack_trace() const {
    return {};
}

void Interpreter::init_exception_classes() {
    _exception_mro["Exception"] = {"Exception"};
    _exception_mro["RuntimeError"] = {"RuntimeError", "Exception"};
    _exception_mro["ValueError"] = {"ValueError", "Exception"};
    _exception_mro["TypeError"] = {"TypeError", "Exception"};
    _exception_mro["KeyError"] = {"KeyError", "LookupError", "Exception"};
    _exception_mro["IndexError"] = {"IndexError", "LookupError", "Exception"};
    _exception_mro["AttributeError"] = {"AttributeError", "Exception"};
    _exception_mro["IOError"] = {"IOError", "Exception"};
    _exception_mro["ArithmeticError"] = {"ArithmeticError", "Exception"};
    _exception_mro["ZeroDivisionError"] = {"ZeroDivisionError", "ArithmeticError", "Exception"};
    _exception_mro["OverflowError"] = {"OverflowError", "ArithmeticError", "Exception"};
    _exception_mro["LookupError"] = {"LookupError", "Exception"};
    _exception_mro["NameError"] = {"NameError", "Exception"};
    _exception_mro["StopIteration"] = {"StopIteration", "Exception"};
}

bool Interpreter::exception_matches(const std::string& exc_type,
                                    const std::vector<std::string>& catch_types) const {
    if (catch_types.empty()) return true;
    auto mro_it = _exception_mro.find(exc_type);
    if (mro_it != _exception_mro.end()) {
        for (const auto& ct : catch_types) {
            for (const auto& mro_type : mro_it->second) {
                if (ct == mro_type) return true;
            }
        }
    } else {
        for (const auto& ct : catch_types) {
            if (ct == exc_type || ct == "Exception") return true;
        }
    }
    return false;
}

ExecSignal Interpreter::exec_try(TryStmt& s) {
    ExecSignal result = ExecSignal::Normal;
    bool exception_caught = false;

    auto exec_finally = [&]() {
        if (s.finally_block.has_value() && s.finally_block.value()) {
            auto& fb = *s.finally_block.value();
            if (fb.node_kind == NodeKind::Block) {
                auto& block = static_cast<Block&>(fb);
                _env.push_scope();
                try {
                    for (auto& stmt : block.stmts) { if (stmt) exec_stmt(*stmt); }
                } catch (...) {
                    _env.pop_scope();
                    throw;
                }
                _env.pop_scope();
            }
        }
    };
    
    // 执行 try 块
    try {
        if (s.try_block) {
            auto& block_expr = *s.try_block;
            if (block_expr.node_kind == NodeKind::Block) {
                auto& block = static_cast<Block&>(block_expr);
                _env.push_scope();
                try {
                    for (auto& stmt : block.stmts) {
                        if (!stmt) continue;
                        auto sig = exec_stmt(*stmt);
                        if (sig == ExecSignal::Return) {
                            result = ExecSignal::Return;
                            break;
                        }
                        if (sig == ExecSignal::Break || sig == ExecSignal::Continue) {
                            result = sig;
                            break;
                        }
                    }
                } catch (...) {
                    _env.pop_scope();
                    throw;
                }
                _env.pop_scope();
            } else {
                eval(block_expr);
            }
        }
    } catch (const Next11Exception& ne) {
        std::string exc_type = ne.exc_value.is_exception() ? ne.exc_value.as_exception().type_name : "Exception";
        if (s.catch_block && !exception_matches(exc_type, s.catch_types)) {
            if (s.finally_block.has_value() && s.finally_block.value()) {
                auto& fb = *s.finally_block.value();
                if (fb.node_kind == NodeKind::Block) {
                    auto& block = static_cast<Block&>(fb);
                    _env.push_scope();
                    try {
                        for (auto& stmt : block.stmts) { if (stmt) exec_stmt(*stmt); }
                    } catch (...) {
                        _env.pop_scope();
                        throw;
                    }
                    _env.pop_scope();
                }
            }
            throw;
        }
        exception_caught = true;
        if (s.catch_block) {
            try {
                _env.push_scope();
                if (!s.catch_var.empty()) {
                    declare_var(s.catch_var, ne.exc_value);
                }
                auto& cb = *s.catch_block;
                if (cb.node_kind == NodeKind::Block) {
                    auto& block = static_cast<Block&>(cb);
                    for (auto& stmt : block.stmts) {
                        if (!stmt) continue;
                        auto sig = exec_stmt(*stmt);
                        if (sig == ExecSignal::Return) {
                            result = ExecSignal::Return;
                            break;
                        }
                        if (sig == ExecSignal::Break) {
                            result = ExecSignal::Break;
                            break;
                        }
                        if (sig == ExecSignal::Continue) {
                            result = ExecSignal::Continue;
                            break;
                        }
                    }
                } else {
                    eval(cb);
                }
                _env.pop_scope();
            } catch (...) {
                _env.pop_scope();
                exec_finally();
                throw;
            }
        } else {
            // 无 catch 块，重新抛出（让外层处理）
            if (s.finally_block.has_value() && s.finally_block.value()) {
                // 先执行 finally，再重新抛出
                auto& fb = *s.finally_block.value();
                if (fb.node_kind == NodeKind::Block) {
                    auto& block = static_cast<Block&>(fb);
                    _env.push_scope();
                    try {
                        for (auto& stmt : block.stmts) {
                            if (stmt) exec_stmt(*stmt);
                        }
                    } catch (...) {
                        _env.pop_scope();
                        throw;
                    }
                    _env.pop_scope();
                }
            }
            throw;
        }
    } catch (const std::runtime_error& e) {
        auto exc_data = std::make_shared<ExceptionData>();
        exc_data->type_name = "RuntimeError";
        exc_data->message = e.what();
        Next11Exception ne(Value::make_exception(exc_data));
        if (s.catch_block && !exception_matches("RuntimeError", s.catch_types)) {
            if (s.finally_block.has_value() && s.finally_block.value()) {
                auto& fb = *s.finally_block.value();
                if (fb.node_kind == NodeKind::Block) {
                    auto& block = static_cast<Block&>(fb);
                    _env.push_scope();
                    try {
                        for (auto& stmt : block.stmts) { if (stmt) exec_stmt(*stmt); }
                    } catch (...) {
                        _env.pop_scope();
                        throw;
                    }
                    _env.pop_scope();
                }
            }
            throw;
        }
        exception_caught = true;
        if (s.catch_block) {
            try {
                _env.push_scope();
                if (!s.catch_var.empty()) {
                    declare_var(s.catch_var, ne.exc_value);
                }
                auto& cb = *s.catch_block;
                if (cb.node_kind == NodeKind::Block) {
                    auto& block = static_cast<Block&>(cb);
                    for (auto& stmt : block.stmts) {
                        if (!stmt) continue;
                        auto sig = exec_stmt(*stmt);
                        if (sig == ExecSignal::Return) {
                            result = ExecSignal::Return;
                            break;
                        }
                        if (sig == ExecSignal::Break) {
                            result = ExecSignal::Break;
                            break;
                        }
                        if (sig == ExecSignal::Continue) {
                            result = ExecSignal::Continue;
                            break;
                        }
                    }
                } else {
                    eval(cb);
                }
                _env.pop_scope();
            } catch (...) {
                _env.pop_scope();
                exec_finally();
                throw;
            }
        } else {
            if (s.finally_block.has_value() && s.finally_block.value()) {
                auto& fb = *s.finally_block.value();
                if (fb.node_kind == NodeKind::Block) {
                    auto& block = static_cast<Block&>(fb);
                    _env.push_scope();
                    try {
                        for (auto& stmt : block.stmts) {
                            if (stmt) exec_stmt(*stmt);
                        }
                    } catch (...) {
                        _env.pop_scope();
                        throw;
                    }
                    _env.pop_scope();
                }
            }
            throw;
        }
    } catch (const std::exception& e) {
        auto exc_data = std::make_shared<ExceptionData>();
        exc_data->type_name = "Exception";
        exc_data->message = e.what();
        Next11Exception ne(Value::make_exception(exc_data));
        if (s.catch_block && !exception_matches("Exception", s.catch_types)) {
            if (s.finally_block.has_value() && s.finally_block.value()) {
                auto& fb = *s.finally_block.value();
                if (fb.node_kind == NodeKind::Block) {
                    auto& block = static_cast<Block&>(fb);
                    _env.push_scope();
                    try {
                        for (auto& stmt : block.stmts) { if (stmt) exec_stmt(*stmt); }
                    } catch (...) {
                        _env.pop_scope();
                        throw;
                    }
                    _env.pop_scope();
                }
            }
            throw;
        }
        exception_caught = true;
        if (s.catch_block) {
            try {
                _env.push_scope();
                if (!s.catch_var.empty()) {
                    declare_var(s.catch_var, ne.exc_value);
                }
                auto& cb = *s.catch_block;
                if (cb.node_kind == NodeKind::Block) {
                    auto& block = static_cast<Block&>(cb);
                    for (auto& stmt : block.stmts) {
                        if (!stmt) continue;
                        auto sig = exec_stmt(*stmt);
                        if (sig == ExecSignal::Return) {
                            result = ExecSignal::Return;
                            break;
                        }
                        if (sig == ExecSignal::Break) {
                            result = ExecSignal::Break;
                            break;
                        }
                        if (sig == ExecSignal::Continue) {
                            result = ExecSignal::Continue;
                            break;
                        }
                    }
                } else {
                    eval(cb);
                }
                _env.pop_scope();
            } catch (...) {
                _env.pop_scope();
                exec_finally();
                throw;
            }
        } else {
            if (s.finally_block.has_value() && s.finally_block.value()) {
                auto& fb = *s.finally_block.value();
                if (fb.node_kind == NodeKind::Block) {
                    auto& block = static_cast<Block&>(fb);
                    _env.push_scope();
                    try {
                        for (auto& stmt : block.stmts) {
                            if (stmt) exec_stmt(*stmt);
                        }
                    } catch (...) {
                        _env.pop_scope();
                        throw;
                    }
                    _env.pop_scope();
                }
            }
            throw;
        }
    }
    
    // 执行 finally 块（无论是否异常）
    if (s.finally_block.has_value() && s.finally_block.value()) {
        auto& fb = *s.finally_block.value();
        if (fb.node_kind == NodeKind::Block) {
            auto& block = static_cast<Block&>(fb);
            _env.push_scope();
            try {
                for (auto& stmt : block.stmts) {
                    if (!stmt) continue;
                    auto sig = exec_stmt(*stmt);
                    if (sig == ExecSignal::Return) {
                        result = ExecSignal::Return;
                        break;
                    }
                    if (sig == ExecSignal::Break) {
                        result = ExecSignal::Break;
                        break;
                    }
                    if (sig == ExecSignal::Continue) {
                        result = ExecSignal::Continue;
                        break;
                    }
                }
            } catch (...) {
                _env.pop_scope();
                throw;
            }
            _env.pop_scope();
        } else {
            eval(fb);
        }
    }
    
    return result;
}

ExecSignal Interpreter::exec_with(WithStmt& s) {
    // 评估上下文表达式
    Value ctx_val;
    if (s.context_expr) {
        ctx_val = eval(*s.context_expr);
    }
    
    // 调用 __enter__
    Value enter_result;
    if (ctx_val.is_struct()) {
        auto& si = ctx_val.as_struct();
        Value enter_attr = lookup_class_attr(si.struct_name, "__enter__");
        if (enter_attr.is_null()) {
            runtime_error("RUN027", s.loc, std::string("with 语句的对象缺少 __enter__ 方法: ") + si.struct_name);
        }
        if (!enter_attr.is_fnref()) {
            runtime_error("RUN028", s.loc, std::string("__enter__ 必须是方法: ") + si.struct_name);
        }
        std::vector<Value> args;
        args.push_back(ctx_val);
        enter_result = call_function(enter_attr.as_fnref().name, std::move(args), s.loc);
    } else {
        runtime_error("RUN027", s.loc, "with 语句需要实现 __enter__/__exit__ 的对象，得到: " + ctx_val.type_name());
    }
    
    ExecSignal result = ExecSignal::Normal;
    bool had_exception = false;
    Value exc_value;
    
    // 执行 with 块
    try {
        if (s.with_block) {
            _env.push_scope();
            try {
                if (!s.as_name.empty()) {
                    declare_var(s.as_name, enter_result);
                }
                auto& wb = *s.with_block;
                if (wb.node_kind == NodeKind::Block) {
                    auto& block = static_cast<Block&>(wb);
                    for (auto& stmt : block.stmts) {
                        if (!stmt) continue;
                        auto sig = exec_stmt(*stmt);
                        if (sig == ExecSignal::Return) {
                            result = ExecSignal::Return;
                            break;
                        }
                        if (sig == ExecSignal::Break) {
                            result = ExecSignal::Break;
                            break;
                        }
                        if (sig == ExecSignal::Continue) {
                            result = ExecSignal::Continue;
                            break;
                        }
                    }
                } else {
                    eval(wb);
                }
            } catch (...) {
                _env.pop_scope();
                throw;
            }
            _env.pop_scope();
        }
    } catch (const Next11Exception& ne) {
        had_exception = true;
        exc_value = ne.exc_value;
    }
    
    // 调用 __exit__
    bool exc_suppressed = false;
    if (ctx_val.is_struct()) {
        auto& si = ctx_val.as_struct();
        Value exit_attr = lookup_class_attr(si.struct_name, "__exit__");
        if (exit_attr.is_null()) {
            runtime_error("RUN029", s.loc, std::string("with 语句的对象缺少 __exit__ 方法: ") + si.struct_name);
        }
        if (!exit_attr.is_fnref()) {
            runtime_error("RUN028", s.loc, std::string("__exit__ 必须是方法: ") + si.struct_name);
        }
        std::vector<Value> args;
        args.push_back(ctx_val);
        if (had_exception) {
            args.push_back(exc_value);
        } else {
            args.push_back(Value::make_null());
        }
        Value exit_ret = call_function(exit_attr.as_fnref().name, std::move(args), s.loc);
        if (had_exception && exit_ret.truthy()) {
            exc_suppressed = true;
        }
    }
    
    // 如果有异常且未被 __exit__ 抑制，重新抛出
    if (had_exception && !exc_suppressed) {
        throw Next11Exception(exc_value);
    }
    
    return result;
}

ExecSignal Interpreter::exec_del(DelStmt& s) {
    if (!s.target) return ExecSignal::Normal;
    if (s.target->node_kind == NodeKind::IndexExpr) {
        // 字典删除: del dict[key]
        auto& ie = static_cast<IndexExpr&>(*s.target);
        Value container = eval(*ie.array);
        Value key = eval(*ie.index);
        if (container.is_dict()) {
            auto& dict_data = container.as_dict();
            // 查找并删除键值对
            bool found = false;
            for (auto it = dict_data.entries.begin(); it != dict_data.entries.end(); ++it) {
                if (it->first.equals(key)) {
                    dict_data.entries.erase(it);
                    found = true;
                    break;
                }
            }
            if (!found) {
                runtime_error("RUN005", s.target->loc, "字典键不存在: " + key.to_display());
            }
        } else {
            runtime_error("RUN005", s.target->loc, "del 只支持字典和对象属性");
        }
    } else if (s.target->node_kind == NodeKind::MemberExpr) {
        // 对象属性删除: del obj.attr
        auto& me = static_cast<MemberExpr&>(*s.target);
        Value obj = eval(*me.object);
        if (obj.is_struct()) {
            auto& si = obj.as_struct();
            auto it = si.fields.find(me.member);
            if (it != si.fields.end()) {
                si.fields.erase(it);
            } else {
                runtime_error("RUN005", s.target->loc, std::string("对象属性 '") + me.member + "' 不存在");
            }
        } else {
            runtime_error("RUN005", s.target->loc, "del 只支持字典和对象属性");
        }
    } else {
        runtime_error("RUN005", s.target->loc, "del 目标必须是索引或成员表达式");
    }
    return ExecSignal::Normal;
}

// ===== OOP 系统实现（4.1-4.5）=====

// 4.2 类创建：构造 ClassObjectData 并用 C3Linearizer 计算 MRO
void Interpreter::create_class(ClassDef& def) {
    ClassObjectData cls;
    cls.name = def.name;
    cls.metaclass = def.metaclass_name;
    cls.slots = def.slots;
    cls.bases = def.base_classes;
    if (!def.base_class.empty()) {
        // 单继承也加入 bases
        bool already = false;
        for (const auto& b : cls.bases) {
            if (b == def.base_class) { already = true; break; }
        }
        if (!already) cls.bases.insert(cls.bases.begin(), def.base_class);
    }

    // 注册方法：方法名 -> 函数全名（ClassName.method）
    // 同时把方法注册到 _fn_map（用 ClassName.method 形式），便于 call_function 查找
    for (auto& m : def.methods) {
        if (!m) continue;
        std::string full_name = def.name + "." + m->name;
        cls.methods[m->name] = full_name;
        cls.method_access[m->name] = m->access_level;
        // 把方法体注册到 _fn_map（用全名）
        _fn_map[full_name] = m.get();
        // 同时把方法作为 FnRef 存入类 dict（便于按 MRO 查找）
        cls.dict[m->name] = Value::make_fnref(FnRef(full_name));
        // 4.5 classmethod/staticmethod 标记：通过装饰器
        if (m->is_classmethod || m->is_staticmethod) {
            // 在 dict 中用 BoundMethodData 包装标记
            // classmethod：调用时第一个参数绑定类对象
            // staticmethod：不绑定 self
            // 这里仅存储标记，实际绑定在 instantiate_class/eval_member 中处理
            cls.dict["__method_kind_" + m->name] = Value::make_string(
                m->is_classmethod ? "classmethod" : "staticmethod");
        }
    }

    // 处理类属性（fields 中的非方法属性视为类属性默认值）
    for (auto& f : def.fields) {
        auto init_it = def.field_inits.find(f.name);
        if (init_it != def.field_inits.end() && init_it->second) {
            cls.dict[f.name] = eval(*init_it->second);
        } else {
            cls.dict[f.name] = fn_default_value(f.type);
        }
        cls.field_access[f.name] = f.access_level;

    }

    // 4.1 计算 MRO（C3 线性化）
    // 构造类层次映射：类名 -> 直接基类列表
    std::unordered_map<std::string, std::vector<std::string>> hierarchy;
    for (auto& kv : _class_objects) {
        hierarchy[kv.first] = kv.second.bases;
    }
    hierarchy[def.name] = cls.bases;
    // 补充基类的层次（若基类尚未在 _class_objects 中，按空基类处理）
    for (const auto& b : cls.bases) {
        if (hierarchy.count(b) == 0) hierarchy[b] = {};
    }

    try {
        cls.mro = C3Linearizer::compute_mro(def.name, hierarchy);
    } catch (const C3LinearizationError& err) {
        record_error("SEM006", def.loc, err.what(),
            "调整继承层次，避免冲突的多重继承");
        cls.mro = {def.name};  // 降级：仅自身
    }

    _class_objects[def.name] = std::move(cls);
}

// 4.2 实例化类：创建实例对象并调用 __init__
Value Interpreter::instantiate_class(const std::string& cls_name,
                                     std::vector<Value> args,
                                     const SourceRange& loc) {
    auto it = _class_objects.find(cls_name);
    if (it == _class_objects.end()) {
        runtime_error("RUN005", loc, std::string("类 '") + cls_name + "' 未定义");
        return Value::make_null();
    }
    const auto& cls = it->second;

    // 创建实例（用 StructInstance 表示，struct_name 存类名）
    StructInstance si;
    si.struct_name = cls_name;
    
    // 收集类属性名（包括 MRO 中所有类的属性和方法名）
    for (const auto& mro_name : cls.mro) {
        auto mit = _class_objects.find(mro_name);
        if (mit == _class_objects.end()) continue;
        for (const auto& kv : mit->second.dict) {
            si.class_attr_names.push_back(kv.first);
        }
        for (const auto& fa : mit->second.field_access) {
            if (si.field_access.find(fa.first) == si.field_access.end()) {
                si.field_access[fa.first] = fa.second;
            }
        }
    }

    // 4.4 若有 __slots__，预初始化 slots 为 null
    for (const auto& slot : cls.slots) {
        si.fields[slot] = Value::make_null();
    }

    Value instance = Value::make_struct(std::move(si));
    track_memory(value_memory(instance), loc);

    // 调用 __init__ 或 init（若存在）：按 MRO 查找
    Value init_attr = lookup_class_attr(cls_name, "__init__");
    if (init_attr.is_null()) {
        init_attr = lookup_class_attr(cls_name, "init");
    }
    if (!init_attr.is_null()) {
        std::vector<Value> init_args;
        init_args.push_back(instance);
        for (auto& a : args) init_args.push_back(std::move(a));
        std::string init_fn = init_attr.is_fnref() ? init_attr.as_fnref().name : "__init__";
        call_function(init_fn, std::move(init_args), loc);
    }

    return instance;
}

// 4.3 按 MRO 查找类属性
Value Interpreter::lookup_class_attr(const std::string& cls_name,
                                     const std::string& attr_name) const {
    auto it = _class_objects.find(cls_name);
    if (it == _class_objects.end()) return Value::make_null();
    const auto& cls = it->second;

    // 按 MRO 顺序查找
    for (const auto& mro_name : cls.mro) {
        auto mit = _class_objects.find(mro_name);
        if (mit == _class_objects.end()) continue;
        auto dit = mit->second.dict.find(attr_name);
        if (dit != mit->second.dict.end()) {
            return dit->second;
        }
    }
    return Value::make_null();
}

// 4.3 查找实例属性（先实例 dict，再类 MRO）

// 4.3 描述符 __get__ 协议
Value Interpreter::eval_descriptor(const Value& desc,
                                   const Value& instance,
                                   const Value& owner,
                                   const SourceRange& loc) {
    // 描述符必须是类实例且有 __get__ 方法
    if (!desc.is_struct()) return Value::make_null();
    const auto& si = desc.as_struct();
    if (_class_objects.count(si.struct_name) == 0) return Value::make_null();

    Value get_attr = lookup_class_attr(si.struct_name, "__get__");
    if (get_attr.is_null()) return Value::make_null();

    std::vector<Value> get_args;
    get_args.push_back(desc);
    get_args.push_back(instance);
    get_args.push_back(owner);
    std::string fn_name = get_attr.is_fnref() ? get_attr.as_fnref().name : "__get__";
    return call_function(fn_name, std::move(get_args), loc);
}

// 4.3 描述符 __set__ 协议
bool Interpreter::assign_descriptor(const Value& desc,
                                    const Value& instance,
                                    const Value& value,
                                    const SourceRange& loc) {
    if (!desc.is_struct()) return false;
    const auto& si = desc.as_struct();
    if (_class_objects.count(si.struct_name) == 0) return false;

    Value set_attr = lookup_class_attr(si.struct_name, "__set__");
    if (set_attr.is_null()) return false;

    std::vector<Value> set_args;
    set_args.push_back(desc);
    set_args.push_back(instance);
    set_args.push_back(value);
    std::string fn_name = set_attr.is_fnref() ? set_attr.as_fnref().name : "__set__";
    call_function(fn_name, std::move(set_args), loc);
    return true;
}

// 4.4 super()：按 MRO 查找当前类的下一个类的方法
Value Interpreter::eval_super(const std::string& current_class,
                              const Value& instance,
                              const std::string& method_name,
                              std::vector<Value> args,
                              const SourceRange& loc) {
    auto it = _class_objects.find(current_class);
    if (it == _class_objects.end()) {
        runtime_error("RUN005", loc, std::string("super()：类 '") + current_class + "' 未定义");
        return Value::make_null();
    }
    const auto& cls = it->second;

    // 在 MRO 中找到 current_class 的下一个类的方法
    std::string method = method_name.empty() ? "__init__" : method_name;
    bool found_current = false;
    for (const auto& mro_name : cls.mro) {
        if (found_current) {
            // 在 mro_name 中查找 method
            auto mit = _class_objects.find(mro_name);
            if (mit != _class_objects.end()) {
                auto dit = mit->second.dict.find(method);
                if (dit != mit->second.dict.end() && dit->second.is_fnref()) {
                    std::vector<Value> full_args;
                    full_args.push_back(instance);
                    for (auto& a : args) full_args.push_back(std::move(a));
                    return call_function(dit->second.as_fnref().name, std::move(full_args), loc);
                }
            }
        }
        if (mro_name == current_class) found_current = true;
    }
    runtime_error("RUN005", loc,
        std::string("super()：在 '") + current_class + "' 的 MRO 中未找到方法 '" + method + "'");
    return Value::make_null();
}

Value Interpreter::eval_super(SuperExpr& se, const Value& instance) {
    if (_current_class.empty()) {
        runtime_error("RUN005", se.loc, "super() 不在类方法中");
        return Value::make_null();
    }
    std::vector<Value> args;
    for (auto& a : se.args) args.push_back(eval(*a));
    return eval_super(_current_class, instance, se.method_name, std::move(args), se.loc);
}

// 4.4 访问控制：private/protected/public
// 判断当前执行上下文（_current_class）是否可访问 owner_class 的受保护成员
// 规则：当前类的 MRO 中包含 owner_class（本类或其子类）即视为类内
bool Interpreter::is_inside_class_access(const std::string& owner_class) {
    if (_current_class.empty()) return false;
    auto it = _class_objects.find(_current_class);
    if (it == _class_objects.end()) return _current_class == owner_class;
    for (const auto& m : it->second.mro) {
        if (m == owner_class) return true;
    }
    return false;
}

void Interpreter::check_access_control(const std::string& attr_name,
                                       int access_level,
                                       bool is_inside_class,
                                       const SourceRange& loc) {
    // access_level: 0=public/1=protected/2=private
    if (access_level == 2 && !is_inside_class) {
        // private 属性类外访问
        runtime_error("RUN017", loc,
            std::string("private 属性 '") + attr_name + "' 不能在类外访问");
    }
    if (access_level == 1 && !is_inside_class) {
        // protected 属性类外访问：发出警告（按 Python 惯例允许，但告知用户）
        std::cerr << "警告: protected 属性 '" << attr_name << "' 在类外访问" << std::endl;
    }
}

// ===== 模块系统实现（6.1-6.5）=====

// 平台无关文件存在检查
inline bool file_exists_check(const std::string& path) {
#ifdef _WIN32
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    std::ifstream ifs(path);
    return ifs.good();
#endif
}

std::string Interpreter::find_module_file(const std::vector<std::string>& module_path) {
    if (module_path.empty()) return "";
    
    // 构造模块路径字符串
    std::string module_name;
    for (size_t i = 0; i < module_path.size(); ++i) {
        if (i > 0) module_name += "\\";
        module_name += module_path[i];
    }
    
    // 在 _sys_path 中搜索
    for (const auto& dir : _sys_path) {
        // 1. 检查目录作为包（含 __init__.next 或 __init__.next11）
        std::string init_path = dir + "\\" + module_name + "\\__init__.next";
        if (file_exists_check(init_path)) {
            return init_path;
        }
        init_path = dir + "\\" + module_name + "\\__init__.next11";
        if (file_exists_check(init_path)) {
            return init_path;
        }
        
        // 2. 检查 .next 文件
        std::string file_path = dir + "\\" + module_name + ".next";
        if (file_exists_check(file_path)) {
            return file_path;
        }
        
        // 3. 检查 .next11 文件
        file_path = dir + "\\" + module_name + ".next11";
        if (file_exists_check(file_path)) {
            return file_path;
        }
        
        // 4. 在 modules 子目录中搜索（用于 tests/ 目录下的测试）
        std::string modules_path = dir + "\\modules\\" + module_name;
        // 检查包
        init_path = modules_path + "\\__init__.next";
        if (file_exists_check(init_path)) {
            return init_path;
        }
        init_path = modules_path + "\\__init__.next11";
        if (file_exists_check(init_path)) {
            return init_path;
        }
        // 检查文件
        file_path = modules_path + ".next";
        if (file_exists_check(file_path)) {
            return file_path;
        }
        file_path = modules_path + ".next11";
        if (file_exists_check(file_path)) {
            return file_path;
        }
    }
    
    return "";
}

Value Interpreter::load_module(const std::vector<std::string>& module_path, const SourceRange& loc) {
    if (module_path.empty()) {
        runtime_error("SEM007", loc, "空模块路径");
    }
    
    // 构造模块全名
    std::string module_full_name;
    for (size_t i = 0; i < module_path.size(); ++i) {
        if (i > 0) module_full_name += ".";
        module_full_name += module_path[i];
    }
    
    // 检查是否已缓存
    auto it = _sys_modules.find(module_full_name);
    if (it != _sys_modules.end()) {
        return it->second;
    }
    
    // 循环依赖检测
    if (_loading_modules.count(module_full_name)) {
        runtime_error("SEM007", loc,
            std::string("检测到循环依赖：'") + module_full_name + "'",
            "检查模块导入链，消除循环依赖");
        return Value::make_null();
    }
    
    // 查找模块文件
    std::string file_path = find_module_file(module_path);
    if (file_path.empty()) {
        runtime_error("SEM007", loc,
            std::string("找不到模块：'") + module_full_name + "'",
            "检查模块路径是否正确，或文件是否存在");
        return Value::make_null();
    }
    
    // 标记为加载中
    _loading_modules.insert(module_full_name);
    bool module_committed = false;

    // 读取源文件
    std::ifstream ifs(file_path, std::ios::binary);
    if (!ifs.is_open()) {
        _loading_modules.erase(module_full_name);
        runtime_error("SEM007", loc,
            std::string("无法打开模块文件：'") + file_path + "'");
        return Value::make_null();
    }
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    ifs.close();

    // 词法分析
    Lexer lexer;
    auto lex_result = lexer.tokenize(content, file_path);
    if (!lex_result.errors.empty()) {
        _loading_modules.erase(module_full_name);
        for (auto& e : lex_result.errors) {
            runtime_error("SEM007", loc, e.format());
        }
        return Value::make_null();
    }

    // 语法分析
    Parser parser;
    auto parse_result = parser.parse(lex_result.tokens, file_path);
    if (!parse_result.errors.empty()) {
        _loading_modules.erase(module_full_name);
        for (auto& e : parse_result.errors) {
            runtime_error("SEM007", loc, e.format());
        }
        return Value::make_null();
    }

    // 语义分析
    SemanticAnalyzer semant;
    auto semant_result = semant.analyze(std::move(parse_result.ast), file_path);
    if (!semant_result.errors.empty()) {
        _loading_modules.erase(module_full_name);
        for (auto& e : semant_result.errors) {
            runtime_error("SEM007", loc, e.format());
        }
        return Value::make_null();
    }

    // 创建模块对象
    auto module_data = std::make_shared<ModuleData>();
    module_data->name = module_full_name;
    module_data->file = file_path;
    Value module_val = Value::make_module(module_data);

    // 先缓存模块（防止循环依赖）
    _sys_modules[module_full_name] = module_val;

    // 将 AST 保活到 _module_asts，防止 _fn_map 中的 FnDef* 悬垂
    _module_asts[module_full_name] = std::move(semant_result.annotated_ast);
    Program* mod_prog = _module_asts[module_full_name].get();

    // 执行顶层声明
    Environment saved_env = std::move(_env);
    _env = Environment{};

    try {
        // 收集 __all__ 导出
        if (mod_prog && semant_result.symbols) {
            for (auto& d : mod_prog->decls) {
                if (!d) continue;
                if (auto* let = dynamic_cast<LetDecl*>(d.get())) {
                    Value v;
                    if (let->init.has_value() && let->init.value()) {
                        v = eval(*let->init.value());
                    } else {
                        v = fn_default_value(let->declared_type);
                    }
                    declare_var(let->name, v);
                    module_data->dict[let->name] = v;
                } else if (auto* fn = dynamic_cast<FnDef*>(d.get())) {
                    std::string full_fn_name = module_full_name + "." + fn->name;
                    _fn_map[full_fn_name] = fn;
                    if (_fn_map.find(fn->name) == _fn_map.end()) {
                        _fn_map[fn->name] = fn;
                    }
                    module_data->dict[fn->name] = Value::make_fnref(FnRef(full_fn_name));
            } else if (auto* sd = dynamic_cast<StructDef*>(d.get())) {
                // 结构体类型
                module_data->dict[sd->name] = Value::make_string("<struct " + sd->name + ">");
            } else if (auto* ed = dynamic_cast<EnumDef*>(d.get())) {
                // 枚举类型
                module_data->dict[ed->name] = Value::make_string("<enum " + ed->name + ">");
            } else if (auto* ta = dynamic_cast<TypeAlias*>(d.get())) {
                // 类型别名
                module_data->dict[ta->name] = Value::make_string("<typealias " + ta->name + ">");
            } else if (auto* cd = dynamic_cast<ClassDef*>(d.get())) {
                // 类定义
                create_class(*cd);
                auto cls_it = _class_objects.find(cd->name);
                if (cls_it != _class_objects.end()) {
                    module_data->dict[cd->name] = Value::make_class(std::make_shared<ClassObjectData>(cls_it->second));
                }
            } else if (auto* imp = dynamic_cast<ImportStmt*>(d.get())) {
                // 嵌套 import
                exec_stmt(*imp);
            }
        }
        
        // 查找 __all__ 声明
        for (auto& d : mod_prog->decls) {
            if (auto* let = dynamic_cast<LetDecl*>(d.get())) {
                if (let->name == "__all__" && let->init.has_value() && let->init.value()) {
                    Value v = eval(*let->init.value());
                    if (v.is_array()) {
                        for (const auto& item : v.as_array()) {
                            if (item.is_string()) {
                                module_data->all_exports.push_back(item.as_string());
                            }
                        }
                    }
                }
            }
        }
    }
    } catch (...) {
        _sys_modules.erase(module_full_name);
        _loading_modules.erase(module_full_name);
        _env = std::move(saved_env);
        throw;
    }

    // 恢复环境
    _env = std::move(saved_env);

    // 包支持：如果是 __init__ 文件，自动发现子模块
    if (file_path.find("__init__") != std::string::npos) {
        std::string pkg_dir = file_path.substr(0, file_path.find_last_of("/\\"));
#ifdef _WIN32
        WIN32_FIND_DATAA find_data;
        std::string search_pattern = pkg_dir + "\\*.next";
        HANDLE h_find = FindFirstFileA(search_pattern.c_str(), &find_data);
        if (h_find != INVALID_HANDLE_VALUE) {
            do {
                if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::string fname = find_data.cFileName;
                    if (fname != "__init__.next" && fname != "__init__.next11") {
                        std::string submod_name = fname.substr(0, fname.find_last_of('.'));
                        std::vector<std::string> submod_path = module_path;
                        submod_path.push_back(submod_name);
                        Value submod_val = load_module(submod_path, loc);
                        if (submod_val.is_module()) {
                            module_data->dict[submod_name] = submod_val;
                        }
                    }
                }
            } while (FindNextFileA(h_find, &find_data));
            FindClose(h_find);
        }
#else
        DIR* dir = opendir(pkg_dir.c_str());
        if (dir) {
            struct dirent* ent;
            while ((ent = readdir(dir)) != nullptr) {
                std::string fname = ent->d_name;
                if (fname.size() > 5 && fname.substr(fname.size() - 5) == ".next") {
                    if (fname != "__init__.next" && fname != "__init__.next11") {
                        std::string submod_name = fname.substr(0, fname.find_last_of('.'));
                        std::vector<std::string> submod_path = module_path;
                        submod_path.push_back(submod_name);
                        Value submod_val = load_module(submod_path, loc);
                        if (submod_val.is_module()) {
                            module_data->dict[submod_name] = submod_val;
                        }
                    }
                }
            }
            closedir(dir);
        }
#endif
    }
    
    // 标记为已加载
    module_data->loaded = true;
    _loading_modules.erase(module_full_name);
    
    return module_val;
}

void Interpreter::import_all_from_module(Value module_val, const SourceRange& loc) {
    if (!module_val.is_module()) {
        runtime_error("SEM007", loc, "import * 需要模块对象");
        return;
    }
    
    auto& mod = module_val.as_module();
    
    // 如果定义了 __all__，只导入 __all__ 中的名称
    if (!mod.all_exports.empty()) {
        for (const auto& name : mod.all_exports) {
            auto it = mod.dict.find(name);
            if (it != mod.dict.end()) {
                declare_var(name, it->second);
            }
        }
    } else {
        // 否则导入所有非下划线开头的名称
        for (const auto& kv : mod.dict) {
            if (!kv.first.empty() && kv.first[0] != '_') {
                declare_var(kv.first, kv.second);
            }
        }
    }
}

Value Interpreter::resume_generator(GeneratorData& gen, const SourceRange& loc) {
    if (gen.state == GeneratorData::Closed) {
        auto exc = std::make_shared<ExceptionData>();
        exc->type_name = "StopIteration";
        exc->message = "生成器已耗尽";
        throw Next11Exception(Value::make_exception(exc));
    }
    
    FnDef* fn = static_cast<FnDef*>(gen.fn_def);
    if (!fn || !fn->body) {
        gen.state = GeneratorData::Closed;
        auto exc = std::make_shared<ExceptionData>();
        exc->type_name = "StopIteration";
        exc->message = "生成器已耗尽";
        throw Next11Exception(Value::make_exception(exc));
    }
    
    // 重放+计数方案：从头执行函数体，用 yield 计数器跳过已 yield 的值
    struct GenStateGuard {
        Interpreter* self;
        bool prev_active;
        int prev_yield_count, prev_target_count;
        GenStateGuard(Interpreter* s) : self(s), prev_active(s->_gen_active),
            prev_yield_count(s->_gen_yield_count), prev_target_count(s->_gen_target_count) {}
        ~GenStateGuard() {
            self->_gen_active = prev_active;
            self->_gen_yield_count = prev_yield_count;
            self->_gen_target_count = prev_target_count;
        }
    } gen_guard(this);

    _gen_active = true;
    _gen_yield_count = 0;
    _gen_target_count = gen.yield_count;
    
    // 恢复生成器初始环境
    auto* saved_env = static_cast<Environment*>(gen.saved_env);
    Environment prev_env = std::move(_env);
    if (saved_env) {
        _env = Environment::from_global(saved_env->global_scope());
        _env.push_scope();
        // 重新绑定参数
        for (size_t i = 0; i < fn->params.size(); ++i) {
            auto* v = saved_env->lookup(fn->params[i].name);
            if (v) _env.declare(fn->params[i].name, *v);
        }
    }
    
    Value result = Value::make_null();
    
    try {
        auto& body = *fn->body;
        if (body.node_kind == NodeKind::Block) {
            auto& block = static_cast<Block&>(body);
            for (auto& s : block.stmts) {
                if (!s) continue;
                auto sig = exec_stmt(*s);
                if (sig == ExecSignal::Return) {
                    gen.return_value = _return_value;
                    gen.state = GeneratorData::Closed;
                    auto exc = std::make_shared<ExceptionData>();
                    exc->type_name = "StopIteration";
                    exc->message = "生成器已耗尽";
                    throw Next11Exception(Value::make_exception(exc));
                }
            }
        }
        // 执行完所有语句没有遇到目标 yield
        if (gen.state != GeneratorData::Closed) {
            gen.state = GeneratorData::Closed;
        }
        auto exc = std::make_shared<ExceptionData>();
        exc->type_name = "StopIteration";
        exc->message = "生成器已耗尽";
        throw Next11Exception(Value::make_exception(exc));
    } catch (const YieldSignal& ys) {
        gen.yield_count++;
        gen.state = GeneratorData::Suspended;
        gen.current_value = ys.value;
        result = ys.value;
    } catch (const Next11Exception&) {
        gen.state = GeneratorData::Closed;
        _env = std::move(prev_env);
        throw;
    } catch (const std::exception& e) {
        gen.state = GeneratorData::Closed;
        _env = std::move(prev_env);
        auto exc_data = std::make_shared<ExceptionData>();
        exc_data->type_name = "RuntimeError";
        exc_data->message = e.what();
        throw Next11Exception(Value::make_exception(exc_data));
    }

    _env = std::move(prev_env);
    
    return result;
}

} // namespace next11