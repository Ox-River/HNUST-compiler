#include "SemanticAnalyzer.h"
#include <sstream>

namespace sysy {

// ======== Scope ========

bool Scope::define(const std::string& name, const SymbolInfo& info) {
    if (symbols_.count(name)) {
        return false; // 同作用域内重复定义
    }
    symbols_[name] = info;
    return true;
}

SymbolInfo* Scope::lookup(const std::string& name) {
    auto it = symbols_.find(name);
    if (it != symbols_.end()) return &it->second;
    return nullptr;
}

bool Scope::contains(const std::string& name) const {
    return symbols_.count(name) > 0;
}

// ======== SymbolTable ========

SymbolTable::SymbolTable() {
    enterScope(); // 全局作用域
}

void SymbolTable::enterScope() {
    scopes_.emplace_back();
}

void SymbolTable::leaveScope() {
    if (scopes_.size() > 1) {
        scopes_.pop_back();
    }
}

bool SymbolTable::define(const std::string& name, const SymbolInfo& info) {
    return scopes_.back().define(name, info);
}

SymbolInfo* SymbolTable::lookup(const std::string& name) {
    // 从内层到外层查找
    for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; --i) {
        auto* info = scopes_[i].lookup(name);
        if (info) return info;
    }
    return nullptr;
}

bool SymbolTable::isGlobalScope() const {
    return scopes_.size() == 1;
}

// ======== SemanticAnalyzer ========

SemanticAnalyzer::SemanticAnalyzer()
    : loopDepth_(0), currentFuncReturnType_(BuiltinType::VOID) {}

bool SemanticAnalyzer::analyze(CompUnit* unit) {
    try {
        visitCompUnit(unit);
    } catch (...) {
        errors_.push_back("语义分析过程中发生未知错误");
    }
    return errors_.empty();
}

bool SemanticAnalyzer::isArithmetic(BuiltinType t) {
    return t == BuiltinType::INT || t == BuiltinType::FLOAT;
}

bool SemanticAnalyzer::isNumeric(BuiltinType t) {
    return t == BuiltinType::INT || t == BuiltinType::FLOAT;
}

BuiltinType SemanticAnalyzer::binaryResultType(BuiltinType lhs, BuiltinType rhs,
                                                const std::string& op) {
    // 算术运算：包含float则结果为float
    if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
        if (op == "%") return BuiltinType::INT; // 取模只能是int
        if (lhs == BuiltinType::FLOAT || rhs == BuiltinType::FLOAT)
            return BuiltinType::FLOAT;
        return BuiltinType::INT;
    }
    // 比较和逻辑运算总是返回int（C语言惯例，1表示true，0表示false）
    return BuiltinType::INT;
}

bool SemanticAnalyzer::canConvert(BuiltinType from, BuiltinType to) {
    if (from == to) return true;
    // int可以隐式转为float
    if (from == BuiltinType::INT && to == BuiltinType::FLOAT) return true;
    return false;
}

// ======== 访问函数 ========

void SemanticAnalyzer::visitCompUnit(CompUnit* unit) {
    // 预注册print函数
    SymbolInfo printInfo;
    printInfo.kind = SymbolInfo::Kind::FUNCTION;
    printInfo.type = BuiltinType::INT; // print返回int
    printInfo.name = "print";
    printInfo.paramTypes = {}; // 可变参数，不检查
    symtab_.define("print", printInfo);

    // 第一遍：只收集全局声明和函数签名
    for (auto& decl : unit->globalDecls) {
        visitVarDecl(decl.get());
    }
    for (auto& func : unit->funcDefs) {
        // 注册函数签名
        SymbolInfo funcInfo;
        funcInfo.kind = SymbolInfo::Kind::FUNCTION;
        funcInfo.type = func->returnType;
        funcInfo.name = func->name;
        for (auto& p : func->params) {
            funcInfo.paramTypes.push_back(p.type);
        }
        if (!symtab_.define(func->name, funcInfo)) {
            std::ostringstream oss;
            oss << "函数 '" << func->name << "' 重复定义";
            errors_.push_back(oss.str());
        }
    }

    // 第二遍：分析函数体
    for (auto& func : unit->funcDefs) {
        visitFuncDef(func.get());
    }
}

void SemanticAnalyzer::visitVarDecl(VarDecl* decl) {
    for (auto& def : decl->definitions) {
        SymbolInfo info;
        info.kind = decl->isConst ? SymbolInfo::Kind::CONST : SymbolInfo::Kind::VARIABLE;
        info.type = decl->baseType;
        info.name = def.name;
        info.arrayDims = def.arrayDims;

        if (!symtab_.define(def.name, info)) {
            std::ostringstream oss;
            oss << "符号 '" << def.name << "' 重复定义";
            errors_.push_back(oss.str());
        }

        // 如果有初始值，检查其类型
        if (def.initValue) {
            BuiltinType initType = visitExpr(def.initValue.get());
            if (!canConvert(initType, decl->baseType)) {
                std::ostringstream oss;
                oss << "变量 '" << def.name << "' 初始化类型不匹配: "
                    << builtinTypeName(initType) << " -> "
                    << builtinTypeName(decl->baseType);
                errors_.push_back(oss.str());
            }
        }
    }
}

void SemanticAnalyzer::visitFuncDef(FuncDef* func) {
    currentFuncReturnType_ = func->returnType;
    symtab_.enterScope();

    // 注册形参
    for (auto& param : func->params) {
        SymbolInfo info;
        info.kind = SymbolInfo::Kind::PARAMETER;
        info.type = param.type;
        info.name = param.name;
        info.arrayDims = param.arrayDims;
        if (param.isArray) {
            info.arrayDims = {0}; // 第一维大小未知
            for (auto d : param.arrayDims) info.arrayDims.push_back(d);
        }
        if (!symtab_.define(param.name, info)) {
            std::ostringstream oss;
            oss << "形参 '" << param.name << "' 在函数 '" << func->name << "' 中重复定义";
            errors_.push_back(oss.str());
        }
    }

    visitStmt(func->body.get());
    symtab_.leaveScope();
}

// ======== 语句 ========

void SemanticAnalyzer::visitStmt(Stmt* stmt) {
    if (auto* s = dynamic_cast<BlockStmt*>(stmt)) { visitBlockStmt(s); return; }
    if (auto* s = dynamic_cast<VarDecl*>(stmt)) { visitVarDecl(s); return; }
    if (auto* s = dynamic_cast<AssignStmt*>(stmt)) { visitAssignStmt(s); return; }
    if (auto* s = dynamic_cast<IfStmt*>(stmt)) { visitIfStmt(s); return; }
    if (auto* s = dynamic_cast<WhileStmt*>(stmt)) { visitWhileStmt(s); return; }
    if (auto* s = dynamic_cast<ReturnStmt*>(stmt)) { visitReturnStmt(s); return; }
    if (auto* s = dynamic_cast<ExprStmt*>(stmt)) { visitExprStmt(s); return; }
    if (dynamic_cast<BreakStmt*>(stmt)) {
        if (loopDepth_ == 0) {
            errors_.push_back("break语句只能在循环内部使用");
        }
        return;
    }
    if (dynamic_cast<ContinueStmt*>(stmt)) {
        if (loopDepth_ == 0) {
            errors_.push_back("continue语句只能在循环内部使用");
        }
        return;
    }
}

void SemanticAnalyzer::visitBlockStmt(BlockStmt* block) {
    symtab_.enterScope();
    for (auto& stmt : block->statements) {
        visitStmt(stmt.get());
    }
    symtab_.leaveScope();
}

void SemanticAnalyzer::visitAssignStmt(AssignStmt* assign) {
    // 检查左值是否存在
    SymbolInfo* lhsInfo = symtab_.lookup(assign->lvalueName);
    if (!lhsInfo) {
        std::ostringstream oss;
        oss << "未定义的变量 '" << assign->lvalueName << "'";
        errors_.push_back(oss.str());
        visitExpr(assign->rhs.get()); // 继续检查右值
        return;
    }
    if (lhsInfo->kind == SymbolInfo::Kind::CONST) {
        std::ostringstream oss;
        oss << "不能对常量 '" << assign->lvalueName << "' 赋值";
        errors_.push_back(oss.str());
    }
    if (lhsInfo->kind == SymbolInfo::Kind::FUNCTION) {
        std::ostringstream oss;
        oss << "'" << assign->lvalueName << "' 是函数，不能作为左值";
        errors_.push_back(oss.str());
    }

    // 检查数组访问维度
    if (assign->indices.empty() && lhsInfo->isArray()) {
        std::ostringstream oss;
        oss << "数组 '" << assign->lvalueName << "' 缺少下标";
        errors_.push_back(oss.str());
    }

    // 检查右值类型
    BuiltinType rhsType = visitExpr(assign->rhs.get());
    if (!canConvert(rhsType, lhsInfo->type)) {
        std::ostringstream oss;
        oss << "赋值类型不匹配: " << builtinTypeName(rhsType)
            << " 不能赋值给 " << builtinTypeName(lhsInfo->type);
        errors_.push_back(oss.str());
    }
}

void SemanticAnalyzer::visitIfStmt(IfStmt* ifStmt) {
    visitExpr(ifStmt->condition.get());
    visitStmt(ifStmt->thenStmt.get());
    if (ifStmt->elseStmt) {
        visitStmt(ifStmt->elseStmt.get());
    }
}

void SemanticAnalyzer::visitWhileStmt(WhileStmt* whileStmt) {
    visitExpr(whileStmt->condition.get());
    loopDepth_++;
    visitStmt(whileStmt->body.get());
    loopDepth_--;
}

void SemanticAnalyzer::visitReturnStmt(ReturnStmt* ret) {
    if (ret->expr) {
        BuiltinType retType = visitExpr(ret->expr.get());
        if (!canConvert(retType, currentFuncReturnType_)) {
            std::ostringstream oss;
            oss << "返回值类型不匹配: " << builtinTypeName(retType)
                << " 不能转为 " << builtinTypeName(currentFuncReturnType_);
            errors_.push_back(oss.str());
        }
    } else if (currentFuncReturnType_ != BuiltinType::VOID) {
        std::ostringstream oss;
        oss << "函数声明返回 " << builtinTypeName(currentFuncReturnType_)
            << " 但return语句没有返回值";
        errors_.push_back(oss.str());
    }
}

void SemanticAnalyzer::visitExprStmt(ExprStmt* exprStmt) {
    if (exprStmt->expr) {
        visitExpr(exprStmt->expr.get());
    }
}

// ======== 表达式 ========

BuiltinType SemanticAnalyzer::visitExpr(Expr* expr) {
    if (auto* e = dynamic_cast<IntLiteralExpr*>(expr)) {
        expr->exprType = BuiltinType::INT;
        return BuiltinType::INT;
    }
    if (auto* e = dynamic_cast<FloatLiteralExpr*>(expr)) {
        expr->exprType = BuiltinType::FLOAT;
        return BuiltinType::FLOAT;
    }
    if (auto* e = dynamic_cast<BinaryExpr*>(expr)) {
        BuiltinType t = visitBinaryExpr(e);
        expr->exprType = t;
        return t;
    }
    if (auto* e = dynamic_cast<UnaryExpr*>(expr)) {
        BuiltinType t = visitUnaryExpr(e);
        expr->exprType = t;
        return t;
    }
    if (auto* e = dynamic_cast<CallExpr*>(expr)) {
        BuiltinType t = visitCallExpr(e);
        expr->exprType = t;
        return t;
    }
    if (auto* e = dynamic_cast<IdentifierExpr*>(expr)) {
        BuiltinType t = visitIdentifierExpr(e);
        expr->exprType = t;
        return t;
    }
    if (auto* e = dynamic_cast<ArrayAccessExpr*>(expr)) {
        BuiltinType t = visitArrayAccessExpr(e);
        expr->exprType = t;
        return t;
    }
    expr->exprType = BuiltinType::INT;
    return BuiltinType::INT;
}

BuiltinType SemanticAnalyzer::visitBinaryExpr(BinaryExpr* expr) {
    BuiltinType lhsType = visitExpr(expr->lhs.get());
    BuiltinType rhsType = visitExpr(expr->rhs.get());

    // 检查取模运算的整型限制
    if (expr->op == "%") {
        if (lhsType != BuiltinType::INT || rhsType != BuiltinType::INT) {
            errors_.push_back("取模运算 % 的操作数必须为整数类型");
        }
    }

    return binaryResultType(lhsType, rhsType, expr->op);
}

BuiltinType SemanticAnalyzer::visitUnaryExpr(UnaryExpr* expr) {
    BuiltinType operandType = visitExpr(expr->operand.get());
    if (expr->op == "!") {
        return BuiltinType::INT; // 逻辑非结果为int
    }
    // + / - 保持类型
    if (!isArithmetic(operandType)) {
        std::ostringstream oss;
        oss << "一元运算符 " << expr->op << " 不能应用于非数值类型";
        errors_.push_back(oss.str());
    }
    return operandType;
}

BuiltinType SemanticAnalyzer::visitCallExpr(CallExpr* expr) {
    SymbolInfo* funcInfo = symtab_.lookup(expr->funcName);
    if (!funcInfo) {
        std::ostringstream oss;
        oss << "未定义的函数 '" << expr->funcName << "'";
        errors_.push_back(oss.str());
        // 继续检查参数
        for (auto& arg : expr->arguments) {
            visitExpr(arg.get());
        }
        return BuiltinType::INT;
    }

    if (funcInfo->kind != SymbolInfo::Kind::FUNCTION) {
        std::ostringstream oss;
        oss << "'" << expr->funcName << "' 不是函数";
        errors_.push_back(oss.str());
    }

    // 检查实参与形参匹配（print函数跳过，因为它是可变参数）
    if (expr->funcName != "print" && !funcInfo->paramTypes.empty()) {
        if (expr->arguments.size() != funcInfo->paramTypes.size()) {
            std::ostringstream oss;
            oss << "函数 '" << expr->funcName << "' 参数数量不匹配: 期望 "
                << funcInfo->paramTypes.size() << " 个，但提供了 "
                << expr->arguments.size() << " 个";
            errors_.push_back(oss.str());
        } else {
            for (size_t i = 0; i < expr->arguments.size(); ++i) {
                BuiltinType argType = visitExpr(expr->arguments[i].get());
                if (!canConvert(argType, funcInfo->paramTypes[i])) {
                    std::ostringstream oss;
                    oss << "函数 '" << expr->funcName << "' 第 " << (i + 1)
                        << " 个参数类型不匹配";
                    errors_.push_back(oss.str());
                }
            }
            return funcInfo->type;
        }
    }

    // 检查所有实参
    for (auto& arg : expr->arguments) {
        visitExpr(arg.get());
    }

    return funcInfo->type;
}

BuiltinType SemanticAnalyzer::visitIdentifierExpr(IdentifierExpr* expr) {
    SymbolInfo* info = symtab_.lookup(expr->name);
    if (!info) {
        std::ostringstream oss;
        oss << "未定义的标识符 '" << expr->name << "'";
        errors_.push_back(oss.str());
        return BuiltinType::INT;
    }
    if (info->kind == SymbolInfo::Kind::FUNCTION) {
        std::ostringstream oss;
        oss << "函数 '" << expr->name << "' 不能作为表达式使用";
        errors_.push_back(oss.str());
        return BuiltinType::INT;
    }
    return info->type;
}

BuiltinType SemanticAnalyzer::visitArrayAccessExpr(ArrayAccessExpr* expr) {
    SymbolInfo* info = symtab_.lookup(expr->name);
    if (!info) {
        std::ostringstream oss;
        oss << "未定义的数组 '" << expr->name << "'";
        errors_.push_back(oss.str());
        return BuiltinType::INT;
    }
    if (!info->isArray()) {
        std::ostringstream oss;
        oss << "'" << expr->name << "' 不是数组";
        errors_.push_back(oss.str());
    }

    // 检查下标表达式
    for (auto& idx : expr->indices) {
        BuiltinType idxType = visitExpr(idx.get());
        if (idxType != BuiltinType::INT) {
            errors_.push_back("数组下标必须是整数类型");
        }
    }

    return info->type; // 返回数组元素类型
}

} // namespace sysy
