// 命令行驱动
#pragma once
#include <string>
#include "pipeline.hpp"

namespace next11 {

class CliDriver {
public:
    int run(int argc, char** argv);

private:
    void print_help();
};

} // namespace next11