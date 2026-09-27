// 词法分析器接口与实现
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "common/token.hpp"
#include "common/diagnostic.hpp"
#include "common/interfaces.hpp"

namespace next11 {

class Lexer : public ILexer {
public:
    LexResult tokenize(std::string_view source, std::string_view filename) override;

    static const std::vector<std::string>& keywords() {
        static const std::vector<std::string> kw = {
            "let","fn","def","if","else","match","return","while","for","in",
            "true","false","int","float","string","bool","void",
            "struct","enum","type","break","continue",
            "async","await","with","try","catch","finally","throw",
            "class","extends","super","classmethod","staticmethod","private","protected","public",
            "import","from","as","del","yield"
        };
        return kw;
    }

    static bool is_keyword(std::string_view s) {
        for (auto& k : keywords()) if (k == s) return true;
        return false;
    }

private:
    std::string_view _source;
    std::string _file;
    size_t _pos = 0;
    int _line = 1;
    int _col = 1;
    std::vector<Diagnostic> _errors;

    char peek(size_t off = 0) const;
    bool at_end() const { return _pos >= _source.size(); }
    void advance();
    void skip_whitespace_and_comments();
    Token scan_token();
    Token scan_identifier();
    Token scan_number();
    Token scan_string();
    Token scan_operator();
    void error(ErrorCategory cat, std::string code, int l, int c,
               std::string desc, std::optional<std::string> fix = std::nullopt);
};

} // namespace next11