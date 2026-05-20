#pragma once

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Verifier.h>

#include "../ast/AST.h"
#include "../semantic/SemanticAnalyzer.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <stack>

namespace sysy {

// LLVM IR代码生成器 — 遍历AST生成LLVM IR
// 使用LLVM的IRBuilder API（而非手写IR字符串）
//
// 核心设计：
// - namedValues: 记录当前作用域内变量名→LLVM值的映射（alloca地址）
// - 使用AllocaInst作为变量存储，通过load/store访问
// - 表达式求值返回llvm::Value*
// - 控制流使用基本块和分支指令实现
class IRGenerator {
public:
    IRGenerator();

    // 生成整个编译单元的IR
    // 返回生成的Module，可通过module.print()输出IR文本
    std::unique_ptr<llvm::Module> generate(CompUnit* unit);

    // 获取LLVM上下文（用于外部访问）
    llvm::LLVMContext& context() { return context_; }

    const std::vector<std::string>& errors() const { return errors_; }

private:
    llvm::LLVMContext context_;
    llvm::IRBuilder<> builder_;
    std::unique_ptr<llvm::Module> module_;

    // 当前作用域内的命名值（变量alloca地址）
    // 使用vector<map>实现作用域栈
    std::vector<std::unordered_map<std::string, llvm::AllocaInst*>> namedValues_;

    // 控制流相关
    llvm::Function* currentFunction_ = nullptr;
    llvm::BasicBlock* breakTarget_ = nullptr;     // break跳转目标
    llvm::BasicBlock* continueTarget_ = nullptr;  // continue跳转目标

    // 错误信息
    std::vector<std::string> errors_;

    // ======== 辅助函数 ========

    // 作用域管理
    void enterScope();
    void leaveScope();
    llvm::AllocaInst* lookupVariable(const std::string& name);
    void defineVariable(const std::string& name, llvm::AllocaInst* alloca);

    // 类型转换
    llvm::Type* toLLVMType(BuiltinType t);
    static llvm::Type* toLLVMType(BuiltinType t, llvm::LLVMContext& ctx);

    // 值类型转换（int→float, float→int等）
    llvm::Value* typeConvert(llvm::Value* val, BuiltinType from, BuiltinType to);

    // 在当前函数入口创建alloca
    llvm::AllocaInst* createEntryAlloca(llvm::Function* func,
                                         llvm::Type* type,
                                         const std::string& name);

    // ======== 生成函数 ========

    void generateGlobalVarDecl(VarDecl* decl);
    void generateFuncDef(FuncDef* func);

    // ======== 语句生成 ========

    void generateStmt(Stmt* stmt);
    void generateBlockStmt(BlockStmt* block);
    void generateExprStmt(ExprStmt* stmt);
    void generateAssignStmt(AssignStmt* assign);
    void generateIfStmt(IfStmt* ifStmt);
    void generateWhileStmt(WhileStmt* whileStmt);
    void generateReturnStmt(ReturnStmt* ret);
    void generateVarDeclStmt(VarDecl* decl);

    // ======== 表达式生成（返回llvm::Value*）=======

    llvm::Value* generateExpr(Expr* expr);
    llvm::Value* generateIntLiteral(IntLiteralExpr* expr);
    llvm::Value* generateFloatLiteral(FloatLiteralExpr* expr);
    llvm::Value* generateIdentifier(IdentifierExpr* expr);
    llvm::Value* generateArrayAccess(ArrayAccessExpr* expr);
    llvm::Value* generateBinaryExpr(BinaryExpr* expr);
    llvm::Value* generateUnaryExpr(UnaryExpr* expr);
    llvm::Value* generateCallExpr(CallExpr* expr);

    // 获取数组元素的指针（GEP）
    llvm::Value* getArrayElementPtr(const std::string& name,
                                     const std::vector<ExprPtr>& indices);
};

} // namespace sysy
