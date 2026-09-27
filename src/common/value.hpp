// 运行时值模块 - 对齐 spec 6.5 / design 2.3.2.4
// Value 采用 std::variant tagged union 持有运行时值
// 扩展类型：Complex/Dict/Set/FrozenSet/FrozenDict/Class/BoundMethod/Module/
//          Generator/AsyncGenerator/Coroutine/Exception/FileHandle/Bytes
// 循环依赖解决：引用 Value 的结构体在 Value 之后定义，variant 中用 shared_ptr 包装；
//              as_xxx（shared_ptr 类型）和 to_display 在类外定义（结构体完整后）
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <variant>
#include <optional>
#include <sstream>
#include <functional>
#include <cstdio>
#include <cmath>
#include <atomic>

namespace next11 {

// 前置声明
struct Value;
struct FnRef;
struct StructInstance;
struct DictData;
struct SetData;
struct ClassObjectData;
struct BoundMethodData;
struct ModuleData;
struct GeneratorData;
struct AsyncGeneratorData;
struct CoroutineData;
struct ExceptionData;

struct Null {};

struct FnRef {
    std::string name;
    FnRef() = default;
    explicit FnRef(std::string n) : name(std::move(n)) {}
};

struct StructInstance {
    std::string struct_name;
    std::map<std::string, Value> fields;
    std::vector<std::string> class_attr_names; // 类属性名列表（用于 hasattr 检查）
    StructInstance() = default;
};

struct ArrayData {
    std::shared_ptr<std::vector<Value>> elements;
    ArrayData() : elements(std::make_shared<std::vector<Value>>()) {}
    ArrayData(std::vector<Value> v) : elements(std::make_shared<std::vector<Value>>(std::move(v))) {}
};

struct TupleData {
    std::vector<Value> elements;
};

// ===== 不引用 Value 的结构体（在 Value 之前完整定义）=====
struct ComplexData {
    double real = 0.0;
    double imag = 0.0;
    ComplexData() = default;
    ComplexData(double r, double i) : real(r), imag(i) {}
};

struct StackFrame {
    std::string file;
    int line = 0;
    int col = 0;
    std::string fn_name;
    StackFrame() = default;
};

struct FileHandleData {
    std::string path;
    std::string mode;
    std::string encoding;
    int buffer_size = 8192;
    int lock_kind = 0;          // 0=none/1=shared/2=exclusive
    bool is_binary = false;
    void* native_handle = nullptr;  // FILE* 或 fd
    bool is_open = false;
    std::shared_ptr<FILE> _handle_owner;  // manages FILE* lifetime via shared_ptr
    FileHandleData() = default;
    ~FileHandleData() = default;
    FileHandleData(const FileHandleData& o) = default;
    FileHandleData(FileHandleData&& o) noexcept = default;
    FileHandleData& operator=(const FileHandleData& o) = default;
    FileHandleData& operator=(FileHandleData&& o) noexcept = default;
};

struct BytesData {
    std::vector<uint8_t> data;
    BytesData() = default;
    explicit BytesData(std::vector<uint8_t> d) : data(std::move(d)) {}
};

enum class ValueType {
    // 现有 9 种（不重排）
    Int, Float, String, Bool, Array, Struct, FnRef, Tuple, Null,
    // 新增 14 种
    Complex, Dict, Set, FrozenSet, FrozenDict, Class, BoundMethod, Module,
    Generator, AsyncGenerator, Coroutine, Exception, FileHandle, Bytes
};

struct Value {
    ValueType tag = ValueType::Null;
    // variant 追加新成员；FrozenSet/FrozenDict 共用 SetData/DictData（通过 is_frozen 区分）
    std::variant<int64_t, double, std::string, bool,
                 ArrayData, std::shared_ptr<StructInstance>, FnRef, TupleData, Null,
                 ComplexData, FileHandleData, BytesData,
                 std::shared_ptr<DictData>, std::shared_ptr<SetData>,
                 std::shared_ptr<ClassObjectData>, std::shared_ptr<BoundMethodData>,
                 std::shared_ptr<ModuleData>, std::shared_ptr<GeneratorData>,
                 std::shared_ptr<AsyncGeneratorData>, std::shared_ptr<CoroutineData>,
                 std::shared_ptr<ExceptionData>> data{Null{}};

    Value() : tag(ValueType::Null), data(Null{}) {}

    // ===== 原有工厂方法（不修改）=====
    static Value make_int(int64_t v) { Value x; x.tag = ValueType::Int; x.data = v; return x; }
    static Value make_float(double v) { Value x; x.tag = ValueType::Float; x.data = v; return x; }
    static Value make_string(std::string v) { Value x; x.tag = ValueType::String; x.data = std::move(v); return x; }
    static Value make_bool(bool v) { Value x; x.tag = ValueType::Bool; x.data = v; return x; }
    static Value make_array(std::vector<Value> v) { Value x; x.tag = ValueType::Array; x.data = ArrayData(std::move(v)); return x; }
    static Value make_struct(StructInstance s) { Value x; x.tag = ValueType::Struct; x.data = std::make_shared<StructInstance>(std::move(s)); return x; }
    static Value make_fnref(FnRef f) { Value x; x.tag = ValueType::FnRef; x.data = std::move(f); return x; }
    static Value make_tuple(std::vector<Value> v) { Value x; x.tag = ValueType::Tuple; x.data = TupleData{std::move(v)}; return x; }
    static Value make_null() { Value x; x.tag = ValueType::Null; x.data = Null{}; return x; }

    // ===== 新增工厂方法（shared_ptr 移动不需要完整类型）=====
    static Value make_complex(double r, double i) {
        Value x; x.tag = ValueType::Complex; x.data = ComplexData(r, i); return x;
    }
    static Value make_dict(std::shared_ptr<DictData> d) {
        Value x; x.tag = ValueType::Dict; x.data = std::move(d); return x;
    }
    static Value make_set(std::shared_ptr<SetData> s) {
        Value x; x.tag = ValueType::Set; x.data = std::move(s); return x;
    }
    static Value make_frozenset(std::shared_ptr<SetData> s) {
        Value x; x.tag = ValueType::FrozenSet; x.data = std::move(s); return x;
    }
    static Value make_frozendict(std::shared_ptr<DictData> d) {
        Value x; x.tag = ValueType::FrozenDict; x.data = std::move(d); return x;
    }
    static Value make_class(std::shared_ptr<ClassObjectData> c) {
        Value x; x.tag = ValueType::Class; x.data = std::move(c); return x;
    }
    static Value make_bound_method(std::shared_ptr<BoundMethodData> b) {
        Value x; x.tag = ValueType::BoundMethod; x.data = std::move(b); return x;
    }
    static Value make_module(std::shared_ptr<ModuleData> m) {
        Value x; x.tag = ValueType::Module; x.data = std::move(m); return x;
    }
    static Value make_generator(std::shared_ptr<GeneratorData> g) {
        Value x; x.tag = ValueType::Generator; x.data = std::move(g); return x;
    }
    static Value make_async_generator(std::shared_ptr<AsyncGeneratorData> a) {
        Value x; x.tag = ValueType::AsyncGenerator; x.data = std::move(a); return x;
    }
    static Value make_coroutine(std::shared_ptr<CoroutineData> c) {
        Value x; x.tag = ValueType::Coroutine; x.data = std::move(c); return x;
    }
    static Value make_exception(std::shared_ptr<ExceptionData> e) {
        Value x; x.tag = ValueType::Exception; x.data = std::move(e); return x;
    }
    static Value make_file_handle(FileHandleData f) {
        Value x; x.tag = ValueType::FileHandle; x.data = std::move(f); return x;
    }
    static Value make_bytes(BytesData b) {
        Value x; x.tag = ValueType::Bytes; x.data = std::move(b); return x;
    }
    static Value make_bytes(std::vector<uint8_t> b) {
        Value x; x.tag = ValueType::Bytes; x.data = BytesData(std::move(b)); return x;
    }

    // ===== is_xxx（原有）=====
    bool is_int() const { return tag == ValueType::Int; }
    bool is_float() const { return tag == ValueType::Float; }
    bool is_string() const { return tag == ValueType::String; }
    bool is_bool() const { return tag == ValueType::Bool; }
    bool is_array() const { return tag == ValueType::Array; }
    bool is_struct() const { return tag == ValueType::Struct; }
    bool is_fnref() const { return tag == ValueType::FnRef; }
    bool is_tuple() const { return tag == ValueType::Tuple; }
    bool is_null() const { return tag == ValueType::Null; }

    // ===== is_xxx（新增）=====
    bool is_complex() const { return tag == ValueType::Complex; }
    bool is_dict() const { return tag == ValueType::Dict; }
    bool is_set() const { return tag == ValueType::Set; }
    bool is_frozenset() const { return tag == ValueType::FrozenSet; }
    bool is_frozendict() const { return tag == ValueType::FrozenDict; }
    bool is_class() const { return tag == ValueType::Class; }
    bool is_bound_method() const { return tag == ValueType::BoundMethod; }
    bool is_module() const { return tag == ValueType::Module; }
    bool is_generator() const { return tag == ValueType::Generator; }
    bool is_async_generator() const { return tag == ValueType::AsyncGenerator; }
    bool is_coroutine() const { return tag == ValueType::Coroutine; }
    bool is_exception() const { return tag == ValueType::Exception; }
    bool is_file_handle() const { return tag == ValueType::FileHandle; }
    bool is_bytes() const { return tag == ValueType::Bytes; }

    // ===== as_xxx（原有，类型不匹配返回默认值，不抛 bad_variant_access）=====
    int64_t as_int() const { return tag == ValueType::Int ? std::get<int64_t>(data) : 0; }
    double as_float() const { return tag == ValueType::Float ? std::get<double>(data) : 0.0; }
    const std::string& as_string() const {
        static const std::string empty;
        return tag == ValueType::String ? std::get<std::string>(data) : empty;
    }
    bool as_bool() const { return tag == ValueType::Bool && std::get<bool>(data); }
    std::vector<Value>& as_array() {
        static std::vector<Value> empty;
        return tag == ValueType::Array ? *std::get<ArrayData>(data).elements : empty;
    }
    const std::vector<Value>& as_array() const {
        static const std::vector<Value> empty;
        return tag == ValueType::Array ? *std::get<ArrayData>(data).elements : empty;
    }
    StructInstance& as_struct() {
        static StructInstance empty;
        if (tag == ValueType::Struct) {
            auto& sp = std::get<std::shared_ptr<StructInstance>>(data);
            if (sp) return *sp;
        }
        return empty;
    }
    const StructInstance& as_struct() const {
        static const StructInstance empty;
        if (tag == ValueType::Struct) {
            const auto& sp = std::get<std::shared_ptr<StructInstance>>(data);
            if (sp) return *sp;
        }
        return empty;
    }
    FnRef& as_fnref() {
        static FnRef empty;
        return tag == ValueType::FnRef ? std::get<FnRef>(data) : empty;
    }
    const FnRef& as_fnref() const {
        static const FnRef empty;
        return tag == ValueType::FnRef ? std::get<FnRef>(data) : empty;
    }
    std::vector<Value>& as_tuple() {
        static std::vector<Value> empty;
        return tag == ValueType::Tuple ? std::get<TupleData>(data).elements : empty;
    }
    const std::vector<Value>& as_tuple() const {
        static const std::vector<Value> empty;
        return tag == ValueType::Tuple ? std::get<TupleData>(data).elements : empty;
    }

    // ===== as_xxx（新增，值类型 - 在 Value 之前完整定义，可内联）=====
    ComplexData& as_complex() {
        static ComplexData empty;
        return tag == ValueType::Complex ? std::get<ComplexData>(data) : empty;
    }
    const ComplexData& as_complex() const {
        static const ComplexData empty;
        return tag == ValueType::Complex ? std::get<ComplexData>(data) : empty;
    }
    FileHandleData& as_file_handle() {
        static FileHandleData empty;
        return tag == ValueType::FileHandle ? std::get<FileHandleData>(data) : empty;
    }
    const FileHandleData& as_file_handle() const {
        static const FileHandleData empty;
        return tag == ValueType::FileHandle ? std::get<FileHandleData>(data) : empty;
    }
    BytesData& as_bytes() {
        static BytesData empty;
        return tag == ValueType::Bytes ? std::get<BytesData>(data) : empty;
    }
    const BytesData& as_bytes() const {
        static const BytesData empty;
        return tag == ValueType::Bytes ? std::get<BytesData>(data) : empty;
    }

    // ===== as_xxx（新增，shared_ptr 类型 - 声明在类内，定义在类外，需要结构体完整）=====
    DictData& as_dict();
    const DictData& as_dict() const;
    SetData& as_set();
    const SetData& as_set() const;
    ClassObjectData& as_class();
    const ClassObjectData& as_class() const;
    BoundMethodData& as_bound_method();
    const BoundMethodData& as_bound_method() const;
    ModuleData& as_module();
    const ModuleData& as_module() const;
    GeneratorData& as_generator();
    const GeneratorData& as_generator() const;
    AsyncGeneratorData& as_async_generator();
    const AsyncGeneratorData& as_async_generator() const;
    CoroutineData& as_coroutine();
    const CoroutineData& as_coroutine() const;
    ExceptionData& as_exception();
    const ExceptionData& as_exception() const;

    // ===== as_number（更新：ComplexData 返回模）=====
    double as_number() const {
        if (tag == ValueType::Int) return static_cast<double>(std::get<int64_t>(data));
        if (tag == ValueType::Float) return std::get<double>(data);
        if (tag == ValueType::Complex) {
            const auto& c = std::get<ComplexData>(data);
            return std::sqrt(c.real * c.real + c.imag * c.imag);
        }
        return 0.0;
    }

    bool truthy() const;

    // ===== type_name（更新，不引用不完整类型，可内联）=====
    std::string type_name() const {
        switch (tag) {
            case ValueType::Int: return "int";
            case ValueType::Float: return "float";
            case ValueType::String: return "string";
            case ValueType::Bool: return "bool";
            case ValueType::Array: return "array";
            case ValueType::Struct: return "struct";
            case ValueType::FnRef: return "fn";
            case ValueType::Tuple: return "tuple";
            case ValueType::Null: return "null";
            case ValueType::Complex: return "complex";
            case ValueType::Dict: return "dict";
            case ValueType::Set: return "set";
            case ValueType::FrozenSet: return "frozenset";
            case ValueType::FrozenDict: return "frozendict";
            case ValueType::Class: return "class";
            case ValueType::BoundMethod: return "bound_method";
            case ValueType::Module: return "module";
            case ValueType::Generator: return "generator";
            case ValueType::AsyncGenerator: return "async_generator";
            case ValueType::Coroutine: return "coroutine";
            case ValueType::Exception: return "exception";
            case ValueType::FileHandle: return "file";
            case ValueType::Bytes: return "bytes";
        }
        return "unknown";
    }

    // ===== to_display（声明在类内，定义在类外，因处理新 shared_ptr 类型需要结构体完整）=====
    std::string to_display() const;

    std::string struct_name_or() const {
        return is_struct() ? as_struct().struct_name : "";
    }

    // ===== equals（声明在类内，定义在类外，因 Dict/Set 递归比较需要结构体完整）=====
    bool equals(const Value& other) const;
};

// ===== 引用 Value 的结构体（在 Value 之后完整定义）=====
struct DictData {
    std::vector<std::pair<Value, Value>> entries;  // 有序存储（插入顺序）
    bool is_frozen = false;
    // 小字典优化：<=8 键值对用 entries 线性查找，>8 可升级哈希表
    // 暂用 vector 实现，后续功能6再优化
    DictData() = default;
};

struct SetData {
    std::vector<Value> elements;
    bool is_frozen = false;
    SetData() = default;
};

struct ClassObjectData {
    std::string name;
    std::vector<std::string> bases;       // 基类名列表
    std::vector<std::string> mro;         // C3 线性化结果
    std::string metaclass;                // 元类名
    std::vector<std::string> slots;       // __slots__
    std::map<std::string, Value> dict;    // 类属性
    std::map<std::string, std::string> methods;  // 方法名->方法体函数名
    std::map<std::string, int> field_access;     // 字段访问级别 0=public/1=protected/2=private
    std::map<std::string, int> method_access;    // 方法访问级别 0=public/1=protected/2=private
    ClassObjectData() = default;
};

struct BoundMethodData {
    Value instance;       // 绑定的 self/cls
    std::string method_name;
    bool is_classmethod = false;
    BoundMethodData() = default;
};

struct ModuleData {
    std::string name;
    std::string file;
    std::vector<std::string> all_exports;
    std::map<std::string, Value> dict;    // 模块命名空间
    bool loaded = false;
    ModuleData() = default;
};

struct GeneratorData {
    // 生成器状态机
    enum State { Created = 0, Suspended = 1, Running = 2, Closed = 3 };
    
    // 生成器函数体（AST 节点），用 void* 避免循环依赖
    void* fn_def = nullptr;
    
    // 保存的执行环境（局部变量、程序计数器等），用 void* 避免循环依赖
    void* saved_env = nullptr;
    
    // 当前 yield 的值
    Value current_value;
    
    // send() 注入的值
    Value sent_value;
    
    // 状态
    int state = Created;
    
    // 生成器名称
    std::string name;
    
    // 调用栈帧
    std::vector<StackFrame> frame;
    
    // 程序计数器：指向下一个要执行的语句索引
    int pc = 0;
    
    // yield 计数器：已 yield 的次数（用于重放方案）
    int yield_count = 0;
    
    // 是否已完成（return）
    bool completed = false;
    
    // 完成后的返回值
    Value return_value;
    
    // 清理闭包：释放 saved_env 指向的堆内存
    std::function<void()> cleanup;
    
    GeneratorData() = default;
    ~GeneratorData() { if (cleanup) cleanup(); }
    GeneratorData(const GeneratorData& o)
        : fn_def(o.fn_def), saved_env(o.saved_env), current_value(o.current_value),
          sent_value(o.sent_value), state(o.state), name(o.name), frame(o.frame),
          pc(o.pc), yield_count(o.yield_count), completed(o.completed),
          return_value(o.return_value), cleanup(o.cleanup) {
        const_cast<GeneratorData&>(o).saved_env = nullptr;
        const_cast<GeneratorData&>(o).cleanup = nullptr;
    }
    GeneratorData(GeneratorData&& o) noexcept
        : fn_def(o.fn_def), saved_env(o.saved_env), current_value(std::move(o.current_value)),
          sent_value(std::move(o.sent_value)), state(o.state), name(std::move(o.name)),
          frame(std::move(o.frame)), pc(o.pc), yield_count(o.yield_count),
          completed(o.completed), return_value(std::move(o.return_value)),
          cleanup(std::move(o.cleanup)) {
        o.saved_env = nullptr; o.cleanup = nullptr;
    }
    GeneratorData& operator=(const GeneratorData& o) {
        if (this != &o) {
            if (cleanup) cleanup();
            fn_def = o.fn_def; saved_env = o.saved_env; current_value = o.current_value;
            sent_value = o.sent_value; state = o.state; name = o.name; frame = o.frame;
            pc = o.pc; yield_count = o.yield_count; completed = o.completed;
            return_value = o.return_value; cleanup = o.cleanup;
        }
        return *this;
    }
    GeneratorData& operator=(GeneratorData&& o) noexcept {
        if (this != &o) {
            if (cleanup) cleanup();
            fn_def = o.fn_def; saved_env = o.saved_env; current_value = std::move(o.current_value);
            sent_value = std::move(o.sent_value); state = o.state; name = std::move(o.name);
            frame = std::move(o.frame); pc = o.pc; yield_count = o.yield_count;
            completed = o.completed; return_value = std::move(o.return_value);
            cleanup = std::move(o.cleanup); o.saved_env = nullptr; o.cleanup = nullptr;
        }
        return *this;
    }
};

struct AsyncGeneratorData {
    // 异步生成器状态
    enum State { Created = 0, Suspended = 1, Running = 2, Closed = 3 };
    
    // 异步生成器函数体，用 void* 避免循环依赖
    void* fn_def = nullptr;
    
    // 保存的执行环境，用 void* 避免循环依赖
    void* saved_env = nullptr;
    
    // 当前 yield 的值
    Value current_value;
    
    // 状态
    int state = Created;
    
    // 生成器名称
    std::string name;
    
    // 是否已完成
    bool completed = false;
    
    // 清理闭包：释放 saved_env 指向的堆内存
    std::function<void()> cleanup;
    
    AsyncGeneratorData() = default;
    ~AsyncGeneratorData() { if (cleanup) cleanup(); }
    AsyncGeneratorData(const AsyncGeneratorData& o)
        : fn_def(o.fn_def), saved_env(o.saved_env), current_value(o.current_value),
          state(o.state), name(o.name), completed(o.completed), cleanup(o.cleanup) {
        const_cast<AsyncGeneratorData&>(o).saved_env = nullptr;
        const_cast<AsyncGeneratorData&>(o).cleanup = nullptr;
    }
    AsyncGeneratorData(AsyncGeneratorData&& o) noexcept
        : fn_def(o.fn_def), saved_env(o.saved_env), current_value(std::move(o.current_value)),
          state(o.state), name(std::move(o.name)), completed(o.completed),
          cleanup(std::move(o.cleanup)) {
        o.saved_env = nullptr; o.cleanup = nullptr;
    }
    AsyncGeneratorData& operator=(const AsyncGeneratorData& o) {
        if (this != &o) {
            if (cleanup) cleanup();
            fn_def = o.fn_def; saved_env = o.saved_env; current_value = o.current_value;
            state = o.state; name = o.name; completed = o.completed; cleanup = o.cleanup;
        }
        return *this;
    }
    AsyncGeneratorData& operator=(AsyncGeneratorData&& o) noexcept {
        if (this != &o) {
            if (cleanup) cleanup();
            fn_def = o.fn_def; saved_env = o.saved_env; current_value = std::move(o.current_value);
            state = o.state; name = std::move(o.name); completed = o.completed;
            cleanup = std::move(o.cleanup); o.saved_env = nullptr; o.cleanup = nullptr;
        }
        return *this;
    }
};

struct CoroutineData {
    void* handle = nullptr;               // coroutine_handle<AsyncPromise>
    std::atomic<bool> completed = false;
    std::shared_ptr<Value> result;        // 完成后的结果（nullptr 表示未完成）
    CoroutineData() = default;
};

struct ExceptionData {
    std::string type_name;
    std::string message;
    std::vector<StackFrame> stack_trace;
    std::shared_ptr<Value> cause;         // 异常链，nullptr 表示无 cause
    ExceptionData() = default;
};

// ===== Value 类外成员函数定义（此时所有结构体已完整）=====
inline DictData& Value::as_dict() {
    static DictData empty;
    if (tag == ValueType::Dict || tag == ValueType::FrozenDict) {
        auto& p = std::get<std::shared_ptr<DictData>>(data);
        if (p) return *p;
    }
    return empty;
}
inline const DictData& Value::as_dict() const {
    static const DictData empty;
    if (tag == ValueType::Dict || tag == ValueType::FrozenDict) {
        const auto& p = std::get<std::shared_ptr<DictData>>(data);
        if (p) return *p;
    }
    return empty;
}
inline SetData& Value::as_set() {
    static SetData empty;
    if (tag == ValueType::Set || tag == ValueType::FrozenSet) {
        auto& p = std::get<std::shared_ptr<SetData>>(data);
        if (p) return *p;
    }
    return empty;
}
inline const SetData& Value::as_set() const {
    static const SetData empty;
    if (tag == ValueType::Set || tag == ValueType::FrozenSet) {
        const auto& p = std::get<std::shared_ptr<SetData>>(data);
        if (p) return *p;
    }
    return empty;
}
inline ClassObjectData& Value::as_class() {
    static ClassObjectData empty;
    if (tag == ValueType::Class) {
        auto& p = std::get<std::shared_ptr<ClassObjectData>>(data);
        if (p) return *p;
    }
    return empty;
}
inline const ClassObjectData& Value::as_class() const {
    static const ClassObjectData empty;
    if (tag == ValueType::Class) {
        const auto& p = std::get<std::shared_ptr<ClassObjectData>>(data);
        if (p) return *p;
    }
    return empty;
}
inline BoundMethodData& Value::as_bound_method() {
    static BoundMethodData empty;
    if (tag == ValueType::BoundMethod) {
        auto& p = std::get<std::shared_ptr<BoundMethodData>>(data);
        if (p) return *p;
    }
    return empty;
}
inline const BoundMethodData& Value::as_bound_method() const {
    static const BoundMethodData empty;
    if (tag == ValueType::BoundMethod) {
        const auto& p = std::get<std::shared_ptr<BoundMethodData>>(data);
        if (p) return *p;
    }
    return empty;
}
inline ModuleData& Value::as_module() {
    static ModuleData empty;
    if (tag == ValueType::Module) {
        auto& p = std::get<std::shared_ptr<ModuleData>>(data);
        if (p) return *p;
    }
    return empty;
}
inline const ModuleData& Value::as_module() const {
    static const ModuleData empty;
    if (tag == ValueType::Module) {
        const auto& p = std::get<std::shared_ptr<ModuleData>>(data);
        if (p) return *p;
    }
    return empty;
}
inline GeneratorData& Value::as_generator() {
    static GeneratorData empty;
    if (tag == ValueType::Generator) {
        auto& p = std::get<std::shared_ptr<GeneratorData>>(data);
        if (p) return *p;
    }
    return empty;
}
inline const GeneratorData& Value::as_generator() const {
    static const GeneratorData empty;
    if (tag == ValueType::Generator) {
        const auto& p = std::get<std::shared_ptr<GeneratorData>>(data);
        if (p) return *p;
    }
    return empty;
}
inline AsyncGeneratorData& Value::as_async_generator() {
    static AsyncGeneratorData empty;
    if (tag == ValueType::AsyncGenerator) {
        auto& p = std::get<std::shared_ptr<AsyncGeneratorData>>(data);
        if (p) return *p;
    }
    return empty;
}
inline const AsyncGeneratorData& Value::as_async_generator() const {
    static const AsyncGeneratorData empty;
    if (tag == ValueType::AsyncGenerator) {
        const auto& p = std::get<std::shared_ptr<AsyncGeneratorData>>(data);
        if (p) return *p;
    }
    return empty;
}
inline CoroutineData& Value::as_coroutine() {
    static CoroutineData empty;
    if (tag == ValueType::Coroutine) {
        auto& p = std::get<std::shared_ptr<CoroutineData>>(data);
        if (p) return *p;
    }
    return empty;
}
inline const CoroutineData& Value::as_coroutine() const {
    static const CoroutineData empty;
    if (tag == ValueType::Coroutine) {
        const auto& p = std::get<std::shared_ptr<CoroutineData>>(data);
        if (p) return *p;
    }
    return empty;
}
inline ExceptionData& Value::as_exception() {
    static ExceptionData empty;
    if (tag == ValueType::Exception) {
        auto& p = std::get<std::shared_ptr<ExceptionData>>(data);
        if (p) return *p;
    }
    return empty;
}
inline const ExceptionData& Value::as_exception() const {
    static const ExceptionData empty;
    if (tag == ValueType::Exception) {
        const auto& p = std::get<std::shared_ptr<ExceptionData>>(data);
        if (p) return *p;
    }
    return empty;
}

inline std::string Value::to_display() const {
    std::ostringstream oss;
    switch (tag) {
        case ValueType::Int: oss << std::get<int64_t>(data); break;
        case ValueType::Float: oss << std::get<double>(data); break;
        case ValueType::String: oss << std::get<std::string>(data); break;
        case ValueType::Bool: oss << (std::get<bool>(data) ? "true" : "false"); break;
        case ValueType::Array: {
            const auto& arr = as_array();
            oss << "[";
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i) oss << ", ";
                oss << arr[i].to_display();
            }
            oss << "]";
            break;
        }
        case ValueType::Struct: {
            const auto& s = as_struct();
            oss << s.struct_name << "{";
            bool first = true;
            for (const auto& kv : s.fields) {
                if (!first) oss << ", ";
                first = false;
                oss << kv.first << ": " << kv.second.to_display();
            }
            oss << "}";
            break;
        }
        case ValueType::FnRef: oss << "<fn " << as_fnref().name << ">"; break;
        case ValueType::Tuple: {
            const auto& tup = as_tuple();
            oss << "(";
            for (size_t i = 0; i < tup.size(); ++i) {
                if (i) oss << ", ";
                oss << tup[i].to_display();
            }
            if (tup.size() == 1) oss << ",";
            oss << ")";
            break;
        }
        case ValueType::Null: oss << "null"; break;
        case ValueType::Complex: {
            const auto& c = as_complex();
            oss << c.real;
            if (c.imag >= 0) oss << "+";
            oss << c.imag << "j";
            break;
        }
        case ValueType::Dict:
        case ValueType::FrozenDict: {
            const auto& d = as_dict();
            oss << "{";
            bool first = true;
            for (const auto& kv : d.entries) {
                if (!first) oss << ", ";
                first = false;
                oss << kv.first.to_display() << ": " << kv.second.to_display();
            }
            oss << "}";
            break;
        }
        case ValueType::Set:
        case ValueType::FrozenSet: {
            const auto& s = as_set();
            oss << "{";
            bool first = true;
            for (const auto& e : s.elements) {
                if (!first) oss << ", ";
                first = false;
                oss << e.to_display();
            }
            oss << "}";
            break;
        }
        case ValueType::Class: {
            const auto& c = as_class();
            oss << "<class " << c.name << ">";
            break;
        }
        case ValueType::BoundMethod: {
            const auto& b = as_bound_method();
            oss << "<bound_method " << b.method_name << ">";
            break;
        }
        case ValueType::Module: {
            const auto& m = as_module();
            oss << "<module " << m.name << ">";
            break;
        }
        case ValueType::Generator: {
            const auto& g = as_generator();
            oss << "<generator " << g.name << ">";
            break;
        }
        case ValueType::AsyncGenerator: oss << "<async_generator>"; break;
        case ValueType::Coroutine: oss << "<coroutine>"; break;
        case ValueType::Exception: {
            const auto& e = as_exception();
            oss << e.type_name << ": " << e.message;
            break;
        }
        case ValueType::FileHandle: {
            const auto& f = as_file_handle();
            oss << "<filehandle " << f.path << ">";
            break;
        }
        case ValueType::Bytes: {
            const auto& b = as_bytes();
            oss << "b'";
            for (uint8_t c : b.data) {
                if (c >= 32 && c < 127) {
                    oss << static_cast<char>(c);
                } else {
                    oss << "\\x" << std::hex << static_cast<int>(c) << std::dec;
                }
            }
            oss << "'";
            break;
        }
    }
    return oss.str();
}

inline bool Value::truthy() const {
    switch (tag) {
        case ValueType::Null: return false;
        case ValueType::Bool: return std::get<bool>(data);
        case ValueType::Int: return std::get<int64_t>(data) != 0;
        case ValueType::Float: return std::get<double>(data) != 0.0;
        case ValueType::String: return !std::get<std::string>(data).empty();
        case ValueType::Array: return !std::get<ArrayData>(data).elements->empty();
        case ValueType::Tuple: return !std::get<TupleData>(data).elements.empty();
        case ValueType::Complex: {
            const auto& c = as_complex();
            return c.real != 0.0 || c.imag != 0.0;
        }
        case ValueType::Bytes: return !std::get<BytesData>(data).data.empty();
        case ValueType::Dict:
        case ValueType::FrozenDict: return !as_dict().entries.empty();
        case ValueType::Set:
        case ValueType::FrozenSet: return !as_set().elements.empty();
        default: return true;
    }
}

inline bool Value::equals(const Value& other) const {
    if (tag != other.tag) {
        if ((tag == ValueType::Int && other.tag == ValueType::Float) ||
            (tag == ValueType::Float && other.tag == ValueType::Int))
            return as_number() == other.as_number();
        return false;
    }
    switch (tag) {
        case ValueType::Null: return true;
        case ValueType::Int: return as_int() == other.as_int();
        case ValueType::Float: return as_float() == other.as_float();
        case ValueType::String: return as_string() == other.as_string();
        case ValueType::Bool: return as_bool() == other.as_bool();
        case ValueType::FnRef: return as_fnref().name == other.as_fnref().name;
        case ValueType::Complex: {
            const auto& a = as_complex();
            const auto& b = other.as_complex();
            return a.real == b.real && a.imag == b.imag;
        }
        case ValueType::Array: {
            const auto& a1 = as_array();
            const auto& a2 = other.as_array();
            if (a1.size() != a2.size()) return false;
            for (size_t i = 0; i < a1.size(); ++i) {
                if (!a1[i].equals(a2[i])) return false;
            }
            return true;
        }
        case ValueType::Tuple: {
            const auto& t1 = as_tuple();
            const auto& t2 = other.as_tuple();
            if (t1.size() != t2.size()) return false;
            for (size_t i = 0; i < t1.size(); ++i) {
                if (!t1[i].equals(t2[i])) return false;
            }
            return true;
        }
        case ValueType::Struct: {
            const auto& s1 = as_struct();
            const auto& s2 = other.as_struct();
            if (s1.struct_name != s2.struct_name) return false;
            if (s1.fields.size() != s2.fields.size()) return false;
            for (const auto& kv : s1.fields) {
                auto it = s2.fields.find(kv.first);
                if (it == s2.fields.end()) return false;
                if (!kv.second.equals(it->second)) return false;
            }
            return true;
        }
        case ValueType::Bytes: {
            return as_bytes().data == other.as_bytes().data;
        }
        case ValueType::Dict:
        case ValueType::FrozenDict: {
            const auto& d1 = as_dict();
            const auto& d2 = other.as_dict();
            if (d1.entries.size() != d2.entries.size()) return false;
            for (const auto& kv : d1.entries) {
                bool found = false;
                for (const auto& kv2 : d2.entries) {
                    if (kv.first.equals(kv2.first) && kv.second.equals(kv2.second)) {
                        found = true;
                        break;
                    }
                }
                if (!found) return false;
            }
            return true;
        }
        case ValueType::Set:
        case ValueType::FrozenSet: {
            const auto& s1 = as_set();
            const auto& s2 = other.as_set();
            if (s1.elements.size() != s2.elements.size()) return false;
            for (const auto& e : s1.elements) {
                bool found = false;
                for (const auto& e2 : s2.elements) {
                    if (e.equals(e2)) { found = true; break; }
                }
                if (!found) return false;
            }
            return true;
        }
        case ValueType::Class: {
            const auto& p1 = std::get<std::shared_ptr<ClassObjectData>>(data);
            const auto& p2 = std::get<std::shared_ptr<ClassObjectData>>(other.data);
            return p1.get() == p2.get();
        }
        case ValueType::BoundMethod: {
            const auto& p1 = std::get<std::shared_ptr<BoundMethodData>>(data);
            const auto& p2 = std::get<std::shared_ptr<BoundMethodData>>(other.data);
            return p1.get() == p2.get();
        }
        case ValueType::Module: {
            const auto& p1 = std::get<std::shared_ptr<ModuleData>>(data);
            const auto& p2 = std::get<std::shared_ptr<ModuleData>>(other.data);
            return p1.get() == p2.get();
        }
        case ValueType::Generator: {
            const auto& p1 = std::get<std::shared_ptr<GeneratorData>>(data);
            const auto& p2 = std::get<std::shared_ptr<GeneratorData>>(other.data);
            return p1.get() == p2.get();
        }
        case ValueType::AsyncGenerator: {
            const auto& p1 = std::get<std::shared_ptr<AsyncGeneratorData>>(data);
            const auto& p2 = std::get<std::shared_ptr<AsyncGeneratorData>>(other.data);
            return p1.get() == p2.get();
        }
        case ValueType::Coroutine: {
            const auto& p1 = std::get<std::shared_ptr<CoroutineData>>(data);
            const auto& p2 = std::get<std::shared_ptr<CoroutineData>>(other.data);
            return p1.get() == p2.get();
        }
        case ValueType::Exception: {
            const auto& p1 = std::get<std::shared_ptr<ExceptionData>>(data);
            const auto& p2 = std::get<std::shared_ptr<ExceptionData>>(other.data);
            return p1.get() == p2.get();
        }
        case ValueType::FileHandle: {
            const auto& a = as_file_handle();
            const auto& b = other.as_file_handle();
            return a.path == b.path && a.native_handle == b.native_handle;
        }
        default: return false;
    }
}

} // namespace next11
