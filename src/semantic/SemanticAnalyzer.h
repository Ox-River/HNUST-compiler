#pragma once

#include "../ast/AST.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace sysy {

// 符号条目 — 记录一个变量或函数的信息
struct SymbolInfo {
    enum class Kind {
        VARIABLE,
        CONST,
        FUNCTION,
        PARAMETER
    };

    Kind kind;
    BuiltinType type;
    std::string name;
    std::vector<int> arrayDims;     // 数组维度（空 = 非数组）
    std::vector<BuiltinType> paramTypes; // 函数参数类型（仅用于函数）

    bool isArray() const { return !arrayDims.empty(); }
};

// 作用域 — 一个作用域内的符号表
class Scope {
public:
    bool define(const std::string& name, const SymbolInfo& info);
    SymbolInfo* lookup(const std::string& name);
    bool contains(const std::string& name) const;

    const std::unordered_map<std::string, SymbolInfo>& symbols() const { return symbols_; }

private:
    std::unordered_map<std::string, SymbolInfo> symbols_;
};

// 符号表 — 管理嵌套作用域
class SymbolTable {
public:
    SymbolTable();

    // 进入新作用域
    void enterScope();
    // 离开当前作用域
    void leaveScope();
    // 定义符号（在当前作用域）
    bool define(const std::string& name, const SymbolInfo& info);
    // 查找符号（从内到外）
    SymbolInfo* lookup(const std::string& name);
    // 是否在全局作用域
    bool isGlobalScope() const;

private:
    std::vector<Scope> scopes_;
};

// 语义分析器 — 对AST进行语义检查和类型推导
// 检查内容：
//   1. 变量/函数先声明后使用
//   2. 类型匹配（赋值、运算、返回值等）
//   3. break/continue只能在循环内使用
//   4. 函数调用实参与形参匹配
// 同时在Expr节点上标注其类型(exprType)
class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer();

    // 执行语义分析
    bool analyze(CompUnit* unit);

    const std::vector<std::string>& errors() const { return errors_; }

private:
    SymbolTable symtab_;
    std::vector<std::string> errors_;
    int loopDepth_;                         // 当前循环嵌套深度
    BuiltinType currentFuncReturnType_;     // 当前函数的返回类型

    // 类型工具
    static bool isArithmetic(BuiltinType t);
    static bool isNumeric(BuiltinType t);
    // 二元运算类型推导
    static BuiltinType binaryResultType(BuiltinType lhs, BuiltinType rhs, const std::string& op);
    // 是否合法转换（int → float 允许）
    static bool canConvert(BuiltinType from, BuiltinType to);

    // 访问AST节点
    void visitCompUnit(CompUnit* unit);

    // 声明
    void visitVarDecl(VarDecl* decl);
    void visitFuncDef(FuncDef* func);

    // 语句
    void visitStmt(Stmt* stmt);
    void visitBlockStmt(BlockStmt* block);
    void visitAssignStmt(AssignStmt* assign);
    void visitIfStmt(IfStmt* ifStmt);
    void visitWhileStmt(WhileStmt* whileStmt);
    void visitReturnStmt(ReturnStmt* ret);
    void visitExprStmt(ExprStmt* exprStmt);

    // 表达式 — 返回推导出的类型
    BuiltinType visitExpr(Expr* expr);
    BuiltinType visitBinaryExpr(BinaryExpr* expr);
    BuiltinType visitUnaryExpr(UnaryExpr* expr);
    BuiltinType visitCallExpr(CallExpr* expr);
    BuiltinType visitIdentifierExpr(IdentifierExpr* expr);
    BuiltinType visitArrayAccessExpr(ArrayAccessExpr* expr);
};

} // namespace sysy
