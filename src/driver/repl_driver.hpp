// REPL 驱动
#pragma once
#include <string>
#include "common/logger.hpp"

namespace next11 {

class ReplDriver {
public:
    ReplDriver(Logger& logger) : _logger(logger) {}
    int run();

private:
    Logger& _logger;
    bool braces_balanced(const std::string& s);
};

} // namespace next11