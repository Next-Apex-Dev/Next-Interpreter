// 源文件读取与 UTF-8 校验模块
#pragma once
#include <fstream>
#include <string>
#include <string_view>
#include <optional>
#include "diagnostic.hpp"

namespace next11 {

class SourceFile {
public:
    std::string _content;
    std::string _filename;

    static std::optional<SourceFile> load(std::string_view path,
                                          ErrorCollector& errors) {
        SourceFile sf;
        sf._filename = std::string(path);
        std::ifstream ifs(sf._filename, std::ios::binary);
        if (!ifs.is_open()) {
            errors.add(Diagnostic(ErrorCategory::Cli, "CLI001",
                sf._filename, 0, 0,
                std::string("源文件不存在：") + sf._filename));
            return std::nullopt;
        }
        std::string content((std::istreambuf_iterator<char>(ifs)),
                             std::istreambuf_iterator<char>());
        ifs.close();
        if (!is_valid_utf8(content)) {
            errors.add(Diagnostic(ErrorCategory::Cli, "CLI002",
                sf._filename, 0, 0,
                std::string("源文件编码错误，请使用 UTF-8：") + sf._filename));
            return std::nullopt;
        }
        sf._content = std::move(content);
        return sf;
    }

    static bool is_valid_utf8(std::string_view s) {
        size_t i = 0;
        while (i < s.size()) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            if (c <= 0x7F) { i += 1; }
            else if ((c & 0xE0) == 0xC0) {
                if (i + 1 >= s.size()) return false;
                if ((static_cast<unsigned char>(s[i+1]) & 0xC0) != 0x80) return false;
                i += 2;
            } else if ((c & 0xF0) == 0xE0) {
                if (i + 2 >= s.size()) return false;
                if ((static_cast<unsigned char>(s[i+1]) & 0xC0) != 0x80) return false;
                if ((static_cast<unsigned char>(s[i+2]) & 0xC0) != 0x80) return false;
                i += 3;
            } else if ((c & 0xF8) == 0xF0) {
                if (i + 3 >= s.size()) return false;
                if ((static_cast<unsigned char>(s[i+1]) & 0xC0) != 0x80) return false;
                if ((static_cast<unsigned char>(s[i+2]) & 0xC0) != 0x80) return false;
                if ((static_cast<unsigned char>(s[i+3]) & 0xC0) != 0x80) return false;
                i += 4;
            } else { return false; }
        }
        return true;
    }
};

} // namespace next11