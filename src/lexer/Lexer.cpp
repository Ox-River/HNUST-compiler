#include "Lexer.h"
#include <cctype>
#include <sstream>

namespace sysy {

// 关键字映射表
const std::unordered_map<std::string, TokenType> Lexer::keywords_ = {
    {"int",      TokenType::INT},
    {"float",    TokenType::FLOAT},
    {"void",     TokenType::VOID},
    {"const",    TokenType::CONST},
    {"if",       TokenType::IF},
    {"else",     TokenType::ELSE},
    {"while",    TokenType::WHILE},
    {"break",    TokenType::BREAK},
    {"continue", TokenType::CONTINUE},
    {"return",   TokenType::RETURN},
};

Lexer::Lexer(std::string source)
    : source_(std::move(source)), pos_(0), line_(1), column_(1),
      length_(source_.length()) {}

char Lexer::peek() const {
    return isAtEnd() ? '\0' : source_[pos_];
}

char Lexer::peekNext() const {
    return (pos_ + 1 >= length_) ? '\0' : source_[pos_ + 1];
}

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

bool Lexer::isAtEnd() const {
    return pos_ >= length_;
}

void Lexer::skipWhitespace() {
    while (!isAtEnd() && std::isspace(peek())) {
        advance();
    }
}

void Lexer::skipComment() {
    if (peek() == '/' && peekNext() == '/') {
        // 行注释：跳过直到行尾
        while (!isAtEnd() && peek() != '\n') {
            advance();
        }
    } else if (peek() == '/' && peekNext() == '*') {
        // 块注释：跳过直到 */
        advance(); advance(); // 跳过 /*
        while (!isAtEnd()) {
            if (peek() == '*' && peekNext() == '/') {
                advance(); advance(); // 跳过 */
                return;
            }
            advance();
        }
        errors_.push_back("未闭合的块注释，从第 " +
                          std::to_string(line_) + " 行开始");
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (!isAtEnd()) {
        skipWhitespace();

        // 处理注释
        if (peek() == '/' && (peekNext() == '/' || peekNext() == '*')) {
            skipComment();
            continue;
        }

        if (isAtEnd()) break;

        Token token = scanToken();
        if (token.type == TokenType::ERROR) {
            errors_.push_back("词法错误在第 " + std::to_string(token.line) +
                              " 行: " + token.lexeme);
        }
        tokens.push_back(std::move(token));
    }
    tokens.emplace_back(TokenType::END_OF_FILE, "EOF", line_, column_);
    return tokens;
}

Token Lexer::scanToken() {
    char c = peek();

    // 标识符或关键字（以字母或下划线开头）
    if (std::isalpha(c) || c == '_') {
        return scanIdentifier();
    }

    // 数字字面量
    if (std::isdigit(c)) {
        return scanNumber();
    }

    // 运算符和分隔符
    return scanOperatorOrDelimiter();
}

Token Lexer::scanIdentifier() {
    int start_col = column_;
    std::string lexeme;
    while (!isAtEnd() && (std::isalnum(peek()) || peek() == '_')) {
        lexeme += advance();
    }

    // 检查是否为关键字
    auto it = keywords_.find(lexeme);
    TokenType type = (it != keywords_.end()) ? it->second : TokenType::IDENTIFIER;

    return Token(type, lexeme, line_, start_col);
}

Token Lexer::scanNumber() {
    int start_col = column_;
    std::string lexeme;
    bool is_float = false;

    // 整数部分
    while (!isAtEnd() && std::isdigit(peek())) {
        lexeme += advance();
    }

    // 小数部分
    if (peek() == '.' && std::isdigit(peekNext())) {
        is_float = true;
        lexeme += advance(); // 跳过 '.'
        while (!isAtEnd() && std::isdigit(peek())) {
            lexeme += advance();
        }
    }

    // 科学计数法（可选）
    if (peek() == 'e' || peek() == 'E') {
        is_float = true;
        lexeme += advance();
        if (peek() == '+' || peek() == '-') {
            lexeme += advance();
        }
        while (!isAtEnd() && std::isdigit(peek())) {
            lexeme += advance();
        }
    }

    if (is_float) {
        float value = std::stof(lexeme);
        return Token(TokenType::FLOAT_LITERAL, lexeme, line_, start_col, value);
    } else {
        int value = std::stoi(lexeme);
        return Token(TokenType::INT_LITERAL, lexeme, line_, start_col, value);
    }
}

Token Lexer::scanOperatorOrDelimiter() {
    int start_col = column_;
    char c = advance();

    switch (c) {
        case '+': return Token(TokenType::PLUS, "+", line_, start_col);
        case '-': return Token(TokenType::MINUS, "-", line_, start_col);
        case '*': return Token(TokenType::STAR, "*", line_, start_col);
        case '%': return Token(TokenType::PERCENT, "%", line_, start_col);

        case '/':
            // 注释已在主循环中处理，这里只可能是单独的除号
            return Token(TokenType::SLASH, "/", line_, start_col);

        case '(': return Token(TokenType::LPAREN, "(", line_, start_col);
        case ')': return Token(TokenType::RPAREN, ")", line_, start_col);
        case '{': return Token(TokenType::LBRACE, "{", line_, start_col);
        case '}': return Token(TokenType::RBRACE, "}", line_, start_col);
        case '[': return Token(TokenType::LBRACKET, "[", line_, start_col);
        case ']': return Token(TokenType::RBRACKET, "]", line_, start_col);
        case ';': return Token(TokenType::SEMICOLON, ";", line_, start_col);
        case ',': return Token(TokenType::COMMA, ",", line_, start_col);

        case '=':
            if (peek() == '=') {
                advance();
                return Token(TokenType::EQUAL, "==", line_, start_col);
            }
            return Token(TokenType::ASSIGN, "=", line_, start_col);

        case '!':
            if (peek() == '=') {
                advance();
                return Token(TokenType::NOT_EQUAL, "!=", line_, start_col);
            }
            return Token(TokenType::NOT, "!", line_, start_col);

        case '<':
            if (peek() == '=') {
                advance();
                return Token(TokenType::LESS_EQ, "<=", line_, start_col);
            }
            return Token(TokenType::LESS, "<", line_, start_col);

        case '>':
            if (peek() == '=') {
                advance();
                return Token(TokenType::GREATER_EQ, ">=", line_, start_col);
            }
            return Token(TokenType::GREATER, ">", line_, start_col);

        case '&':
            if (peek() == '&') {
                advance();
                return Token(TokenType::AND, "&&", line_, start_col);
            }
            break;

        case '|':
            if (peek() == '|') {
                advance();
                return Token(TokenType::OR, "||", line_, start_col);
            }
            break;

        default:
            break;
    }

    return Token(TokenType::ERROR,
                 "非法字符 '" + std::string(1, c) + "'",
                 line_, start_col);
}

// Token辅助方法实现
std::string tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::INT:           return "INT";
        case TokenType::FLOAT:         return "FLOAT";
        case TokenType::VOID:          return "VOID";
        case TokenType::CONST:         return "CONST";
        case TokenType::IF:            return "IF";
        case TokenType::ELSE:          return "ELSE";
        case TokenType::WHILE:         return "WHILE";
        case TokenType::BREAK:         return "BREAK";
        case TokenType::CONTINUE:      return "CONTINUE";
        case TokenType::RETURN:        return "RETURN";
        case TokenType::IDENTIFIER:    return "IDENTIFIER";
        case TokenType::INT_LITERAL:   return "INT_LITERAL";
        case TokenType::FLOAT_LITERAL: return "FLOAT_LITERAL";
        case TokenType::PLUS:          return "PLUS";
        case TokenType::MINUS:         return "MINUS";
        case TokenType::STAR:          return "STAR";
        case TokenType::SLASH:         return "SLASH";
        case TokenType::PERCENT:       return "PERCENT";
        case TokenType::ASSIGN:        return "ASSIGN";
        case TokenType::EQUAL:         return "EQUAL";
        case TokenType::NOT_EQUAL:     return "NOT_EQUAL";
        case TokenType::LESS:          return "LESS";
        case TokenType::GREATER:       return "GREATER";
        case TokenType::LESS_EQ:       return "LESS_EQ";
        case TokenType::GREATER_EQ:    return "GREATER_EQ";
        case TokenType::AND:           return "AND";
        case TokenType::OR:            return "OR";
        case TokenType::NOT:           return "NOT";
        case TokenType::LPAREN:        return "LPAREN";
        case TokenType::RPAREN:        return "RPAREN";
        case TokenType::LBRACE:        return "LBRACE";
        case TokenType::RBRACE:        return "RBRACE";
        case TokenType::LBRACKET:      return "LBRACKET";
        case TokenType::RBRACKET:      return "RBRACKET";
        case TokenType::SEMICOLON:     return "SEMICOLON";
        case TokenType::COMMA:         return "COMMA";
        case TokenType::END_OF_FILE:   return "EOF";
        case TokenType::ERROR:         return "ERROR";
        default:                       return "UNKNOWN";
    }
}

std::string Token::toString() const {
    std::ostringstream oss;
    oss << "Token{" << tokenTypeName(type)
        << ", '" << lexeme << "'"
        << ", line " << line
        << ", col " << column << "}";
    return oss.str();
}

} // namespace sysy
