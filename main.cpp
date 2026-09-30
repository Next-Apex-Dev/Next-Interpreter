// Next1.1 解释器入口
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include "interp/ide_server.hpp"
#endif
#include "driver/cli_driver.hpp"
#include "common/registry.hpp"
#include "stdlib/stdlib.hpp"

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    bool want_ide = false;
    bool has_file = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ide" || arg == "-ide") want_ide = true;
        else if (arg.size() > 0 && arg[0] != '-') has_file = true;
    }

    if (want_ide || (!has_file && argc <= 1)) {
#ifdef _WIN32
        next11::IdeServer::instance().start();
        while (true) { Sleep(1000); }
        return 0;
#else
        std::cerr << "IDE 仅支持 Windows\n";
        return 1;
#endif
    }

    next11::register_all_defaults();
    next11::StdLibRegistry::register_all();
    next11::CliDriver driver;
    return driver.run(argc, argv);
}
