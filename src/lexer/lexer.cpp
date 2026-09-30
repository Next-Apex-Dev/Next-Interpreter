// 词法分析器实现
#include "lexer.hpp"
#include <cctype>

namespace next11 {

char Lexer::peek(size_t off) const {
    if (_pos + off >= _source.size()) return '\0';
    return _source[_pos + off];
}

void Lexer::advance() {
    if (_pos < _source.size()) {
        if (_source[_pos] == '\n') { ++_line; _col = 1; }
        else { ++_col; }
        ++_pos;
    }
}

void Lexer::error(ErrorCategory cat, std::string code, int l, int c,
                   std::string desc, std::optional<std::string> fix) {
    _errors.emplace_back(cat, std::move(code), _file, l, c, std::move(desc), std::move(fix));
}

void Lexer::skip_whitespace_and_comments() {
    while (!at_end()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            while (!at_end() && peek() != '\n') advance();
        } else if (c == '/' && peek(1) == '*') {
            int start_line = _line, start_col = _col;
            advance(); advance();
            bool closed = false;
            while (!at_end()) {
                if (peek() == '*' && peek(1) == '/') {
                    advance(); advance();
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) {
                error(ErrorCategory::Lex, "LEX003", start_line, start_col,
                      "块注释未闭合", "在注释末尾添加 */ 闭合块注释");
            }
        } else {
            break;
        }
    }
}

Token Lexer::scan_identifier() {
    int l = _line, c = _col;
    std::string lex;
    while (!at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
        lex += peek();
        advance();
    }
    TokenKind kind;
    if (is_keyword(lex)) {
        if (lex == "true" || lex == "false") kind = TokenKind::BoolLiteral;
        else kind = TokenKind::Keyword;
    } else {
        kind = TokenKind::Identifier;
    }
    return Token(kind, lex, l, c, _file);
}

Token Lexer::scan_number() {
    int l = _line, c = _col;
    std::string lex;
    bool is_float = false;
    bool is_complex = false;
    while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
        lex += peek();
        advance();
    }
    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
        is_float = true;
        lex += '.';
        advance();
        while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
            lex += peek();
            advance();
        }
    }
    // 检查复数后缀：数字后跟 + 或 - 再跟数字和 i
    if (!at_end() && (peek() == '+' || peek() == '-')) {
        char sign = peek();
        lex += sign;
        advance();
        std::string imag_part;
        while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
            imag_part += peek();
            advance();
        }
        if (!imag_part.empty() && peek() == 'i') {
            is_complex = true;
            lex += imag_part;
            lex += 'i';
            advance();
        } else {
            // 不是复数，回退
            for (size_t i = 0; i < imag_part.size() + 1; ++i) {
                _pos--;
                _col--;
            }
            lex.pop_back();
        }
    }
    // 检查纯虚数：如 4i 或 -3i（负号在前面已处理）
    if (!is_complex && peek() == 'i') {
        is_complex = true;
        lex += 'i';
        advance();
    }
    if (is_complex) {
        return Token(TokenKind::ComplexLiteral, lex, l, c, _file);
    }
    return Token(is_float ? TokenKind::FloatLiteral : TokenKind::IntLiteral,
                 lex, l, c, _file);
}

Token Lexer::scan_string() {
    int l = _line, c = _col;
    advance(); // 跳过开始引号
    std::string value;
    while (!at_end() && peek() != '"') {
        if (peek() == '\n') {
            error(ErrorCategory::Lex, "LEX002", l, c, "字符串未闭合", "在字符串末尾添加闭合引号 \"");
            return Token(TokenKind::StringLiteral, value, l, c, _file);
        }
        if (peek() == '\\') {
            advance();
            char esc = peek();
            switch (esc) {
                case 'n': value += '\n'; advance(); break;
                case 't': value += '\t'; advance(); break;
                case 'r': value += '\r'; advance(); break;
                case 'b': value += '\b'; advance(); break;
                case 'f': value += '\f'; advance(); break;
                case 'v': value += '\v'; advance(); break;
                case 'a': value += '\a'; advance(); break;
                case '\\': value += '\\'; advance(); break;
                case '"': value += '"'; advance(); break;
                case '\'': value += '\''; advance(); break;
                case '0': value += '\0'; advance(); break;
                case '\0':
                    error(ErrorCategory::Lex, "LEX002", l, c, "字符串未闭合", "在字符串末尾添加闭合引号 \"");
                    return Token(TokenKind::StringLiteral, value, l, c, _file);
                default:
                    error(ErrorCategory::Lex, "LEX004", _line, _col,
                          std::string("非法转义序列 '\\") + esc + "'",
                          "使用合法转义序列: \\n \\t \\\\ \\\" \\0");
                    value += esc;
                    advance();
                    break;
            }
        } else {
            value += peek();
            advance();
        }
    }
    if (at_end()) {
        error(ErrorCategory::Lex, "LEX002", l, c, "字符串未闭合", "在字符串末尾添加闭合引号 \"");
    } else {
        advance(); // 跳过结束引号
    }
    return Token(TokenKind::StringLiteral, value, l, c, _file);
}

Token Lexer::scan_operator() {
    int l = _line, c = _col;
    char ch = peek();
    char next = peek(1);

    // 三字符运算符（暂无）

    // 双字符运算符
    std::string two; two += ch; two += next;
    if (two == "==" || two == "!=" || two == "<=" || two == ">=" ||
        two == "&&" || two == "||" || two == "|>" || two == "->" || two == "=>") {
        advance(); advance();
        return Token(TokenKind::Operator, two, l, c, _file);
    }

    // 单字符运算符
    std::string one; one += ch;
    if (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%' ||
        ch == '<' || ch == '>' || ch == '=' || ch == '!' || ch == '|' ||
        ch == '&' || ch == '^' || ch == '@') {
        advance();
        return Token(TokenKind::Operator, one, l, c, _file);
    }

    // 分隔符
    if (ch == '{' || ch == '}' || ch == '(' || ch == ')' ||
        ch == '[' || ch == ']' || ch == ';' || ch == ',' || ch == '.') {
        advance();
        return Token(TokenKind::Delimiter, one, l, c, _file);
    }

    // 类型注解符冒号
    if (ch == ':') {
        advance();
        return Token(TokenKind::TypeAnnot, one, l, c, _file);
    }

    // 非法字符
    error(ErrorCategory::Lex, "LEX001", l, c,
          std::string("非法字符 '") + ch + "'", "移除该非法字符或替换为合法字符");
    advance();
    return Token(TokenKind::Operator, one, l, c, _file);
}

Token Lexer::scan_token() {
    skip_whitespace_and_comments();
    if (at_end()) {
        return Token(TokenKind::Eof, "", _line, _col, _file);
    }
    char c = peek();
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        return scan_identifier();
    }
    if (std::isdigit(static_cast<unsigned char>(c))) {
        return scan_number();
    }
    if (c == '"') {
        return scan_string();
    }
    return scan_operator();
}

LexResult Lexer::tokenize(std::string_view source, std::string_view filename) {
    _source = source;
    _file = std::string(filename);
    _pos = 0;
    _line = 1;
    _col = 1;
    _errors.clear();

    LexResult result;
    while (true) {
        Token t = scan_token();
        if (t.kind == TokenKind::Eof) {
            result.tokens.push_back(t);
            break;
        }
        result.tokens.push_back(t);
    }
    result.errors = std::move(_errors);
    return result;
}

} // namespace next11