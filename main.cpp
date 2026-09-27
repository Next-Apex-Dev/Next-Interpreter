// Next1.1 解释器入口
#include "driver/cli_driver.hpp"
#include "common/registry.hpp"
#include "stdlib/stdlib.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    next11::register_all_defaults();
    next11::StdLibRegistry::register_all();
    next11::CliDriver driver;
    return driver.run(argc, argv);
}