#pragma once

#include <string>
#include <variant>

namespace sysy {

// Token类型枚举 — 覆盖SysY2022语言所有词法单元
enum class TokenType {
    // 关键字
    INT,            // int
    FLOAT,          // float
    VOID,           // void
    CONST,          // const
    IF,             // if
    ELSE,           // else
    WHILE,          // while
    BREAK,          // break
    CONTINUE,       // continue
    RETURN,         // return

    // 标识符与字面量
    IDENTIFIER,     // 标识符
    INT_LITERAL,    // 整数字面量
    FLOAT_LITERAL,  // 浮点数字面量

    // 运算符
    PLUS,           // +
    MINUS,          // -
    STAR,           // *
    SLASH,          // /
    PERCENT,        // %
    ASSIGN,         // =
    EQUAL,          // ==
    NOT_EQUAL,      // !=
    LESS,           // <
    GREATER,        // >
    LESS_EQ,        // <=
    GREATER_EQ,     // >=
    AND,            // &&
    OR,             // ||
    NOT,            // !

    // 分隔符
    LPAREN,         // (
    RPAREN,         // )
    LBRACE,         // {
    RBRACE,         // }
    LBRACKET,       // [
    RBRACKET,       // ]
    SEMICOLON,      // ;
    COMMA,          // ,

    END_OF_FILE,    // 文件结束
    ERROR           // 错误Token
};

// Token值类型 — 支持int和float两种数值
using TokenValue = std::variant<std::monostate, int, float>;

struct Token {
    TokenType type;
    std::string lexeme;     // 原始字符串
    int line;               // 行号
    int column;             // 列号
    TokenValue value;       // 数值（字面量Token使用）

    Token(TokenType t, std::string lex, int l, int c, TokenValue v = {})
        : type(t), lexeme(std::move(lex)), line(l), column(c), value(std::move(v)) {}

    // 调试用：返回Token的字符串描述
    std::string toString() const;
};

// 将TokenType转换为可读字符串
std::string tokenTypeName(TokenType type);

} // namespace sysy
