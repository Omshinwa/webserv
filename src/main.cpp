#include <iostream>

#include "server/Server.hpp"
#include "utils/Log.hpp"

static const char*  DEFAULT_CONFIG = "configs/default.conf";

int main(int ac, char** av) {
    std::string config_path = DEFAULT_CONFIG;

    if (ac > 2) {
        std::cerr << "Usage: " << av[0] << " [config_file]" << std::endl;
        return 1;
    }

    if (ac == 2)
        config_path = av[1];

    try {
        Server server;
        server.run();
    }
    catch (const std::exception& e) {
        Log::error(e.what());
        return 1;
    }
    return 0;
}
