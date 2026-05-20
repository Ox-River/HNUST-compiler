#include "Parser.h"
#include <sstream>

namespace sysy {

Parser::Parser(std::vector<Token> tokens)
    : tokens_(std::move(tokens)), pos_(0) {}

// ======== 辅助函数 ========

const Token& Parser::peek() const {
    return tokens_[pos_];
}

const Token& Parser::peekPrev() const {
    return tokens_[pos_ - 1];
}

Token Parser::advance() {
    if (pos_ < tokens_.size()) {
        return tokens_[pos_++];
    }
    return tokens_.back();
}

bool Parser::check(TokenType type) const {
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::expect(TokenType type) {
    if (check(type)) {
        return advance();
    }
    Token current = peek();
    addError("期望 " + tokenTypeName(type) + "，但得到 " +
             tokenTypeName(current.type) + " ('" + current.lexeme + "')", current);
    // 返回一个错误标记，调用者可以继续解析
    return Token(type, "", current.line, current.column);
}

void Parser::addError(const std::string& msg, const Token& tok) {
    std::ostringstream oss;
    oss << "语法错误 [行" << tok.line << ", 列" << tok.column << "]: " << msg;
    errors_.push_back(oss.str());
}

// ======== 编译单元 ========

std::unique_ptr<CompUnit> Parser::parse() {
    return parseCompUnit();
}

std::unique_ptr<CompUnit> Parser::parseCompUnit() {
    auto unit = std::make_unique<CompUnit>();

    while (!check(TokenType::END_OF_FILE)) {
        // 跳过意外出现的分号
        if (match(TokenType::SEMICOLON)) continue;

        if (isDeclStart()) {
            // 尝试区分变量声明和函数定义
            // 函数定义特征: type Ident (... ，即类型后跟标识符再跟 (
            // 变量声明特征: type Ident ... ; 或 type Ident , ...
            // 需要看第三个Token来判断
            size_t savedPos = pos_;
            advance(); // BType
            if (!check(TokenType::IDENTIFIER)) {
                pos_ = savedPos;
                advance(); // 跳过有问题的token
                continue;
            }
            advance(); // Ident
            if (check(TokenType::LPAREN)) {
                // 是函数定义: type Ident(
                pos_ = savedPos;
                auto func = parseFuncDef();
                unit->funcDefs.push_back(std::unique_ptr<FuncDef>(
                    static_cast<FuncDef*>(func.release())));
            } else {
                // 是变量声明
                pos_ = savedPos;
                auto decl = parseDecl();
                unit->globalDecls.push_back(std::unique_ptr<VarDecl>(
                    static_cast<VarDecl*>(decl.release())));
            }
        } else if (isFuncDefStart()) {
            auto func = parseFuncDef();
            unit->funcDefs.push_back(std::unique_ptr<FuncDef>(
                static_cast<FuncDef*>(func.release())));
        } else {
            Token tok = peek();
            addError("非预期的Token: '" + tok.lexeme + "'", tok);
            advance(); // 跳过有问题的token继续解析
        }
    }
    return unit;
}

// ======== 声明 ========

bool Parser::isDeclStart() const {
    return check(TokenType::INT) || check(TokenType::FLOAT) || check(TokenType::CONST);
}

BuiltinType Parser::parseBType() {
    if (match(TokenType::INT)) return BuiltinType::INT;
    if (match(TokenType::FLOAT)) return BuiltinType::FLOAT;
    addError("期望类型名 int 或 float，但得到 '" + peek().lexeme + "'", peek());
    return BuiltinType::INT; // 容错
}

StmtPtr Parser::parseDecl() {
    if (check(TokenType::CONST)) {
        return parseConstDecl();
    }
    return parseVarDecl();
}

StmtPtr Parser::parseConstDecl() {
    expect(TokenType::CONST);
    BuiltinType bt = parseBType();

    std::vector<VarDef> defs;
    do {
        if (!check(TokenType::IDENTIFIER)) break;
        std::string name = advance().lexeme;

        // 数组维度
        std::vector<int> arrayDims;
        while (match(TokenType::LBRACKET)) {
            auto dimExpr = parseConstExp();
            // 尝试求值常量表达式（简单处理：整数常量直接取值）
            int dimVal = 0;
            if (auto* lit = dynamic_cast<IntLiteralExpr*>(dimExpr.get())) {
                dimVal = lit->value;
            } else {
                addError("数组维度必须是编译期常量整数", peek());
            }
            arrayDims.push_back(dimVal);
            expect(TokenType::RBRACKET);
        }

        expect(TokenType::ASSIGN);
        auto initVal = parseExp(); // InitVal
        defs.push_back({name, arrayDims, std::move(initVal)});
    } while (match(TokenType::COMMA));

    expect(TokenType::SEMICOLON);
    return std::make_unique<VarDecl>(bt, true, std::move(defs));
}

StmtPtr Parser::parseVarDecl() {
    BuiltinType bt = parseBType();

    std::vector<VarDef> defs;
    do {
        if (!check(TokenType::IDENTIFIER)) break;
        std::string name = advance().lexeme;

        // 数组维度
        std::vector<int> arrayDims;
        while (match(TokenType::LBRACKET)) {
            auto dimExpr = parseConstExp();
            int dimVal = 0;
            if (auto* lit = dynamic_cast<IntLiteralExpr*>(dimExpr.get())) {
                dimVal = lit->value;
            } else {
                addError("数组维度必须是编译期常量整数", peek());
            }
            arrayDims.push_back(dimVal);
            expect(TokenType::RBRACKET);
        }

        // 初始化表达式（可选）
        ExprPtr initVal = nullptr;
        if (match(TokenType::ASSIGN)) {
            initVal = parseExp();
        }

        defs.push_back({name, arrayDims, std::move(initVal)});
    } while (match(TokenType::COMMA));

    expect(TokenType::SEMICOLON);
    return std::make_unique<VarDecl>(bt, false, std::move(defs));
}

// ======== 函数定义 ========

bool Parser::isFuncDefStart() const {
    // 和isDeclStart一样，实际区分在parseCompUnit中处理
    return check(TokenType::INT) || check(TokenType::FLOAT) || check(TokenType::VOID);
}

BuiltinType Parser::parseFuncType() {
    if (match(TokenType::INT)) return BuiltinType::INT;
    if (match(TokenType::FLOAT)) return BuiltinType::FLOAT;
    if (match(TokenType::VOID)) return BuiltinType::VOID;
    addError("期望函数返回类型 int/float/void，但得到 '" + peek().lexeme + "'", peek());
    return BuiltinType::INT;
}

StmtPtr Parser::parseFuncDef() {
    BuiltinType retType = parseFuncType();

    std::string name;
    if (check(TokenType::IDENTIFIER)) {
        name = advance().lexeme;
    } else {
        addError("期望函数名，但得到 '" + peek().lexeme + "'", peek());
    }

    expect(TokenType::LPAREN);
    std::vector<FuncParam> params;
    if (!check(TokenType::RPAREN)) {
        params = parseFuncFParams();
    }
    expect(TokenType::RPAREN);

    auto body = parseBlock();

    return std::make_unique<FuncDef>(retType, name, std::move(params), std::move(body));
}

std::vector<FuncParam> Parser::parseFuncFParams() {
    std::vector<FuncParam> params;
    do {
        params.push_back(parseFuncFParam());
    } while (match(TokenType::COMMA));
    return params;
}

FuncParam Parser::parseFuncFParam() {
    BuiltinType type = parseBType();
    std::string name;
    if (check(TokenType::IDENTIFIER)) {
        name = advance().lexeme;
    } else {
        addError("期望形参名", peek());
    }

    bool isArray = false;
    std::vector<int> arrayDims;
    // 数组参数: type name[] 或 type name[][N]...
    if (match(TokenType::LBRACKET)) {
        isArray = true;
        expect(TokenType::RBRACKET); // 第一维为空
        while (match(TokenType::LBRACKET)) {
            auto dimExpr = parseConstExp();
            int dimVal = 0;
            if (auto* lit = dynamic_cast<IntLiteralExpr*>(dimExpr.get())) {
                dimVal = lit->value;
            }
            arrayDims.push_back(dimVal);
            expect(TokenType::RBRACKET);
        }
    }

    return {name, type, isArray, arrayDims};
}

// ======== 语句块 ========

StmtPtr Parser::parseBlock() {
    expect(TokenType::LBRACE);
    std::vector<StmtPtr> stmts;
    while (!check(TokenType::RBRACE) && !check(TokenType::END_OF_FILE)) {
        auto item = parseBlockItem();
        if (item) stmts.push_back(std::move(item));
    }
    expect(TokenType::RBRACE);
    return std::make_unique<BlockStmt>(std::move(stmts));
}

StmtPtr Parser::parseBlockItem() {
    if (isDeclStart()) {
        return parseDecl();
    }
    return parseStmt();
}

// ======== 语句 ========

StmtPtr Parser::parseStmt() {
    // if语句
    if (match(TokenType::IF)) {
        expect(TokenType::LPAREN);
        auto cond = parseLOrExp(); // Cond → LOrExp
        expect(TokenType::RPAREN);
        auto thenStmt = parseStmt();
        StmtPtr elseStmt = nullptr;
        if (match(TokenType::ELSE)) {
            elseStmt = parseStmt();
        }
        return std::make_unique<IfStmt>(std::move(cond), std::move(thenStmt),
                                         std::move(elseStmt));
    }

    // while语句
    if (match(TokenType::WHILE)) {
        expect(TokenType::LPAREN);
        auto cond = parseLOrExp();
        expect(TokenType::RPAREN);
        auto body = parseStmt();
        return std::make_unique<WhileStmt>(std::move(cond), std::move(body));
    }

    // break语句
    if (match(TokenType::BREAK)) {
        expect(TokenType::SEMICOLON);
        return std::make_unique<BreakStmt>();
    }

    // continue语句
    if (match(TokenType::CONTINUE)) {
        expect(TokenType::SEMICOLON);
        return std::make_unique<ContinueStmt>();
    }

    // return语句
    if (match(TokenType::RETURN)) {
        ExprPtr retExpr = nullptr;
        if (!check(TokenType::SEMICOLON)) {
            retExpr = parseExp();
        }
        expect(TokenType::SEMICOLON);
        return std::make_unique<ReturnStmt>(std::move(retExpr));
    }

    // 语句块
    if (check(TokenType::LBRACE)) {
        return parseBlock();
    }

    // 表达式语句 / 赋值语句 / 空语句
    // 需要区分 LVal '=' Exp ';' 和 Exp ';'
    if (match(TokenType::SEMICOLON)) {
        return std::make_unique<ExprStmt>(nullptr); // 空语句
    }

    // 尝试解析LVal = Exp 或普通表达式
    // 特征: 以标识符开头，可能是赋值或函数调用
    if (check(TokenType::IDENTIFIER)) {
        size_t savedPos = pos_;
        std::string name = advance().lexeme;

        // 检查是否是数组访问或函数调用
        if (check(TokenType::LBRACKET)) {
            // 数组访问: arr[idx]... 可能是赋值左侧
            std::vector<ExprPtr> indices;
            while (match(TokenType::LBRACKET)) {
                indices.push_back(parseExp());
                expect(TokenType::RBRACKET);
            }

            if (check(TokenType::ASSIGN)) {
                advance(); // 跳过 =
                auto rhs = parseExp();
                expect(TokenType::SEMICOLON);
                return std::make_unique<AssignStmt>(name, std::move(indices),
                                                     std::move(rhs));
            }
            // 不是赋值，回退按普通表达式处理
            pos_ = savedPos;
        } else if (check(TokenType::ASSIGN)) {
            // 普通变量赋值: a = expr;
            advance(); // 跳过 =
            auto rhs = parseExp();
            expect(TokenType::SEMICOLON);
            return std::make_unique<AssignStmt>(name, {}, std::move(rhs));
        } else {
            // 回退，按普通表达式处理
            pos_ = savedPos;
        }
    }

    // 普通表达式语句
    auto expr = parseExp();
    expect(TokenType::SEMICOLON);
    return std::make_unique<ExprStmt>(std::move(expr));
}

// ======== 表达式解析 ========

ExprPtr Parser::parseExp() {
    return parseLOrExp();
}

ExprPtr Parser::parseConstExp() {
    // 编译期常量表达式，这里简化处理为普通表达式
    return parseExp();
}

// LOrExp → LAndExp ('||' LAndExp)*
ExprPtr Parser::parseLOrExp() {
    auto left = parseLAndExp();
    while (match(TokenType::OR)) {
        auto right = parseLAndExp();
        left = std::make_unique<BinaryExpr>("||", std::move(left), std::move(right));
    }
    return left;
}

// LAndExp → EqExp ('&&' EqExp)*
ExprPtr Parser::parseLAndExp() {
    auto left = parseEqExp();
    while (match(TokenType::AND)) {
        auto right = parseEqExp();
        left = std::make_unique<BinaryExpr>("&&", std::move(left), std::move(right));
    }
    return left;
}

// EqExp → RelExp (('==' | '!=') RelExp)*
ExprPtr Parser::parseEqExp() {
    auto left = parseRelExp();
    while (check(TokenType::EQUAL) || check(TokenType::NOT_EQUAL)) {
        std::string op = advance().lexeme;
        auto right = parseRelExp();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

// RelExp → AddExp (('<' | '>' | '<=' | '>=') AddExp)*
ExprPtr Parser::parseRelExp() {
    auto left = parseAddExp();
    while (check(TokenType::LESS) || check(TokenType::GREATER) ||
           check(TokenType::LESS_EQ) || check(TokenType::GREATER_EQ)) {
        std::string op = advance().lexeme;
        auto right = parseAddExp();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

// AddExp → MulExp (('+' | '-') MulExp)*
ExprPtr Parser::parseAddExp() {
    auto left = parseMulExp();
    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        std::string op = advance().lexeme;
        auto right = parseMulExp();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

// MulExp → UnaryExp (('*' | '/' | '%') UnaryExp)*
ExprPtr Parser::parseMulExp() {
    auto left = parseUnaryExp();
    while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::PERCENT)) {
        std::string op = advance().lexeme;
        auto right = parseUnaryExp();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

// UnaryExp → PrimaryExp | UnaryOp UnaryExp
ExprPtr Parser::parseUnaryExp() {
    // 一元运算符
    if (check(TokenType::MINUS) || check(TokenType::PLUS) || check(TokenType::NOT)) {
        std::string op = advance().lexeme;
        auto operand = parseUnaryExp();
        return std::make_unique<UnaryExpr>(op, std::move(operand));
    }

    return parsePrimaryExp();
}

// PrimaryExp → '(' Exp ')' | LVal | Number | Ident '(' [FuncRParams] ')'
ExprPtr Parser::parsePrimaryExp() {
    // 括号表达式
    if (match(TokenType::LPAREN)) {
        auto expr = parseExp();
        expect(TokenType::RPAREN);
        return expr;
    }

    // 数字字面量
    if (check(TokenType::INT_LITERAL)) {
        int val = std::get<int>(peek().value);
        advance();
        return std::make_unique<IntLiteralExpr>(val);
    }
    if (check(TokenType::FLOAT_LITERAL)) {
        float val = std::get<float>(peek().value);
        advance();
        return std::make_unique<FloatLiteralExpr>(val);
    }

    // 标识符：可能是变量引用、函数调用或数组访问
    if (check(TokenType::IDENTIFIER)) {
        std::string name = advance().lexeme;

        // 函数调用
        if (check(TokenType::LPAREN)) {
            advance(); // 跳过 (
            std::vector<ExprPtr> args;
            if (!check(TokenType::RPAREN)) {
                args = parseFuncRParams();
            }
            expect(TokenType::RPAREN);
            return std::make_unique<CallExpr>(name, std::move(args));
        }

        // 数组访问
        if (check(TokenType::LBRACKET)) {
            std::vector<ExprPtr> indices;
            while (match(TokenType::LBRACKET)) {
                indices.push_back(parseExp());
                expect(TokenType::RBRACKET);
            }
            return std::make_unique<ArrayAccessExpr>(name, std::move(indices));
        }

        // 普通变量引用
        return std::make_unique<IdentifierExpr>(name);
    }

    addError("非预期的表达式Token: '" + peek().lexeme + "'", peek());
    advance();
    return std::make_unique<IntLiteralExpr>(0); // 容错
}

ExprPtr Parser::parseLVal() {
    if (!check(TokenType::IDENTIFIER)) {
        addError("期望标识符作为左值", peek());
        return std::make_unique<IdentifierExpr>("<error>");
    }
    std::string name = advance().lexeme;

    // 数组访问
    if (check(TokenType::LBRACKET)) {
        std::vector<ExprPtr> indices;
        while (match(TokenType::LBRACKET)) {
            indices.push_back(parseExp());
            expect(TokenType::RBRACKET);
        }
        return std::make_unique<ArrayAccessExpr>(name, std::move(indices));
    }

    return std::make_unique<IdentifierExpr>(name);
}

std::vector<ExprPtr> Parser::parseFuncRParams() {
    std::vector<ExprPtr> args;
    do {
        args.push_back(parseExp());
    } while (match(TokenType::COMMA));
    return args;
}

} // namespace sysy
