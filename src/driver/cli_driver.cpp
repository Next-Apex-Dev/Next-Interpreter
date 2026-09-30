// 命令行驱动实现
#include "cli_driver.hpp"
#include "repl_driver.hpp"
#include <iostream>

namespace next11 {

void CliDriver::print_help() {
    std::cout << "Next1.1 解释器\n"
              << "用法: next11 [子命令] [选项] <文件>\n\n"
              << "子命令:\n"
              << "  run <文件>    执行源文件\n"
              << "  repl          交互式 REPL\n\n"
              << "选项:\n"
              << "  --lex-only     仅执行词法分析并输出 Token 流\n"
              << "  --parse-only   仅执行语法分析并输出 AST\n"
              << "  --semant-only  仅执行语义分析并输出符号表\n"
              << "  --debug        输出调试信息\n"
              << "  --log-level <level>  日志级别 (error/warn/info/debug)\n"
              << "  --help         显示帮助\n\n"
              << "退出码:\n"
              << "  0 成功  1 词法错误  2 语法错误  3 语义错误  4 运行期错误\n";
}

int CliDriver::run(int argc, char** argv) {
    Logger logger;
    logger.set_level(LogLevel::Info);

    PipelineOptions opts;
    bool show_help = false;
    std::string subcommand;

    int i = 1;
    while (i < argc) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            show_help = true;
        } else if (arg == "run") {
            subcommand = "run";
        } else if (arg == "repl") {
            opts.repl_mode = true;
            subcommand = "repl";
        } else if (arg == "--lex-only") {
            opts.stop_at = Stage::Lex;
        } else if (arg == "--parse-only") {
            opts.stop_at = Stage::Parse;
        } else if (arg == "--semant-only") {
            opts.stop_at = Stage::Semant;
        } else if (arg == "--debug") {
            opts.debug = true;
            logger.set_level(LogLevel::Debug);
        } else if (arg == "--log-level") {
            if (i + 1 >= argc) {
                std::cerr << "命令行错误 CLI001: --log-level 需要参数 (error/warn/info/debug)\n";
                return 1;
            }
            std::string lvl = argv[++i];
            if (lvl == "error") logger.set_level(LogLevel::Error);
            else if (lvl == "warn") logger.set_level(LogLevel::Warn);
            else if (lvl == "info") logger.set_level(LogLevel::Info);
            else if (lvl == "debug") logger.set_level(LogLevel::Debug);
            else {
                std::cerr << "命令行错误 CLI002: 无效日志级别 '" << lvl << "'，可选: error/warn/info/debug\n";
                return 1;
            }
        } else if (arg.size() > 0 && arg[0] == '-') {
            std::cerr << "命令行错误 CLI003: 未知选项 '" << arg << "'，使用 next11 --help 查看用法\n";
            return 1;
        } else {
            if (opts.source_file.empty()) opts.source_file = arg;
        }
        ++i;
    }

    if (show_help) {
        print_help();
        return 0;
    }

    if (opts.repl_mode) {
        ReplDriver repl(logger);
        return repl.run();
    }

    if (opts.source_file.empty() && subcommand.empty()) {
        // 无参数进入 REPL
        ReplDriver repl(logger);
        return repl.run();
    }

    Pipeline pipe(logger);
    return pipe.run(opts);
}

} // namespace next11