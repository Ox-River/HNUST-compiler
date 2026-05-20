#pragma once

#include "Token.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace sysy {

// 词法分析器 — 将SysY源代码字符串转换为Token序列
// 支持：关键字、标识符、整/浮点字面量、运算符、分隔符、注释
class Lexer {
public:
    explicit Lexer(std::string source);

    // 执行词法分析，返回所有Token
    std::vector<Token> tokenize();

    // 获取错误信息
    const std::vector<std::string>& errors() const { return errors_; }

private:
    std::string source_;
    size_t pos_;        // 当前字符位置
    int line_;          // 当前行号
    int column_;        // 当前列号
    size_t length_;     // 源代码总长度

    std::vector<std::string> errors_;

    // 关键字映射表
    static const std::unordered_map<std::string, TokenType> keywords_;

    // 辅助函数
    char peek() const;              // 查看当前字符
    char peekNext() const;          // 查看下一个字符
    char advance();                 // 消费当前字符并前进
    bool isAtEnd() const;           // 是否到达末尾
    void skipWhitespace();          // 跳过空白字符
    void skipComment();             // 跳过注释（//行注释 和 /*块注释*/）
    Token scanToken();              // 扫描下一个Token
    Token scanIdentifier();         // 扫描标识符或关键字
    Token scanNumber();             // 扫描数字字面量
    Token scanOperatorOrDelimiter();// 扫描运算符或分隔符
};

} // namespace sysy
