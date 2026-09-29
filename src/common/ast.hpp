// AST 节点体系 - 对齐 spec 6.2
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include <cstdint>
#include <unordered_map>
#include "token.hpp"

namespace next11 {

struct SourceRange {
    int line = 0;
    int col = 0;
    int end_line = 0;
    int end_col = 0;
    std::string file;
};

class Type; // 前置声明

enum class NodeKind {
    Program, LetDecl, FnDef, StructDef, EnumDef, TypeAlias,
    ExprStmt, IfStmt, WhileStmt, ForStmt, ReturnStmt, BreakStmt, ContinueStmt,
    Block, BinaryExpr, UnaryExpr, LiteralExpr, IdentExpr, CallExpr,
    PipeExpr, MatchExpr, DestructureExpr, AssignExpr, MemberExpr, IndexExpr,
    ArrayExpr, TupleExpr, StructLitExpr, GroupExpr, TypeRef,
    WithStmt, OptionalChainExpr, GenExpExpr, AwaitExpr, AsyncFnDef, SpreadExpr, SuperExpr, TypeHintExpr,
    ClassDef, TryStmt, ThrowStmt, ImportStmt, YieldExpr, DictExpr, SetExpr, DelStmt
};

class AstNode {
public:
    NodeKind node_kind;
    SourceRange loc;
    std::shared_ptr<Type> type; // 语义分析后填充的类型标注

    explicit AstNode(NodeKind k) : node_kind(k) {}
    virtual ~AstNode() = default;
};

class Stmt : public AstNode {
public:
    explicit Stmt(NodeKind k) : AstNode(k) {}
};
class Expr : public AstNode {
public:
    explicit Expr(NodeKind k) : AstNode(k) {}
};
class Decl : public Stmt {
public:
    explicit Decl(NodeKind k) : Stmt(k) {}
};

struct TypeRef {
    std::string name;
    std::vector<TypeRef> type_args; // 泛型实参
    bool is_array = false;
    std::shared_ptr<TypeRef> elem; // 数组元素类型
    bool is_tuple = false;
    std::vector<TypeRef> tuple_elems; // 元组元素类型
    bool valid = true;
    TypeRef() = default;
    explicit TypeRef(std::string n) : name(std::move(n)) {}
};

struct Param {
    std::string name;
    TypeRef type;
    int access_level = 0; // 0=public/1=protected/2=private（类字段访问级别）
};

struct MatchArm {
    std::unique_ptr<Expr> pattern; // 字面量/标识符/通配符
    std::unique_ptr<Expr> guard;   // 可选守卫
    std::unique_ptr<Expr> result;
    bool is_wildcard = false;
    bool is_var_bind = false;
    std::string var_name; // 变量绑定模式时的变量名
};

struct DestructurePattern {
    bool is_array = false;
    bool is_struct = false;
    bool is_tuple = false;
    std::vector<DestructurePattern> elements;
    std::vector<std::pair<std::string, DestructurePattern>> fields;
    std::string var_name;
    bool is_wildcard = false;
    bool is_leaf = false;
};

class Program : public AstNode {
public:
    std::vector<std::unique_ptr<Decl>> decls;
    Program() : AstNode(NodeKind::Program) {}
};

class LetDecl : public Decl {
public:
    std::string name;
    TypeRef declared_type;
    std::optional<std::unique_ptr<Expr>> init;
    bool is_destructure = false;
    DestructurePattern destr_pattern;
    LetDecl() : Decl(NodeKind::LetDecl) {}
};

class FnDef : public Decl {
public:
    std::string name;
    std::vector<std::string> type_params;
    std::vector<Param> params;
    TypeRef return_type;
    std::unique_ptr<Expr> body; // Block
    bool is_async = false;
    std::vector<TypeRef> type_hints; // 参数类型提示
    std::string doc_string; // 文档字符串
    int access_level = 0; // 0=public/1=protected/2=private
    bool is_classmethod = false;
    bool is_staticmethod = false;
    std::vector<std::pair<std::string, std::vector<std::unique_ptr<Expr>>>> decorators;
    FnDef() : Decl(NodeKind::FnDef) {}
};

class StructDef : public Decl {
public:
    std::string name;
    std::vector<std::string> type_params;
    std::vector<Param> fields;
    StructDef() : Decl(NodeKind::StructDef) {}
};

class EnumDef : public Decl {
public:
    std::string name;
    std::vector<std::string> variants;
    EnumDef() : Decl(NodeKind::EnumDef) {}
};

class TypeAlias : public Decl {
public:
    std::string name;
    std::vector<std::string> type_params;
    TypeRef aliased;
    TypeAlias() : Decl(NodeKind::TypeAlias) {}
};

class Block : public Expr {
public:
    std::vector<std::unique_ptr<Stmt>> stmts;
    Block() : Expr(NodeKind::Block) {}
};

class ExprStmt : public Stmt {
public:
    std::unique_ptr<Expr> expr;
    ExprStmt() : Stmt(NodeKind::ExprStmt) {}
};

class IfStmt : public Stmt {
public:
    std::unique_ptr<Expr> cond;
    std::unique_ptr<Expr> then_block; // Block
    std::optional<std::unique_ptr<Expr>> else_block;
    IfStmt() : Stmt(NodeKind::IfStmt) {}
};

class WhileStmt : public Stmt {
public:
    std::unique_ptr<Expr> cond;
    std::unique_ptr<Expr> body; // Block
    WhileStmt() : Stmt(NodeKind::WhileStmt) {}
};

class ForStmt : public Stmt {
public:
    std::string var_name;
    std::unique_ptr<Expr> iterable;
    std::unique_ptr<Expr> body; // Block
    ForStmt() : Stmt(NodeKind::ForStmt) {}
};

class ReturnStmt : public Stmt {
public:
    std::optional<std::unique_ptr<Expr>> value;
    ReturnStmt() : Stmt(NodeKind::ReturnStmt) {}
};

class BreakStmt : public Stmt {
public:
    BreakStmt() : Stmt(NodeKind::BreakStmt) {}
};

class ContinueStmt : public Stmt {
public:
    ContinueStmt() : Stmt(NodeKind::ContinueStmt) {}
};

class BinaryExpr : public Expr {
public:
    std::string op;
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;
    BinaryExpr() : Expr(NodeKind::BinaryExpr) {}
};

class UnaryExpr : public Expr {
public:
    std::string op;
    std::unique_ptr<Expr> operand;
    UnaryExpr() : Expr(NodeKind::UnaryExpr) {}
};

class LiteralExpr : public Expr {
public:
    enum LitKind { Int, Float, String, Bool, Complex } lit_kind;
    int64_t int_val = 0;
    double float_val = 0.0;
    std::string str_val;
    bool bool_val = false;
    double complex_real = 0.0;
    double complex_imag = 0.0;
    LiteralExpr() : Expr(NodeKind::LiteralExpr) {}
};

class IdentExpr : public Expr {
public:
    std::string name;
    IdentExpr() : Expr(NodeKind::IdentExpr) {}
};

class CallExpr : public Expr {
public:
    std::unique_ptr<Expr> callee;
    std::vector<TypeRef> type_args;
    std::vector<std::unique_ptr<Expr>> args;
    CallExpr() : Expr(NodeKind::CallExpr) {}
};

class PipeExpr : public Expr {
public:
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<CallExpr> call;
    PipeExpr() : Expr(NodeKind::PipeExpr) {}
};

class MatchExpr : public Expr {
public:
    std::unique_ptr<Expr> scrutinee;
    std::vector<MatchArm> arms;
    MatchExpr() : Expr(NodeKind::MatchExpr) {}
};

class AssignExpr : public Expr {
public:
    std::unique_ptr<Expr> target;
    std::unique_ptr<Expr> value;
    AssignExpr() : Expr(NodeKind::AssignExpr) {}
};

class MemberExpr : public Expr {
public:
    std::unique_ptr<Expr> object;
    std::string member;
    MemberExpr() : Expr(NodeKind::MemberExpr) {}
};

class IndexExpr : public Expr {
public:
    std::unique_ptr<Expr> array;
    std::unique_ptr<Expr> index;
    IndexExpr() : Expr(NodeKind::IndexExpr) {}
};

class ArrayExpr : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> elements;
    ArrayExpr() : Expr(NodeKind::ArrayExpr) {}
};

class TupleExpr : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> elements;
    TupleExpr() : Expr(NodeKind::TupleExpr) {}
};

class StructLitExpr : public Expr {
public:
    std::string struct_name;
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> fields;
    StructLitExpr() : Expr(NodeKind::StructLitExpr) {}
};

class DestructureExpr : public Expr {
public:
    DestructurePattern pattern;
    std::unique_ptr<Expr> source;
    DestructureExpr() : Expr(NodeKind::DestructureExpr) {}
};

class GroupExpr : public Expr {
public:
    std::unique_ptr<Expr> inner;
    GroupExpr() : Expr(NodeKind::GroupExpr) {}
};

// ===== 扩展节点：现代语言特性 =====

class WithStmt : public Stmt {
public:
    std::unique_ptr<Expr> context_expr;
    std::string as_name;
    std::unique_ptr<Expr> with_block; // Block
    bool is_async = false;
    WithStmt() : Stmt(NodeKind::WithStmt) {}
};

class OptionalChainExpr : public Expr {
public:
    std::unique_ptr<Expr> object;
    std::string member;
    bool is_call = false;
    std::vector<std::unique_ptr<Expr>> call_args;
    OptionalChainExpr() : Expr(NodeKind::OptionalChainExpr) {}
};

class GenExpExpr : public Expr {
public:
    std::unique_ptr<Expr> element_expr;
    std::string var_name;
    std::unique_ptr<Expr> iterable;
    std::vector<std::unique_ptr<Expr>> conditions; // if 守卫条件列表
    GenExpExpr() : Expr(NodeKind::GenExpExpr) {}
};

class AwaitExpr : public Expr {
public:
    std::unique_ptr<Expr> await_expr;
    AwaitExpr() : Expr(NodeKind::AwaitExpr) {}
};

class AsyncFnDef : public FnDef {
public:
    AsyncFnDef() : FnDef() { is_async = true; node_kind = NodeKind::AsyncFnDef; }
};

class SpreadExpr : public Expr {
public:
    std::unique_ptr<Expr> inner_expr;
    enum SpreadKind { Star, DoubleStar } spread_kind = Star;
    SpreadExpr() : Expr(NodeKind::SpreadExpr) {}
};

class SuperExpr : public Expr {
public:
    std::string method_name;
    std::vector<std::unique_ptr<Expr>> args;
    SuperExpr() : Expr(NodeKind::SuperExpr) {}
};

class TypeHintExpr : public Expr {
public:
    std::unique_ptr<Expr> expr;
    TypeRef hint;
    TypeHintExpr() : Expr(NodeKind::TypeHintExpr) {}
};

// ===== 缺失节点：OOP/异常/模块/生成器/容器 =====

class ClassDef : public Decl {
public:
    std::string name;
    std::string base_class; // 单继承
    std::vector<std::string> base_classes; // 多重继承
    std::vector<Param> fields;
    std::unordered_map<std::string, std::unique_ptr<Expr>> field_inits; // 类属性初始值
    std::vector<std::unique_ptr<FnDef>> methods;
    std::vector<std::pair<std::string, std::vector<std::unique_ptr<Expr>>>> decorators; // name + args
    std::vector<std::string> slots; // __slots__
    std::string metaclass_name; // 元类名
    std::vector<std::string> all_exports; // __all__
    int access_level = 0; // 0=public/1=protected/2=private
    ClassDef() : Decl(NodeKind::ClassDef) {}
};

class TryStmt : public Stmt {
public:
    std::unique_ptr<Expr> try_block; // Block
    std::vector<std::string> catch_types; // catch 多类型
    std::string catch_var;
    std::unique_ptr<Expr> catch_block; // Block
    std::optional<std::unique_ptr<Expr>> finally_block;
    TryStmt() : Stmt(NodeKind::TryStmt) {}
};

class ThrowStmt : public Stmt {
public:
    std::unique_ptr<Expr> expr;
    ThrowStmt() : Stmt(NodeKind::ThrowStmt) {}
};

class ImportStmt : public Decl {
public:
    std::vector<std::string> module_path; // a.b.c
    std::vector<std::string> imported_names;
    std::string alias;
    bool is_from_import = false;
    bool is_wildcard = false;
    bool is_dynamic = false;
    ImportStmt() : Decl(NodeKind::ImportStmt) {}
};

class YieldExpr : public Expr {
public:
    std::optional<std::unique_ptr<Expr>> value;
    YieldExpr() : Expr(NodeKind::YieldExpr) {}
};

class DictExpr : public Expr {
public:
    std::vector<std::pair<std::unique_ptr<Expr>, std::unique_ptr<Expr>>> pairs; // key, value
    std::vector<bool> pair_is_unpack; // ** 解包标记，与 pairs 一一对应
    DictExpr() : Expr(NodeKind::DictExpr) {}
};

class SetExpr : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> elements;
    std::vector<bool> element_is_unpack; // * 解包标记，与 elements 一一对应
    SetExpr() : Expr(NodeKind::SetExpr) {}
};

class DelStmt : public Stmt {
public:
    std::unique_ptr<Expr> target; // IndexExpr (dict[key]) 或 MemberExpr (obj.attr)
    DelStmt() : Stmt(NodeKind::DelStmt) {}
};

} // namespace next11