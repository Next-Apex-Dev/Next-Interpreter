// 阶段流水线编排
#pragma once
#include <string>
#include <string_view>
#include <optional>
#include <iostream>
#include <utility>
#include "common/logger.hpp"
#include "common/diagnostic.hpp"
#include "common/token.hpp"
#include "common/ast.hpp"
#include "common/symbol_table.hpp"

namespace next11 {

enum class Stage { Lex, Parse, Semant, Exec };

struct PipelineOptions {
    std::string source_file;
    bool repl_mode = false;
    std::optional<Stage> stop_at;
    bool debug = false;
    LogLevel log_level = LogLevel::Info;
};

class Pipeline {
public:
    Pipeline(Logger& logger) : _logger(logger) {}

    int run(const PipelineOptions& opts);
    std::pair<std::string, bool> eval_line(std::string_view code);

private:
    Logger& _logger;

    int run_file(const PipelineOptions& opts);
    void print_tokens(const std::vector<Token>& tokens);
    void print_ast(const Program& prog, int indent = 0);
    void print_symbols(const SymbolTable& sym);
};

} // namespace next11