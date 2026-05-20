#pragma once

#include <memory>
#include <string>
#include <vector>
#include <variant>

namespace sysy {

// ============================================================
// 前向声明
// ============================================================
class Expr;
class Stmt;
class CompUnit;

// 常用指针类型别名
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

// SysY类型系统
enum class BuiltinType {
    INT,
    FLOAT,
    VOID
};

std::string builtinTypeName(BuiltinType t);

// ============================================================
// 表达式基类
// ============================================================
class Expr {
public:
    virtual ~Expr() = default;

    // 用于类型检查后记录表达式的类型
    BuiltinType exprType = BuiltinType::INT;

    // 调试用：转为字符串表示
    virtual std::string toString(int indent = 0) const = 0;

protected:
    static std::string indentStr(int n) { return std::string(n * 2, ' '); }
};

// ============================================================
// 具体表达式节点
// ============================================================

// 整数字面量：42
class IntLiteralExpr : public Expr {
public:
    int value;
    explicit IntLiteralExpr(int v) : value(v) { exprType = BuiltinType::INT; }
    std::string toString(int indent = 0) const override;
};

// 浮点数字面量：3.14
class FloatLiteralExpr : public Expr {
public:
    float value;
    explicit FloatLiteralExpr(float v) : value(v) { exprType = BuiltinType::FLOAT; }
    std::string toString(int indent = 0) const override;
};

// 标识符引用：a, foo
class IdentifierExpr : public Expr {
public:
    std::string name;
    explicit IdentifierExpr(std::string n) : name(std::move(n)) {}
    std::string toString(int indent = 0) const override;
};

// 数组下标访问：arr[0], mat[i][j]
class ArrayAccessExpr : public Expr {
public:
    std::string name;           // 数组名
    std::vector<ExprPtr> indices; // 各维下标
    ArrayAccessExpr(std::string n, std::vector<ExprPtr> idxs)
        : name(std::move(n)), indices(std::move(idxs)) {}
    std::string toString(int indent = 0) const override;
};

// 二元运算表达式：a + b, x * y
class BinaryExpr : public Expr {
public:
    std::string op;     // 运算符字符串，如 "+", "-", "*", "/", "%", "&&", "||", etc.
    ExprPtr lhs;
    ExprPtr rhs;
    BinaryExpr(std::string o, ExprPtr l, ExprPtr r)
        : op(std::move(o)), lhs(std::move(l)), rhs(std::move(r)) {}
    std::string toString(int indent = 0) const override;
};

// 一元运算表达式：-a, !flag, +x
class UnaryExpr : public Expr {
public:
    std::string op;     // 运算符："-", "!", "+"
    ExprPtr operand;
    UnaryExpr(std::string o, ExprPtr opnd)
        : op(std::move(o)), operand(std::move(opnd)) {}
    std::string toString(int indent = 0) const override;
};

// 函数调用表达式：foo(1, 2), print(x)
class CallExpr : public Expr {
public:
    std::string funcName;
    std::vector<ExprPtr> arguments;
    CallExpr(std::string name, std::vector<ExprPtr> args)
        : funcName(std::move(name)), arguments(std::move(args)) {}
    std::string toString(int indent = 0) const override;
};

// ============================================================
// 语句基类
// ============================================================
class Stmt {
public:
    virtual ~Stmt() = default;
    virtual std::string toString(int indent = 0) const = 0;

protected:
    static std::string indentStr(int n) { return std::string(n * 2, ' '); }
};

// ============================================================
// 具体语句节点
// ============================================================

// 表达式语句：expr;
class ExprStmt : public Stmt {
public:
    ExprPtr expr; // 可为空（单独的分号）
    explicit ExprStmt(ExprPtr e) : expr(std::move(e)) {}
    std::string toString(int indent = 0) const override;
};

// 语句块：{ stmt1; stmt2; ... }
class BlockStmt : public Stmt {
public:
    std::vector<StmtPtr> statements;
    explicit BlockStmt(std::vector<StmtPtr> stmts)
        : statements(std::move(stmts)) {}
    std::string toString(int indent = 0) const override;
};

// 赋值语句：lvalue = expr;
class AssignStmt : public Stmt {
public:
    // lvalue 是标识符或数组访问
    std::string lvalueName;
    std::vector<ExprPtr> indices; // 空表示普通变量，非空表示数组元素
    ExprPtr rhs;

    AssignStmt(std::string name, std::vector<ExprPtr> idxs, ExprPtr r)
        : lvalueName(std::move(name)), indices(std::move(idxs)), rhs(std::move(r)) {}
    std::string toString(int indent = 0) const override;
};

// if语句：if (cond) then_stmt [else else_stmt]
class IfStmt : public Stmt {
public:
    ExprPtr condition;
    StmtPtr thenStmt;
    StmtPtr elseStmt; // 可为空
    IfStmt(ExprPtr cond, StmtPtr then_s, StmtPtr else_s = nullptr)
        : condition(std::move(cond)), thenStmt(std::move(then_s)),
          elseStmt(std::move(else_s)) {}
    std::string toString(int indent = 0) const override;
};

// while语句：while (cond) body
class WhileStmt : public Stmt {
public:
    ExprPtr condition;
    StmtPtr body;
    WhileStmt(ExprPtr cond, StmtPtr b)
        : condition(std::move(cond)), body(std::move(b)) {}
    std::string toString(int indent = 0) const override;
};

// break语句
class BreakStmt : public Stmt {
public:
    std::string toString(int indent = 0) const override;
};

// continue语句
class ContinueStmt : public Stmt {
public:
    std::string toString(int indent = 0) const override;
};

// return语句：return [expr];
class ReturnStmt : public Stmt {
public:
    ExprPtr expr; // 可为空（void函数）
    explicit ReturnStmt(ExprPtr e = nullptr) : expr(std::move(e)) {}
    std::string toString(int indent = 0) const override;
};

// ============================================================
// 声明 / 顶层节点
// ============================================================

// 单个变量/常量定义
struct VarDef {
    std::string name;
    std::vector<int> arrayDims; // 数组各维大小；空表示普通变量
    ExprPtr initValue;          // 初始值（可为空）

    bool isArray() const { return !arrayDims.empty(); }
};

// 变量/常量声明（可包含多个VarDef）
class VarDecl : public Stmt {
public:
    BuiltinType baseType;       // int 或 float
    bool isConst;               // 是否为const声明
    std::vector<VarDef> definitions;
    VarDecl(BuiltinType bt, bool c, std::vector<VarDef> defs)
        : baseType(bt), isConst(c), definitions(std::move(defs)) {}
    std::string toString(int indent = 0) const override;
};

// 函数形参定义
struct FuncParam {
    std::string name;
    BuiltinType type;
    bool isArray;               // 是否为数组参数（如 int arr[]）
    std::vector<int> arrayDims; // 数组参数的后续维度（第一维为空表示为[]）
};

// 函数定义
class FuncDef : public Stmt {
public:
    BuiltinType returnType;     // 返回类型（int/float/void）
    std::string name;           // 函数名
    std::vector<FuncParam> params; // 形参列表
    StmtPtr body;               // 函数体（通常是BlockStmt）
    FuncDef(BuiltinType rt, std::string n, std::vector<FuncParam> ps, StmtPtr b)
        : returnType(rt), name(std::move(n)), params(std::move(ps)), body(std::move(b)) {}
    std::string toString(int indent = 0) const override;
};

// ============================================================
// 编译单元（整个程序的根节点）
// ============================================================
class CompUnit {
public:
    // 全局变量声明
    std::vector<std::unique_ptr<VarDecl>> globalDecls;
    // 函数定义
    std::vector<std::unique_ptr<FuncDef>> funcDefs;

    std::string toString() const;
};

} // namespace sysy
