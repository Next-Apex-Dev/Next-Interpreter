// 解释执行器接口与实现
#pragma once
#include <memory>
#include <string>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "common/ast.hpp"
#include "common/symbol_table.hpp"
#include "common/value.hpp"
#include "common/diagnostic.hpp"
#include "common/registry.hpp"
#include "common/interfaces.hpp"
#include "environment.hpp"

namespace next11 {

enum class ExecSignal { Normal, Return, Break, Continue };

// 异常传播类型：用 C++ 异常机制包装 Next1.1 异常对象
// 正常路径零开销（不构造），仅 throw 时才创建
struct Next11Exception {
    Value exc_value;  // tag = ValueType::Exception
    explicit Next11Exception(Value v) : exc_value(std::move(v)) {}
};

struct YieldSignal {
    Value value;
    explicit YieldSignal(Value v) : value(std::move(v)) {}
};

class Interpreter : public IInterpreter {
public:
    static constexpr int MAX_RECURSION = 1000;
    static constexpr int64_t MAX_LOOP = 1000000000LL;
    static constexpr int64_t MEMORY_LIMIT = 512LL * 1024 * 1024; // 512MB

    ExecResult execute(Program& ast, SymbolTable& symbols,
                       std::ostream& out, std::ostream& err, std::istream& in,
                       std::string_view filename) override;

private:
    std::ostream* _out;
    std::ostream* _err;
    std::istream* _in;
    std::string _file;
    std::vector<Diagnostic> _errors;
    int _recursion_depth = 0;
    int64_t _loop_count = 0;
    int64_t _memory_used = 0;

    Environment _env;
    std::vector<CallFrame> _call_stack;
    SymbolTable* _symbols;
    std::unordered_map<std::string, FnDef*> _fn_map;
    std::unordered_map<std::string, std::string> _enum_variants; // 变体名 -> 枚举名
    Value _return_value;

    // 1.3 扩展成员：模块/OOP/异常/事件循环/沙箱/后续子系统
    // 模块系统
    std::vector<std::string> _sys_path;                          // 模块搜索路径
    std::unordered_map<std::string, Value> _sys_modules;         // 模块缓存
    std::unordered_set<std::string> _loading_modules;            // 加载中模块（循环依赖检测）
    std::unordered_map<std::string, std::unique_ptr<Program>> _module_asts; // 模块 AST 保活（防止悬垂指针）

    // OOP 系统
    std::unordered_map<std::string, ClassObjectData> _class_objects; // 完整类对象表
    std::string _current_class;                                     // 当前执行中的类名（用于 super）

    // 异常系统

    std::unordered_map<std::string, std::vector<std::string>> _exception_mro; // 异常类 MRO 表

    // 事件循环（EventLoop 在 1.4 定义，用 void* 避免循环依赖）
    void* _event_loop = nullptr;                                 // 事件循环实例

    // 沙箱校验器（SandboxChecker 在功能1定义）
    void* _sandbox = nullptr;                                    // 沙箱校验器

    // 生成器重放计数器
    int _gen_yield_count = 0;    // 当前执行中已遇到的 yield 次数
    int _gen_target_count = 0;   // 目标 yield 次数（达到此次数后停止并返回值）
    bool _gen_active = false;    // 是否正在生成器重放模式下执行


    Value eval(Expr& e);
    ExecSignal exec_stmt(Stmt& s);
    ExecSignal exec_block(Block& b);

    Value eval_binary(BinaryExpr& e);
    Value eval_unary(UnaryExpr& e);
    Value eval_call(CallExpr& e);
    Value eval_match(MatchExpr& e);
    Value eval_pipe(PipeExpr& e);
    Value eval_index(IndexExpr& e);
    Value eval_member(MemberExpr& e);
    Value eval_array(ArrayExpr& e);
    Value eval_dict(DictExpr& e);
    Value eval_set(SetExpr& e);
    Value eval_tuple(TupleExpr& e);
    ExecSignal exec_del(DelStmt& s);

    Value call_function(const std::string& name, std::vector<Value> args, const SourceRange& loc);

    // 模块系统
    Value load_module(const std::vector<std::string>& module_path, const SourceRange& loc);
    std::string find_module_file(const std::vector<std::string>& module_path);
    void import_all_from_module(Value module_val, const SourceRange& loc);

    void declare_var(const std::string& name, Value val);
    Value* lookup_var(const std::string& name);
    void assign_var(const std::string& name, Value val);

    void exec_destructure(const DestructurePattern& pat, const Value& src, const SourceRange& loc);
    bool match_pattern(Expr& pattern, const Value& val, MatchArm& arm);

    // ===== OOP 系统（4.1-4.5）=====
    void create_class(ClassDef& def);                              // 4.2 类创建（构造 ClassObjectData + C3 MRO）
    Value instantiate_class(const std::string& cls_name,           // 4.2 实例化（创建实例 + __init__）
                            std::vector<Value> args,
                            const SourceRange& loc);
    Value lookup_class_attr(const std::string& cls_name,           // 4.3 按 MRO 查找类属性
                            const std::string& attr_name) const;
    Value eval_descriptor(const Value& desc,                       // 4.3 描述符 __get__ 协议
                           const Value& instance,
                           const Value& owner,
                           const SourceRange& loc);
    bool assign_descriptor(const Value& desc,                      // 4.3 描述符 __set__ 协议
                            const Value& instance,
                            const Value& value,
                            const SourceRange& loc);
    Value eval_super(const std::string& current_class,
                     const Value& instance,
                     const std::string& method_name,
                     std::vector<Value> args,
                     const SourceRange& loc);
    Value eval_super(SuperExpr& se, const Value& instance);         // 4.4 super() 重载
    void check_access_control(const std::string& attr_name,        // 4.4 访问控制（private/protected/public）
                              int access_level,
                              bool is_inside_class,
                              const SourceRange& loc);
    bool is_inside_class_access(const std::string& owner_class);   // 4.4 当前方法是否可访问 owner_class 的受保护成员

    // ===== 异常系统（3.1-3.4）=====
    std::vector<StackFrame> capture_stack_trace() const;         // 3.1 遍历 _call_stack 构造栈帧
    void init_exception_classes();                               // 3.1 初始化异常类 MRO
    bool exception_matches(const std::string& exc_type,          // 3.2 基于 MRO 的 catch 类型匹配
                           const std::vector<std::string>& catch_types) const;
    ExecSignal exec_try(TryStmt& s);                             // 3.2 try/catch/finally（零开销）
    ExecSignal exec_with(WithStmt& s);                           // 3.3 with 语句（__enter__/__exit__）


    void runtime_error(std::string code, const SourceRange& loc, std::string desc,
                       std::optional<std::string> fix = std::nullopt,
                       ErrorCategory cat = ErrorCategory::Run);
    void record_error(std::string code, const SourceRange& loc, std::string desc,
                      std::optional<std::string> fix = std::nullopt,
                      ErrorCategory cat = ErrorCategory::Run);
    void track_memory(int64_t bytes, const SourceRange& loc);
    int64_t value_memory(const Value& v) const;

    Value fn_default_value(const TypeRef& tr);
    
    // ===== 生成器系统（9.1-9.5）=====
    Value resume_generator(GeneratorData& gen, const SourceRange& loc);
};

} // namespace next11