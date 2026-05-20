#pragma once

#include "Token.h"
#include "AST.h"
#include "../lexer/Token.h"
#include <memory>
#include <vector>

namespace sysy {

// 语法分析器 — 递归下降法，将Token序列解析为AST
// SysY2022语法规范：
//   CompUnit      → (Decl | FuncDef)*
//   Decl          → ConstDecl | VarDecl
//   BType         → 'int' | 'float'
//   ConstDecl     → 'const' BType ConstDef (',' ConstDef)* ';'
//   ConstDef      → Ident ( '[' ConstExp ']' )* '=' ConstInitVal
//   VarDecl       → BType VarDef (',' VarDef)* ';'
//   VarDef        → Ident ( '[' ConstExp ']' )* ( '=' InitVal )?
//   FuncDef       → FuncType Ident '(' [FuncFParams] ')' Block
//   FuncType      → 'int' | 'float' | 'void'
//   FuncFParams   → FuncFParam (',' FuncFParam)*
//   FuncFParam    → BType Ident ( '[' ']' ( '[' Exp ']' )* )?
//   Block         → '{' {BlockItem} '}'
//   BlockItem     → Decl | Stmt
//   Stmt          → LVal '=' Exp ';'                    // 赋值
//                 | [Exp] ';'                           // 表达式语句(可为空)
//                 | Block                                // 语句块
//                 | 'if' '(' Cond ')' Stmt ['else' Stmt] // if语句
//                 | 'while' '(' Cond ')' Stmt            // while循环
//                 | 'break' ';'                          // break
//                 | 'continue' ';'                       // continue
//                 | 'return' [Exp] ';'                   // return
//   Exp           → AddExp
//   ConstExp      → AddExp (编译期可求值的表达式)
//   Cond          → LOrExp
//   LVal          → Ident ('[' Exp ']')*
//   PrimaryExp    → '(' Exp ')' | LVal | Number | Ident '(' [FuncRParams] ')'
//   FuncRParams   → Exp (',' Exp)*
//   UnaryExp      → PrimaryExp | UnaryOp UnaryExp
//   UnaryOp       → '+' | '-' | '!'
//   MulExp        → UnaryExp (('*' | '/' | '%') UnaryExp)*
//   AddExp        → MulExp (('+' | '-') MulExp)*
//   RelExp        → AddExp (('<' | '>' | '<=' | '>=') AddExp)*
//   EqExp         → RelExp (('==' | '!=') RelExp)*
//   LAndExp       → EqExp ('&&' EqExp)*
//   LOrExp        → LAndExp ('||' LAndExp)*
class Parser {
public:
    Parser(std::vector<Token> tokens);

    // 解析入口 — 返回整个编译单元
    std::unique_ptr<CompUnit> parse();

    // 错误信息
    const std::vector<std::string>& errors() const { return errors_; }

private:
    std::vector<Token> tokens_;
    size_t pos_;
    std::vector<std::string> errors_;

    // ======== 辅助函数 ========
    const Token& peek() const;          // 查看当前Token
    const Token& peekPrev() const;      // 查看前一个Token
    Token advance();                    // 消费当前Token并返回
    bool check(TokenType type) const;   // 检查当前Token类型
    bool match(TokenType type);         // 如果匹配则消费，返回是否匹配
    Token expect(TokenType type);      // 期望并消费指定类型，否则添加错误
    void addError(const std::string& msg, const Token& tok);

    // ======== 解析函数 — 按优先级从低到高 ========
    std::unique_ptr<CompUnit> parseCompUnit();

    // 声明
    bool isDeclStart() const;           // 判断是否是声明开始 (int/float/const)
    StmtPtr parseDecl();
    StmtPtr parseConstDecl();
    StmtPtr parseVarDecl();
    BuiltinType parseBType();

    // 函数
    bool isFuncDefStart() const;        // 判断是否是函数定义开始
    StmtPtr parseFuncDef();
    BuiltinType parseFuncType();
    std::vector<FuncParam> parseFuncFParams();
    FuncParam parseFuncFParam();

    // 语句块
    StmtPtr parseBlock();
    StmtPtr parseBlockItem();
    StmtPtr parseStmt();

    // 表达式（按优先级）
    ExprPtr parseExp();                 // Exp → AddExp (入口，保留扩展点)
    ExprPtr parseLOrExp();             // LOrExp → LAndExp ('||' LAndExp)*
    ExprPtr parseLAndExp();            // LAndExp → EqExp ('&&' EqExp)*
    ExprPtr parseEqExp();              // EqExp → RelExp (('==' | '!=') RelExp)*
    ExprPtr parseRelExp();             // RelExp → AddExp (('<' | '>' | '<=' | '>=') AddExp)*
    ExprPtr parseAddExp();             // AddExp → MulExp (('+' | '-') MulExp)*
    ExprPtr parseMulExp();             // MulExp → UnaryExp (('*' | '/' | '%') UnaryExp)*
    ExprPtr parseUnaryExp();           // UnaryExp → PrimaryExp | UnaryOp UnaryExp
    ExprPtr parsePrimaryExp();         // PrimaryExp → '(' Expr ')' | LVal | Number | Call
    ExprPtr parseLVal();               // LVal → Ident ('[' Exp ']')*

    // 辅助
    ExprPtr parseConstExp();           // 编译期常量表达式
    std::vector<ExprPtr> parseFuncRParams(); // 函数实参列表
};

} // namespace sysy
