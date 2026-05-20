#include "AST.h"
#include <sstream>

namespace sysy {

std::string builtinTypeName(BuiltinType t) {
    switch (t) {
        case BuiltinType::INT:   return "int";
        case BuiltinType::FLOAT: return "float";
        case BuiltinType::VOID:  return "void";
        default:                 return "unknown";
    }
}

// ---- Expr节点 ----

std::string IntLiteralExpr::toString(int indent) const {
    return indentStr(indent) + "IntLiteral(" + std::to_string(value) + ")";
}

std::string FloatLiteralExpr::toString(int indent) const {
    return indentStr(indent) + "FloatLiteral(" + std::to_string(value) + ")";
}

std::string IdentifierExpr::toString(int indent) const {
    return indentStr(indent) + "Identifier(" + name + ")";
}

std::string ArrayAccessExpr::toString(int indent) const {
    std::ostringstream oss;
    oss << indentStr(indent) << "ArrayAccess(" << name << ")[";
    for (size_t i = 0; i < indices.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "\n" << indices[i]->toString(indent + 1);
    }
    oss << "\n" << indentStr(indent) << "]";
    return oss.str();
}

std::string BinaryExpr::toString(int indent) const {
    std::ostringstream oss;
    oss << indentStr(indent) << "BinaryExpr(" << op << ")\n"
        << lhs->toString(indent + 1) << "\n"
        << rhs->toString(indent + 1);
    return oss.str();
}

std::string UnaryExpr::toString(int indent) const {
    std::ostringstream oss;
    oss << indentStr(indent) << "UnaryExpr(" << op << ")\n"
        << operand->toString(indent + 1);
    return oss.str();
}

std::string CallExpr::toString(int indent) const {
    std::ostringstream oss;
    oss << indentStr(indent) << "Call(" << funcName << ")";
    if (!arguments.empty()) {
        oss << "\n" << indentStr(indent + 1) << "args:";
        for (auto& arg : arguments) {
            oss << "\n" << arg->toString(indent + 2);
        }
    }
    return oss.str();
}

// ---- Stmt节点 ----

std::string ExprStmt::toString(int indent) const {
    if (expr) {
        return expr->toString(indent);
    }
    return indentStr(indent) + "EmptyStmt";
}

std::string BlockStmt::toString(int indent) const {
    std::ostringstream oss;
    oss << indentStr(indent) << "Block {";
    for (auto& stmt : statements) {
        oss << "\n" << stmt->toString(indent + 1);
    }
    oss << "\n" << indentStr(indent) << "}";
    return oss.str();
}

std::string AssignStmt::toString(int indent) const {
    std::ostringstream oss;
    oss << indentStr(indent) << "Assign(" << lvalueName;
    if (!indices.empty()) {
        for (auto& idx : indices) {
            oss << "\n" << idx->toString(indent + 1);
        }
    }
    oss << ")\n" << indentStr(indent + 1) << "=\n"
        << rhs->toString(indent + 1);
    return oss.str();
}

std::string IfStmt::toString(int indent) const {
    std::ostringstream oss;
    oss << indentStr(indent) << "If\n"
        << indentStr(indent + 1) << "cond:\n"
        << condition->toString(indent + 2) << "\n"
        << indentStr(indent + 1) << "then:\n"
        << thenStmt->toString(indent + 2);
    if (elseStmt) {
        oss << "\n" << indentStr(indent + 1) << "else:\n"
            << elseStmt->toString(indent + 2);
    }
    return oss.str();
}

std::string WhileStmt::toString(int indent) const {
    std::ostringstream oss;
    oss << indentStr(indent) << "While\n"
        << indentStr(indent + 1) << "cond:\n"
        << condition->toString(indent + 2) << "\n"
        << indentStr(indent + 1) << "body:\n"
        << body->toString(indent + 2);
    return oss.str();
}

std::string BreakStmt::toString(int indent) const {
    return indentStr(indent) + "Break";
}

std::string ContinueStmt::toString(int indent) const {
    return indentStr(indent) + "Continue";
}

std::string ReturnStmt::toString(int indent) const {
    if (expr) {
        return indentStr(indent) + "Return\n" + expr->toString(indent + 1);
    }
    return indentStr(indent) + "Return(void)";
}

std::string VarDecl::toString(int indent) const {
    std::ostringstream oss;
    oss << indentStr(indent)
        << (isConst ? "ConstDecl" : "VarDecl")
        << "(" << builtinTypeName(baseType) << ")";
    for (auto& def : definitions) {
        oss << "\n" << indentStr(indent + 1) << "def: " << def.name;
        if (!def.arrayDims.empty()) {
            oss << "[";
            for (size_t i = 0; i < def.arrayDims.size(); ++i) {
                if (i > 0) oss << "][";
                oss << def.arrayDims[i];
            }
            oss << "]";
        }
        if (def.initValue) {
            oss << " =\n" << def.initValue->toString(indent + 2);
        }
    }
    return oss.str();
}

std::string FuncDef::toString(int indent) const {
    std::ostringstream oss;
    oss << indentStr(indent) << "FuncDef "
        << builtinTypeName(returnType) << " " << name << "(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << builtinTypeName(params[i].type) << " " << params[i].name;
        if (params[i].isArray) oss << "[]";
    }
    oss << ")\n" << body->toString(indent + 1);
    return oss.str();
}

std::string CompUnit::toString() const {
    std::ostringstream oss;
    oss << "=== CompUnit ===\n";
    for (auto& decl : globalDecls) {
        oss << decl->toString() << "\n";
    }
    for (auto& func : funcDefs) {
        oss << func->toString() << "\n";
    }
    return oss.str();
}

} // namespace sysy
