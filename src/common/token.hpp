// Token 数据结构 - 对齐 spec 6.1
#pragma once
#include <string>
#include <vector>

namespace next11 {

enum class TokenKind {
    Keyword, Identifier, IntLiteral, FloatLiteral, StringLiteral,
    BoolLiteral, ComplexLiteral, Operator, Delimiter, TypeAnnot, Eof
};

struct Token {
    TokenKind kind;
    std::string lexeme;
    int line;
    int col;
    std::string file;

    Token() : kind(TokenKind::Eof), lexeme(""), line(0), col(0), file("") {}
    Token(TokenKind k, std::string lex, int l, int c, std::string f)
        : kind(k), lexeme(std::move(lex)), line(l), col(c), file(std::move(f)) {}
};

inline std::string token_kind_name(TokenKind k) {
    switch (k) {
        case TokenKind::Keyword: return "Keyword";
        case TokenKind::Identifier: return "Identifier";
        case TokenKind::IntLiteral: return "IntLiteral";
        case TokenKind::FloatLiteral: return "FloatLiteral";
        case TokenKind::StringLiteral: return "StringLiteral";
        case TokenKind::BoolLiteral: return "BoolLiteral";
        case TokenKind::ComplexLiteral: return "ComplexLiteral";
        case TokenKind::Operator: return "Operator";
        case TokenKind::Delimiter: return "Delimiter";
        case TokenKind::TypeAnnot: return "TypeAnnot";
        case TokenKind::Eof: return "Eof";
    }
    return "Unknown";
}

} // namespace next11