#include "IRGenerator.h"
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <sstream>

namespace sysy {

IRGenerator::IRGenerator()
    : builder_(context_) {}

// ======== 作用域管理 ========

void IRGenerator::enterScope() {
    namedValues_.emplace_back();
}

void IRGenerator::leaveScope() {
    if (!namedValues_.empty()) {
        namedValues_.pop_back();
    }
}

llvm::AllocaInst* IRGenerator::lookupVariable(const std::string& name) {
    for (int i = static_cast<int>(namedValues_.size()) - 1; i >= 0; --i) {
        auto it = namedValues_[i].find(name);
        if (it != namedValues_[i].end()) {
            return it->second;
        }
    }
    return nullptr;
}

void IRGenerator::defineVariable(const std::string& name, llvm::AllocaInst* alloca) {
    if (!namedValues_.empty()) {
        namedValues_.back()[name] = alloca;
    }
}

// ======== 类型转换 ========

llvm::Type* IRGenerator::toLLVMType(BuiltinType t) {
    switch (t) {
        case BuiltinType::INT:   return llvm::Type::getInt32Ty(context_);
        case BuiltinType::FLOAT: return llvm::Type::getFloatTy(context_);
        case BuiltinType::VOID:  return llvm::Type::getVoidTy(context_);
        default:                 return llvm::Type::getInt32Ty(context_);
    }
}

llvm::Type* IRGenerator::toLLVMType(BuiltinType t, llvm::LLVMContext& ctx) {
    switch (t) {
        case BuiltinType::INT:   return llvm::Type::getInt32Ty(ctx);
        case BuiltinType::FLOAT: return llvm::Type::getFloatTy(ctx);
        case BuiltinType::VOID:  return llvm::Type::getVoidTy(ctx);
        default:                 return llvm::Type::getInt32Ty(ctx);
    }
}

llvm::Value* IRGenerator::typeConvert(llvm::Value* val, BuiltinType from,
                                       BuiltinType to) {
    if (from == to) return val;

    // int → float: SIToFP
    if (from == BuiltinType::INT && to == BuiltinType::FLOAT) {
        return builder_.CreateSIToFP(val, llvm::Type::getFloatTy(context_));
    }

    // float → int: FPToSI
    if (from == BuiltinType::FLOAT && to == BuiltinType::INT) {
        return builder_.CreateFPToSI(val, llvm::Type::getInt32Ty(context_));
    }

    return val;
}

llvm::AllocaInst* IRGenerator::createEntryAlloca(llvm::Function* func,
                                                   llvm::Type* type,
                                                   const std::string& name) {
    llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(),
                                  func->getEntryBlock().begin());
    return tmpBuilder.CreateAlloca(type, nullptr, name);
}

// ======== 主生成函数 ========

std::unique_ptr<llvm::Module> IRGenerator::generate(CompUnit* unit) {
    module_ = std::make_unique<llvm::Module>("SysY2022", context_);

    // 声明printf函数（print的内部实现依赖它）
    // declare i32 @printf(i8*, ...)
    llvm::FunctionType* printfType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(context_),
        {llvm::Type::getInt8PtrTy(context_)},
        true); // 可变参数
    llvm::Function::Create(printfType, llvm::Function::ExternalLinkage,
                           "printf", module_.get());

    // 生成全局变量声明
    for (auto& decl : unit->globalDecls) {
        generateGlobalVarDecl(decl.get());
    }

    // 生成函数定义
    for (auto& func : unit->funcDefs) {
        generateFuncDef(func.get());
    }

    // 验证IR
    std::string errStr;
    llvm::raw_string_ostream errStream(errStr);
    if (llvm::verifyModule(*module_, &errStream)) {
        errors_.push_back("IR验证失败: " + errStr);
    }

    return std::move(module_);
}

// ======== 全局变量 ========

void IRGenerator::generateGlobalVarDecl(VarDecl* decl) {
    for (auto& def : decl->definitions) {
        llvm::Type* varType;
        if (def.isArray()) {
            // 构建数组类型
            llvm::Type* elemType = toLLVMType(decl->baseType);
            for (auto it = def.arrayDims.rbegin(); it != def.arrayDims.rend(); ++it) {
                elemType = llvm::ArrayType::get(elemType, *it);
            }
            varType = elemType;
        } else {
            varType = toLLVMType(decl->baseType);
        }

        llvm::Constant* initVal = nullptr;
        if (def.initValue) {
            // 简单初始化处理
            if (auto* intLit = dynamic_cast<IntLiteralExpr*>(def.initValue.get())) {
                initVal = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_),
                                                  intLit->value);
            } else {
                initVal = llvm::Constant::getNullValue(varType);
            }
        } else {
            initVal = llvm::Constant::getNullValue(varType);
        }

        auto* globalVar = new llvm::GlobalVariable(
            *module_, varType, decl->isConst,
            llvm::GlobalValue::InternalLinkage,
            initVal, def.name);
        (void)globalVar; // 全局变量通过名称在lookup中查找
    }
}

// ======== 函数定义 ========

void IRGenerator::generateFuncDef(FuncDef* func) {
    // 构建函数类型
    llvm::Type* retType = toLLVMType(func->returnType);
    std::vector<llvm::Type*> paramTypes;
    for (auto& p : func->params) {
        if (p.isArray) {
            // 数组参数作为指针传递
            llvm::Type* elemType = toLLVMType(p.type);
            // 如果有多维，构建完整数组类型然后取指针
            if (!p.arrayDims.empty()) {
                llvm::Type* arrType = elemType;
                for (auto it = p.arrayDims.rbegin(); it != p.arrayDims.rend(); ++it) {
                    arrType = llvm::ArrayType::get(arrType, *it);
                }
                paramTypes.push_back(llvm::PointerType::get(arrType, 0));
            } else {
                paramTypes.push_back(llvm::PointerType::get(elemType, 0));
            }
        } else {
            paramTypes.push_back(toLLVMType(p.type));
        }
    }

    llvm::FunctionType* funcType = llvm::FunctionType::get(retType, paramTypes, false);
    auto* llvmFunc = llvm::Function::Create(
        funcType, llvm::Function::ExternalLinkage, func->name, module_.get());

    // 设置参数名
    size_t idx = 0;
    for (auto& arg : llvmFunc->args()) {
        arg.setName(func->params[idx].name);
        ++idx;
    }

    currentFunction_ = llvmFunc;

    // 创建入口基本块
    auto* entryBB = llvm::BasicBlock::Create(context_, "entry", llvmFunc);
    builder_.SetInsertPoint(entryBB);

    // 进入函数作用域
    enterScope();

    // 为形参创建alloca
    idx = 0;
    for (auto& arg : llvmFunc->args()) {
        llvm::Type* pType = toLLVMType(func->params[idx].type);
        if (func->params[idx].isArray) {
            // 数组参数：直接存储指针
            auto* alloca = createEntryAlloca(llvmFunc, arg.getType(),
                                              func->params[idx].name + ".addr");
            builder_.CreateStore(&arg, alloca);
            defineVariable(func->params[idx].name, alloca);
        } else {
            auto* alloca = createEntryAlloca(llvmFunc, pType,
                                              func->params[idx].name);
            builder_.CreateStore(&arg, alloca);
            defineVariable(func->params[idx].name, alloca);
        }
        ++idx;
    }

    // 生成函数体
    bool hasTerminator = false;
    if (auto* block = dynamic_cast<BlockStmt*>(func->body.get())) {
        for (auto& stmt : block->statements) {
            generateStmt(stmt.get());
            // 检查是否已经生成了终止指令
            if (builder_.GetInsertBlock()->getTerminator()) {
                hasTerminator = true;
                break;
            }
        }
    }

    // 如果函数体没有返回且返回类型为void，添加ret void
    if (!hasTerminator) {
        if (func->returnType == BuiltinType::VOID) {
            builder_.CreateRetVoid();
        } else {
            // 默认返回0
            builder_.CreateRet(llvm::ConstantInt::get(
                llvm::Type::getInt32Ty(context_), 0));
        }
    }

    leaveScope();
    currentFunction_ = nullptr;
    breakTarget_ = nullptr;
    continueTarget_ = nullptr;
}

// ======== 语句生成 ========

void IRGenerator::generateStmt(Stmt* stmt) {
    if (auto* s = dynamic_cast<BlockStmt*>(stmt))      { generateBlockStmt(s); return; }
    if (auto* s = dynamic_cast<ExprStmt*>(stmt))       { generateExprStmt(s); return; }
    if (auto* s = dynamic_cast<AssignStmt*>(stmt))     { generateAssignStmt(s); return; }
    if (auto* s = dynamic_cast<IfStmt*>(stmt))         { generateIfStmt(s); return; }
    if (auto* s = dynamic_cast<WhileStmt*>(stmt))      { generateWhileStmt(s); return; }
    if (auto* s = dynamic_cast<ReturnStmt*>(stmt))     { generateReturnStmt(s); return; }
    if (auto* s = dynamic_cast<VarDecl*>(stmt))        { generateVarDeclStmt(s); return; }
    if (dynamic_cast<BreakStmt*>(stmt)) {
        if (breakTarget_) builder_.CreateBr(breakTarget_);
        return;
    }
    if (dynamic_cast<ContinueStmt*>(stmt)) {
        if (continueTarget_) builder_.CreateBr(continueTarget_);
        return;
    }
}

void IRGenerator::generateBlockStmt(BlockStmt* block) {
    enterScope();
    for (auto& stmt : block->statements) {
        generateStmt(stmt.get());
        // 如果当前基本块已有终止指令，停止继续生成
        if (builder_.GetInsertBlock()->getTerminator()) break;
    }
    leaveScope();
}

void IRGenerator::generateExprStmt(ExprStmt* stmt) {
    if (stmt->expr) {
        generateExpr(stmt->expr.get());
    }
}

void IRGenerator::generateAssignStmt(AssignStmt* assign) {
    // 获取左值指针
    llvm::Value* lhsPtr = nullptr;
    if (assign->indices.empty()) {
        // 普通变量
        lhsPtr = lookupVariable(assign->lvalueName);
    } else {
        // 数组元素
        lhsPtr = getArrayElementPtr(assign->lvalueName, assign->indices);
    }

    if (!lhsPtr) {
        std::ostringstream oss;
        oss << "IR生成错误: 未定义的变量 '" << assign->lvalueName << "'";
        errors_.push_back(oss.str());
        return;
    }

    // 生成右值
    llvm::Value* rhsVal = generateExpr(assign->rhs.get());

    // 类型转换（如果需要）
    llvm::Type* targetType = lhsPtr->getType()->getPointerElementType();
    if (rhsVal->getType() != targetType) {
        if (rhsVal->getType()->isIntegerTy() && targetType->isFloatTy()) {
            rhsVal = builder_.CreateSIToFP(rhsVal, targetType);
        } else if (rhsVal->getType()->isFloatTy() && targetType->isIntegerTy()) {
            rhsVal = builder_.CreateFPToSI(rhsVal, targetType);
        }
    }

    builder_.CreateStore(rhsVal, lhsPtr);
}

void IRGenerator::generateIfStmt(IfStmt* ifStmt) {
    llvm::Value* condVal = generateExpr(ifStmt->condition.get());
    // 条件可能是int或float，需要转为i1
    if (!condVal->getType()->isIntegerTy(1)) {
        if (condVal->getType()->isFloatTy()) {
            condVal = builder_.CreateFCmpONE(condVal,
                llvm::ConstantFP::get(llvm::Type::getFloatTy(context_), 0.0));
        } else {
            condVal = builder_.CreateICmpNE(condVal,
                llvm::ConstantInt::get(condVal->getType(), 0));
        }
    }

    llvm::Function* func = builder_.GetInsertBlock()->getParent();

    auto* thenBB = llvm::BasicBlock::Create(context_, "then", func);
    auto* elseBB = ifStmt->elseStmt
        ? llvm::BasicBlock::Create(context_, "else", func)
        : nullptr;
    auto* mergeBB = llvm::BasicBlock::Create(context_, "if_end", func);

    builder_.CreateCondBr(condVal, thenBB, elseBB ? elseBB : mergeBB);

    // then分支
    builder_.SetInsertPoint(thenBB);
    generateStmt(ifStmt->thenStmt.get());
    if (!builder_.GetInsertBlock()->getTerminator()) {
        builder_.CreateBr(mergeBB);
    }

    // else分支
    if (elseBB) {
        builder_.SetInsertPoint(elseBB);
        generateStmt(ifStmt->elseStmt.get());
        if (!builder_.GetInsertBlock()->getTerminator()) {
            builder_.CreateBr(mergeBB);
        }
    }

    builder_.SetInsertPoint(mergeBB);
}

void IRGenerator::generateWhileStmt(WhileStmt* whileStmt) {
    llvm::Function* func = builder_.GetInsertBlock()->getParent();

    auto* condBB = llvm::BasicBlock::Create(context_, "while_cond", func);
    auto* bodyBB = llvm::BasicBlock::Create(context_, "while_body", func);
    auto* exitBB = llvm::BasicBlock::Create(context_, "while_end", func);

    // 保存旧的break/continue目标
    auto* oldBreak = breakTarget_;
    auto* oldContinue = continueTarget_;
    breakTarget_ = exitBB;
    continueTarget_ = condBB;

    // 跳转到条件检查
    builder_.CreateBr(condBB);

    // 条件检查块
    builder_.SetInsertPoint(condBB);
    llvm::Value* condVal = generateExpr(whileStmt->condition.get());
    if (!condVal->getType()->isIntegerTy(1)) {
        if (condVal->getType()->isFloatTy()) {
            condVal = builder_.CreateFCmpONE(condVal,
                llvm::ConstantFP::get(llvm::Type::getFloatTy(context_), 0.0));
        } else {
            condVal = builder_.CreateICmpNE(condVal,
                llvm::ConstantInt::get(condVal->getType(), 0));
        }
    }
    builder_.CreateCondBr(condVal, bodyBB, exitBB);

    // 循环体
    builder_.SetInsertPoint(bodyBB);
    generateStmt(whileStmt->body.get());
    if (!builder_.GetInsertBlock()->getTerminator()) {
        builder_.CreateBr(condBB); // 循环回到条件检查
    }

    builder_.SetInsertPoint(exitBB);

    // 恢复break/continue目标
    breakTarget_ = oldBreak;
    continueTarget_ = oldContinue;
}

void IRGenerator::generateReturnStmt(ReturnStmt* ret) {
    if (ret->expr) {
        llvm::Value* retVal = generateExpr(ret->expr.get());
        llvm::Type* expectedType = toLLVMType(
            currentFunction_->getReturnType()->isVoidTy()
                ? BuiltinType::VOID
                : (currentFunction_->getReturnType()->isFloatTy()
                    ? BuiltinType::FLOAT : BuiltinType::INT));
        // 类型适配
        if (retVal->getType() != currentFunction_->getReturnType()) {
            if (currentFunction_->getReturnType()->isFloatTy()) {
                retVal = builder_.CreateSIToFP(retVal,
                    llvm::Type::getFloatTy(context_));
            } else if (currentFunction_->getReturnType()->isIntegerTy()) {
                retVal = builder_.CreateFPToSI(retVal,
                    llvm::Type::getInt32Ty(context_));
            }
        }
        builder_.CreateRet(retVal);
    } else {
        builder_.CreateRetVoid();
    }
}

void IRGenerator::generateVarDeclStmt(VarDecl* decl) {
    for (auto& def : decl->definitions) {
        llvm::Type* varType;
        if (def.isArray()) {
            llvm::Type* elemType = toLLVMType(decl->baseType);
            for (auto it = def.arrayDims.rbegin(); it != def.arrayDims.rend(); ++it) {
                elemType = llvm::ArrayType::get(elemType, *it);
            }
            varType = elemType;
        } else {
            varType = toLLVMType(decl->baseType);
        }

        auto* alloca = builder_.CreateAlloca(varType, nullptr, def.name);

        // 如果有初始化值
        if (def.initValue) {
            llvm::Value* initVal = generateExpr(def.initValue.get());
            // 类型转换
            llvm::Type* targetType = alloca->getAllocatedType();
            if (initVal->getType() != targetType) {
                if (targetType->isFloatTy() && initVal->getType()->isIntegerTy()) {
                    initVal = builder_.CreateSIToFP(initVal, targetType);
                } else if (targetType->isIntegerTy() && initVal->getType()->isFloatTy()) {
                    initVal = builder_.CreateFPToSI(initVal, targetType);
                }
            }
            builder_.CreateStore(initVal, alloca);
        }

        defineVariable(def.name, alloca);
    }
}

// ======== 表达式生成 ========

llvm::Value* IRGenerator::generateExpr(Expr* expr) {
    if (auto* e = dynamic_cast<IntLiteralExpr*>(expr))
        return generateIntLiteral(e);
    if (auto* e = dynamic_cast<FloatLiteralExpr*>(expr))
        return generateFloatLiteral(e);
    if (auto* e = dynamic_cast<IdentifierExpr*>(expr))
        return generateIdentifier(e);
    if (auto* e = dynamic_cast<ArrayAccessExpr*>(expr))
        return generateArrayAccess(e);
    if (auto* e = dynamic_cast<BinaryExpr*>(expr))
        return generateBinaryExpr(e);
    if (auto* e = dynamic_cast<UnaryExpr*>(expr))
        return generateUnaryExpr(e);
    if (auto* e = dynamic_cast<CallExpr*>(expr))
        return generateCallExpr(e);

    errors_.push_back("IR生成错误: 未处理的表达式类型");
    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 0);
}

llvm::Value* IRGenerator::generateIntLiteral(IntLiteralExpr* expr) {
    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), expr->value);
}

llvm::Value* IRGenerator::generateFloatLiteral(FloatLiteralExpr* expr) {
    return llvm::ConstantFP::get(llvm::Type::getFloatTy(context_), expr->value);
}

llvm::Value* IRGenerator::generateIdentifier(IdentifierExpr* expr) {
    llvm::AllocaInst* alloca = lookupVariable(expr->name);
    if (!alloca) {
        errors_.push_back("IR生成错误: 未定义的变量 '" + expr->name + "'");
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 0);
    }
    return builder_.CreateLoad(alloca->getAllocatedType(), alloca, expr->name);
}

llvm::Value* IRGenerator::generateArrayAccess(ArrayAccessExpr* expr) {
    llvm::Value* ptr = getArrayElementPtr(expr->name, expr->indices);
    if (!ptr) {
        errors_.push_back("IR生成错误: 无法访问数组 '" + expr->name + "'");
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 0);
    }
    llvm::Type* elemType = ptr->getType()->getPointerElementType();
    return builder_.CreateLoad(elemType, ptr, expr->name + ".load");
}

llvm::Value* IRGenerator::getArrayElementPtr(const std::string& name,
                                               const std::vector<ExprPtr>& indices) {
    llvm::AllocaInst* alloca = lookupVariable(name);
    if (!alloca) {
        errors_.push_back("IR生成错误: 未定义的数组 '" + name + "'");
        return nullptr;
    }

    llvm::Type* elementType = alloca->getAllocatedType();
    std::vector<llvm::Value*> gepIndices;

    // GEP: 第一个索引是0（跳过alloca指针本身）
    gepIndices.push_back(llvm::ConstantInt::get(
        llvm::Type::getInt32Ty(context_), 0));

    for (auto& idxExpr : indices) {
        llvm::Value* idxVal = generateExpr(idxExpr.get());
        if (idxVal->getType()->isFloatTy()) {
            idxVal = builder_.CreateFPToSI(idxVal,
                llvm::Type::getInt32Ty(context_));
        }
        gepIndices.push_back(idxVal);
    }

    return builder_.CreateGEP(elementType, alloca, gepIndices, name + ".gep");
}

llvm::Value* IRGenerator::generateBinaryExpr(BinaryExpr* expr) {
    llvm::Value* lhs = generateExpr(expr->lhs.get());
    llvm::Value* rhs = generateExpr(expr->rhs.get());

    bool lhsFloat = lhs->getType()->isFloatTy();
    bool rhsFloat = rhs->getType()->isFloatTy();

    // 类型统一：如果一个是float，另一个转为float
    if (lhsFloat && !rhsFloat) {
        rhs = builder_.CreateSIToFP(rhs, llvm::Type::getFloatTy(context_));
    } else if (!lhsFloat && rhsFloat) {
        lhs = builder_.CreateSIToFP(lhs, llvm::Type::getFloatTy(context_));
    }

    bool isFloat = lhsFloat || rhsFloat;
    llvm::Type* intType = llvm::Type::getInt32Ty(context_);
    llvm::Type* floatType = llvm::Type::getFloatTy(context_);

    // 算术运算
    if (expr->op == "+") {
        return isFloat ? builder_.CreateFAdd(lhs, rhs) : builder_.CreateAdd(lhs, rhs);
    }
    if (expr->op == "-") {
        return isFloat ? builder_.CreateFSub(lhs, rhs) : builder_.CreateSub(lhs, rhs);
    }
    if (expr->op == "*") {
        return isFloat ? builder_.CreateFMul(lhs, rhs) : builder_.CreateMul(lhs, rhs);
    }
    if (expr->op == "/") {
        return isFloat ? builder_.CreateFDiv(lhs, rhs) : builder_.CreateSDiv(lhs, rhs);
    }
    if (expr->op == "%") {
        return builder_.CreateSRem(lhs, rhs);
    }

    // 比较运算
    if (isFloat) {
        if (expr->op == "==") return builder_.CreateFCmpOEQ(lhs, rhs);
        if (expr->op == "!=") return builder_.CreateFCmpONE(lhs, rhs);
        if (expr->op == "<")  return builder_.CreateFCmpOLT(lhs, rhs);
        if (expr->op == ">")  return builder_.CreateFCmpOGT(lhs, rhs);
        if (expr->op == "<=") return builder_.CreateFCmpOLE(lhs, rhs);
        if (expr->op == ">=") return builder_.CreateFCmpOGE(lhs, rhs);
    } else {
        if (expr->op == "==") return builder_.CreateICmpEQ(lhs, rhs);
        if (expr->op == "!=") return builder_.CreateICmpNE(lhs, rhs);
        if (expr->op == "<")  return builder_.CreateICmpSLT(lhs, rhs);
        if (expr->op == ">")  return builder_.CreateICmpSGT(lhs, rhs);
        if (expr->op == "<=") return builder_.CreateICmpSLE(lhs, rhs);
        if (expr->op == ">=") return builder_.CreateICmpSGE(lhs, rhs);
    }

    // 逻辑运算（&& 和 || 使用短路求值更合适，简化处理先用按位方式）
    if (expr->op == "&&") {
        // 短路求值：lhs && rhs
        return builder_.CreateAnd(
            builder_.CreateICmpNE(lhs, llvm::ConstantInt::get(lhs->getType(), 0)),
            builder_.CreateICmpNE(rhs, llvm::ConstantInt::get(rhs->getType(), 0)));
    }
    if (expr->op == "||") {
        return builder_.CreateOr(
            builder_.CreateICmpNE(lhs, llvm::ConstantInt::get(lhs->getType(), 0)),
            builder_.CreateICmpNE(rhs, llvm::ConstantInt::get(rhs->getType(), 0)));
    }

    errors_.push_back("IR生成错误: 未知的二元运算符 '" + expr->op + "'");
    return llvm::ConstantInt::get(intType, 0);
}

llvm::Value* IRGenerator::generateUnaryExpr(UnaryExpr* expr) {
    llvm::Value* operand = generateExpr(expr->operand.get());

    if (expr->op == "-") {
        if (operand->getType()->isFloatTy()) {
            return builder_.CreateFNeg(operand);
        } else {
            return builder_.CreateNeg(operand);
        }
    }
    if (expr->op == "+") {
        return operand;
    }
    if (expr->op == "!") {
        if (operand->getType()->isFloatTy()) {
            return builder_.CreateFCmpOEQ(operand,
                llvm::ConstantFP::get(llvm::Type::getFloatTy(context_), 0.0));
        } else {
            return builder_.CreateICmpEQ(operand,
                llvm::ConstantInt::get(operand->getType(), 0));
        }
    }

    errors_.push_back("IR生成错误: 未知的一元运算符 '" + expr->op + "'");
    return operand;
}

llvm::Value* IRGenerator::generateCallExpr(CallExpr* expr) {
    // 特殊处理print函数 — 映射到printf
    if (expr->funcName == "print") {
        llvm::Function* printfFunc = module_->getFunction("printf");
        if (!printfFunc) {
            errors_.push_back("IR生成错误: printf函数未声明");
            return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 0);
        }

        std::vector<llvm::Value*> printfArgs;
        // 根据参数类型构建格式串和参数列表
        for (auto& arg : expr->arguments) {
            llvm::Value* argVal = generateExpr(arg.get());

            if (argVal->getType()->isFloatTy()) {
                // float提升为double用于printf
                argVal = builder_.CreateFPExt(argVal,
                    llvm::Type::getDoubleTy(context_));
                printfArgs.push_back(
                    builder_.CreateGlobalStringPtr("%f\n"));
            } else if (argVal->getType()->isIntegerTy()) {
                printfArgs.push_back(
                    builder_.CreateGlobalStringPtr("%d\n"));
            } else {
                printfArgs.push_back(
                    builder_.CreateGlobalStringPtr("%d\n"));
            }
            printfArgs.push_back(argVal);
        }

        // 如果没有参数，输出空行
        if (expr->arguments.empty()) {
            printfArgs.push_back(
                builder_.CreateGlobalStringPtr("\n"));
        }

        return builder_.CreateCall(printfFunc, printfArgs);
    }

    // 普通函数调用
    llvm::Function* callee = module_->getFunction(expr->funcName);
    if (!callee) {
        errors_.push_back("IR生成错误: 未定义的函数 '" + expr->funcName + "'");
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), 0);
    }

    std::vector<llvm::Value*> args;
    for (auto& arg : expr->arguments) {
        args.push_back(generateExpr(arg.get()));
    }

    // 调整实参类型以匹配形参（如需要）
    for (size_t i = 0; i < args.size() && i < callee->arg_size(); ++i) {
        llvm::Type* expected = callee->getArg(i)->getType();
        if (args[i]->getType() != expected) {
            if (expected->isFloatTy() && args[i]->getType()->isIntegerTy()) {
                args[i] = builder_.CreateSIToFP(args[i], expected);
            } else if (expected->isIntegerTy() && args[i]->getType()->isFloatTy()) {
                args[i] = builder_.CreateFPToSI(args[i], expected);
            }
        }
    }

    return builder_.CreateCall(callee, args);
}

} // namespace sysy
