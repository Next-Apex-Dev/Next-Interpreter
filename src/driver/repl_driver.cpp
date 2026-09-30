// REPL 驱动实现
#include "repl_driver.hpp"
#include "pipeline.hpp"
#include "common/registry.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#endif

namespace next11 {

bool ReplDriver::braces_balanced(const std::string& s) {
    int depth = 0;
    for (char c : s) {
        if (c == '{') ++depth;
        else if (c == '}') --depth;
    }
    return depth <= 0;
}

// 历史记录管理
class ReplHistory {
public:
    static constexpr size_t MAX_HISTORY = 1000;

    void push(const std::string& line) {
        if (line.empty() || line == ":quit" || line == "exit" || line == "quit") return;
        // 避免连续重复
        if (!_history.empty() && _history.back() == line) return;
        _history.push_back(line);
        if (_history.size() > MAX_HISTORY) {
            _history.pop_front();
        }
        _pos = _history.size();
    }

    const std::string& prev(size_t& pos) const {
        if (_history.empty()) return _empty;
        if (pos > 0) --pos;
        return _history[pos];
    }

    const std::string& next(size_t& pos) const {
        if (_history.empty()) return _empty;
        if (pos < _history.size()) ++pos;
        if (pos >= _history.size()) return _empty;
        return _history[pos];
    }

    void save(const std::string& path) const {
        std::ofstream ofs(path);
        for (const auto& line : _history) {
            ofs << line << "\n";
        }
    }

    void load(const std::string& path) {
        std::ifstream ifs(path);
        if (!ifs) return;
        _history.clear();
        std::string line;
        while (std::getline(ifs, line)) {
            if (!line.empty()) _history.push_back(line);
        }
        if (_history.size() > MAX_HISTORY) {
            _history.erase(_history.begin(), _history.begin() + (_history.size() - MAX_HISTORY));
        }
        _pos = _history.size();
    }

    size_t size() const { return _history.size(); }

private:
    std::deque<std::string> _history;
    size_t _pos = 0;
    std::string _empty;
};

// 自动补全管理
class ReplCompleter {
public:
    void add_keyword(const std::string& kw) { _keywords.push_back(kw); }
    void add_builtin(const std::string& name) { _builtins.push_back(name); }
    void add_variable(const std::string& name) { _variables.push_back(name); }
    void clear_variables() { _variables.clear(); }

    std::vector<std::string> complete(const std::string& prefix) const {
        std::vector<std::string> matches;
        if (prefix.empty()) return matches;

        auto add_matches = [&](const std::vector<std::string>& items) {
            for (const auto& item : items) {
                if (item.size() >= prefix.size() &&
                    std::equal(prefix.begin(), prefix.end(), item.begin(),
                               [](char a, char b) { return std::tolower(a) == std::tolower(b); })) {
                    matches.push_back(item);
                }
            }
        };

        add_matches(_keywords);
        add_matches(_builtins);
        add_matches(_variables);
        return matches;
    }

private:
    std::vector<std::string> _keywords;
    std::vector<std::string> _builtins;
    std::vector<std::string> _variables;
};

// 会话持久化管理
class ReplSession {
public:
    void save(const std::string& path, const std::vector<std::string>& history) {
        std::ofstream ofs(path);
        ofs << "# Next1.1 Session\n";
        for (const auto& line : history) {
            ofs << line << "\n";
        }
    }

    std::vector<std::string> load(const std::string& path) {
        std::vector<std::string> lines;
        std::ifstream ifs(path);
        if (!ifs) return lines;
        std::string line;
        while (std::getline(ifs, line)) {
            if (!line.empty() && line[0] != '#') lines.push_back(line);
        }
        return lines;
    }
};

// Windows 控制台输入处理
#ifdef _WIN32
class ConsoleInput {
public:
    struct KeyResult {
        enum Type { Char, Special, Eof };
        Type type;
        int code;        // 字符码或虚拟键码
        char ch;         // 字符
    };

    KeyResult read_key() {
        INPUT_RECORD ir;
        DWORD read;
        while (true) {
            if (!ReadConsoleInputW(_hStdin, &ir, 1, &read)) return {KeyResult::Eof, 0, 0};
            if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) continue;

            WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
            WCHAR ch = ir.Event.KeyEvent.uChar.UnicodeChar;

            // 特殊键处理
            if (vk == VK_UP) return {KeyResult::Special, VK_UP, 0};
            if (vk == VK_DOWN) return {KeyResult::Special, VK_DOWN, 0};
            if (vk == VK_LEFT) return {KeyResult::Special, VK_LEFT, 0};
            if (vk == VK_RIGHT) return {KeyResult::Special, VK_RIGHT, 0};
            if (vk == VK_HOME) return {KeyResult::Special, VK_HOME, 0};
            if (vk == VK_END) return {KeyResult::Special, VK_END, 0};
            if (vk == VK_TAB) return {KeyResult::Special, VK_TAB, 0};
            if (vk == VK_ESCAPE) return {KeyResult::Special, VK_ESCAPE, 0};
            if (vk == VK_RETURN) return {KeyResult::Special, VK_RETURN, 0};
            if (vk == VK_BACK) return {KeyResult::Special, VK_BACK, 0};

            // 字符输入
            if (ch >= 32 && ch != 127) {
                char buf[4] = {};
                if (ch < 128) { buf[0] = static_cast<char>(ch); }
                else if (ch < 2048) { buf[0] = static_cast<char>(0xC0 | (ch >> 6)); buf[1] = static_cast<char>(0x80 | (ch & 0x3F)); }
                else { buf[0] = static_cast<char>(0xE0 | (ch >> 12)); buf[1] = static_cast<char>(0x80 | ((ch >> 6) & 0x3F)); buf[2] = static_cast<char>(0x80 | (ch & 0x3F)); }
                KeyResult r = {KeyResult::Char, 0, buf[0]};
                return r;
            }
        }
    }

private:
    HANDLE _hStdin = GetStdHandle(STD_INPUT_HANDLE);
};
#endif

// 行编辑状态
struct LineEditState {
    std::string buffer;
    size_t cursor = 0;
    size_t history_pos = 0;

    void insert(char ch) {
        buffer.insert(cursor, 1, ch);
        ++cursor;
    }

    void backspace() {
        if (cursor > 0) {
            buffer.erase(cursor - 1, 1);
            --cursor;
        }
    }

    void del() {
        if (cursor < buffer.size()) {
            buffer.erase(cursor, 1);
        }
    }

    void move_left() { if (cursor > 0) --cursor; }
    void move_right() { if (cursor < buffer.size()) ++cursor; }
    void move_home() { cursor = 0; }
    void move_end() { cursor = buffer.size(); }

    void clear() { buffer.clear(); cursor = 0; }
    void set(const std::string& s) { buffer = s; cursor = buffer.size(); }
};

int ReplDriver::run() {
    Pipeline pipe(_logger);

    // 初始化关键字列表（用于自动补全）
    ReplCompleter completer;
    const std::vector<std::string> keywords = {
        "let", "const", "fn", "struct", "enum", "trait", "impl", "if", "else",
        "for", "while", "loop", "match", "return", "break", "continue",
        "in", "as", "use", "mod", "pub", "priv", "mut", "ref",
        "true", "false", "null", "self", "super", "crate"
    };
    for (const auto& kw : keywords) completer.add_keyword(kw);

    // 初始化内建函数列表
    const std::vector<std::string> builtins = {
        "abs", "real", "imag", "conj", "min", "max", "floor", "ceil", "round",
        "sqrt", "pow", "sin", "cos", "tan", "log", "exp", "atan", "asin",
        "acos", "atan2", "log2", "log10", "sign", "clamp", "gcd", "lcm",
        "hypot", "deg2rad", "rad2deg", "factorial", "fibonacci", "is_prime",
        "pi", "complex", "polar",
        "str", "concat", "split", "join", "replace", "substring", "upper",
        "lower", "trim", "starts_with", "ends_with", "contains", "index_of",
        "char_at", "reverse_str", "repeat_str", "pad_left", "pad_right",
        "format", "parse_int", "parse_float", "to_hex", "from_hex",
        "is_digit", "is_alpha",
        "push", "pop", "shift", "unshift", "sort", "reverse_arr", "slice",
        "concat_arr", "fill", "range", "zip", "flatten", "count", "sum", "avg",
        "min_arr", "max_arr", "distinct", "take", "drop", "chunk", "interleave",
        "rotate", "first", "last",
        "to_int", "to_float", "to_string", "to_bool",
        "is_int", "is_float", "is_string", "is_bool", "is_array", "is_null",
        "print", "println", "input", "len", "type",
        "print_err", "print_raw", "assert", "error", "now", "sleep",
        "open", "read", "write", "close", "stat", "atomic_write",
        "async_read", "async_write",
        "classmethod", "staticmethod", "super", "isinstance", "issubtype",
        "hasattr", "getattr", "setattr", "cast", "callable", "protocol_check",
        "dict", "set", "keys", "values", "items",
        "next", "iter", "StopIteration",
        "property", "wraps", "lru_cache", "cached_property",
        "exit", "help", "reload"
    };
    for (const auto& fn : builtins) completer.add_builtin(fn);

    // 加载历史记录
    ReplHistory history;
    history.load(".next11_history");

    // 加载会话
    ReplSession session;
    auto session_lines = session.load(".next11_session");
    std::string session_history;
    if (!session_lines.empty()) {
        std::cout << "恢复会话 (" << session_lines.size() << " 条声明)\n";
        for (const auto& line : session_lines) {
            session_history += line + "\n";
        }
    }

    std::cout << "Next1.1 REPL (输入 exit 或 :quit 退出)\n";
    std::cout << "输入 help 或 :help 获取帮助\n";

    LineEditState edit;
    std::string accumulated_history = session_history;

#ifdef _WIN32
    // 设置控制台模式以启用 ANSI 转义序列
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hStdout, &mode);
    SetConsoleMode(hStdout, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    // 检测 stdin 是否为控制台（管道模式时用 getline）
    HANDLE hStdin_check = GetStdHandle(STD_INPUT_HANDLE);
    bool is_console = GetConsoleMode(hStdin_check, &mode);
#endif

    while (true) {
        // 显示提示符
        std::cout << (edit.buffer.empty() ? "next11> " : "...> ");
        std::cout.flush();

        // 显示当前行
        std::cout << edit.buffer;
        if (edit.cursor < edit.buffer.size()) {
            std::cout << "\x1b[" << (edit.buffer.size() - edit.cursor) << "D";
        }
        std::cout.flush();

        // 读取并处理输入
        bool line_done = false;
        while (!line_done) {
#ifdef _WIN32
            if (!is_console) {
                std::string line;
                if (!std::getline(std::cin, line)) {
                    edit.buffer = ":quit";
                    line_done = true;
                    break;
                }
                edit.buffer = line;
                line_done = true;
                break;
            }
            ConsoleInput console;
            auto key = console.read_key();

            if (key.type == ConsoleInput::KeyResult::Eof) {
                std::cout << "\n";
                line_done = true;
                edit.buffer = ":quit";
                break;
            }

            if (key.type == ConsoleInput::KeyResult::Char) {
                edit.insert(key.ch);
                std::cout << key.ch;
                if (edit.cursor < edit.buffer.size()) {
                    std::cout << edit.buffer.substr(edit.cursor);
                    std::cout << "\x1b[" << (edit.buffer.size() - edit.cursor) << "D";
                }
                std::cout.flush();
            } else if (key.type == ConsoleInput::KeyResult::Special) {
                switch (key.code) {
                case VK_UP:
                    edit.buffer = history.prev(edit.history_pos);
                    edit.cursor = edit.buffer.size();
                    std::cout << "\r" << std::string(80, ' ') << "\r";
                    std::cout << (edit.buffer.empty() ? "next11> " : "...> ") << edit.buffer;
                    std::cout.flush();
                    break;
                case VK_DOWN:
                    edit.buffer = history.next(edit.history_pos);
                    edit.cursor = edit.buffer.size();
                    std::cout << "\r" << std::string(80, ' ') << "\r";
                    std::cout << (edit.buffer.empty() ? "next11> " : "...> ") << edit.buffer;
                    std::cout.flush();
                    break;
                case VK_LEFT:
                    edit.move_left();
                    std::cout << "\x1b[D";
                    std::cout.flush();
                    break;
                case VK_RIGHT:
                    edit.move_right();
                    std::cout << "\x1b[C";
                    std::cout.flush();
                    break;
                case VK_HOME:
                    edit.move_home();
                    std::cout << "\r" << std::string(80, ' ') << "\r";
                    std::cout << (edit.buffer.empty() ? "next11> " : "...> ") << edit.buffer;
                    if (edit.cursor < edit.buffer.size()) {
                        std::cout << "\x1b[" << (edit.buffer.size() - edit.cursor) << "D";
                    }
                    std::cout.flush();
                    break;
                case VK_END:
                    edit.move_end();
                    std::cout << "\r" << std::string(80, ' ') << "\r";
                    std::cout << (edit.buffer.empty() ? "next11> " : "...> ") << edit.buffer;
                    std::cout.flush();
                    break;
                case VK_TAB: {
                    // 自动补全
                    std::string prefix = edit.buffer;
                    auto matches = completer.complete(prefix);
                    if (!matches.empty()) {
                        if (matches.size() == 1) {
                            edit.set(matches[0]);
                            std::cout << "\r" << std::string(80, ' ') << "\r";
                            std::cout << (edit.buffer.empty() ? "next11> " : "...> ") << edit.buffer;
                            std::cout.flush();
                        } else {
                            std::cout << "\n";
                            for (size_t i = 0; i < matches.size(); ++i) {
                                if (i > 0 && i % 4 == 0) std::cout << "\n";
                                std::cout << matches[i] << "\t";
                            }
                            std::cout << "\n";
                            std::cout << (edit.buffer.empty() ? "next11> " : "...> ") << edit.buffer;
                            std::cout.flush();
                        }
                    }
                    break;
                }
                case VK_ESCAPE:
                    edit.clear();
                    std::cout << "\r" << std::string(80, ' ') << "\r";
                    std::cout << (edit.buffer.empty() ? "next11> " : "...> ");
                    std::cout.flush();
                    break;
                case VK_RETURN:
                    line_done = true;
                    break;
                case VK_BACK:
                    edit.backspace();
                    std::cout << "\b";
                    if (edit.cursor < edit.buffer.size()) {
                        std::cout << edit.buffer.substr(edit.cursor) << " ";
                        std::cout << "\x1b[" << (edit.buffer.size() - edit.cursor + 1) << "D";
                    }
                    std::cout.flush();
                    break;
                }
            }
#else
            // 非 Windows 平台的简单输入
            std::string line;
            if (!std::getline(std::cin, line)) break;
            edit.buffer = line;
            line_done = true;
#endif
        }

        if (edit.buffer.empty()) continue;

        // 检查 REPL 命令（支持带冒号和不带冒号两种形式）
        if (edit.buffer == ":q" || edit.buffer == ":quit" ||
            edit.buffer == "exit" || edit.buffer == "quit") {
            break;
        }
        if (edit.buffer == ":h" || edit.buffer == ":help" || edit.buffer == "help") {
            std::cout << "REPL 命令:\n";
            std::cout << ":quit, :q, quit, exit  - 退出 REPL\n";
            std::cout << ":help, :h, help        - 显示此帮助\n";
            std::cout << ":save                    - 保存会话\n";
            std::cout << ":load                    - 加载会话\n";
            std::cout << "\n快捷键:\n";
            std::cout << "上/下箭头 - 浏览历史\n";
            std::cout << "Tab         - 自动补全\n";
            std::cout << "Esc         - 清空当前行\n";
            edit.clear();
            continue;
        }
        if (edit.buffer == ":save") {
            std::vector<std::string> lines;
            std::istringstream iss(accumulated_history);
            std::string line;
            while (std::getline(iss, line)) {
                if (!line.empty()) lines.push_back(line);
            }
            session.save(".next11_session", lines);
            std::cout << "会话已保存到 .next11_session (" << lines.size() << " 条声明)\n";
            edit.clear();
            continue;
        }
        if (edit.buffer == ":load") {
            session_lines = session.load(".next11_session");
            std::cout << "已加载 " << session_lines.size() << " 条声明\n";
            edit.clear();
            continue;
        }

        // 添加到历史记录
        history.push(edit.buffer);
        history.save(".next11_history");

        // 更新变量列表用于补全（简化：从累积历史中提取）
        // 实际实现需要从解释器获取变量列表

        // 处理多行输入
        if (edit.buffer.empty()) {
            edit.clear();
            continue;
        }

        // 将历史声明与新输入拼接，整体求值
        std::string full = accumulated_history + edit.buffer + "\n";
        auto [result, success] = pipe.eval_line(full);

        if (!result.empty()) {
            std::cout << result;
            if (result.back() != '\n') std::cout << "\n";
        }

        // 仅在执行成功且为声明语句时追加到历史，避免表达式副作用重复执行
        if (success) {
            std::string trimmed = edit.buffer;
            size_t start = trimmed.find_first_not_of(" \t\n\r");
            if (start != std::string::npos) {
                std::string rest = trimmed.substr(start);
                if (rest.starts_with("let ") || rest.starts_with("fn ") || rest.starts_with("def ") ||
                    rest.starts_with("struct ") || rest.starts_with("enum ") || rest.starts_with("type ") ||
                    rest.starts_with("import ") || rest.starts_with("class ")) {
                    accumulated_history += edit.buffer + "\n";
                }
            }
        }
        edit.clear();
    }

    // 保存历史记录
    history.save(".next11_history");

    return 0;
}

} // namespace next11