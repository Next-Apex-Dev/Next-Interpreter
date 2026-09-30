// 分级日志模块 - ERROR/WARN/INFO/DEBUG 四级过滤
#pragma once
#include <iostream>
#include <string_view>
#include <mutex>

namespace next11 {

enum class LogLevel { Error = 0, Warn = 1, Info = 2, Debug = 3 };

class Logger {
public:
    LogLevel _level = LogLevel::Info;
    std::ostream* _out = &std::cerr;

    void set_level(LogLevel level) { std::lock_guard<std::mutex> lock(_mutex); _level = level; }
    void set_stream(std::ostream& os) { std::lock_guard<std::mutex> lock(_mutex); _out = &os; }

    void error(std::string_view msg) { log(LogLevel::Error, "ERROR", msg); }
    void warn(std::string_view msg) { log(LogLevel::Warn, "WARN", msg); }
    void info(std::string_view msg) { log(LogLevel::Info, "INFO", msg); }
    void debug(std::string_view msg) { log(LogLevel::Debug, "DEBUG", msg); }

private:
    void log(LogLevel level, const char* tag, std::string_view msg) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (static_cast<int>(level) > static_cast<int>(_level)) return;
        (*_out) << "[" << tag << "] " << msg << std::endl;
    }
    std::mutex _mutex;
};

} // namespace next11