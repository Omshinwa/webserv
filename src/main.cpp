#include <iostream>

#include "config/Config.hpp"
#include "server/Server.hpp"
#include "server/signal.hpp"
#include "utils/Log.hpp"

int main(int ac, char** av) {
    try {
        std::string path = (ac == 2) ? av[1] : "configs/default.conf";
        std::vector<ServerConfig> configs = Config::parse(path);
        Server server(configs);
        webserv::setup_signals();
        server.run();
    } catch (const std::exception& e) {
        Log::error(e.what());
        return 1;
    }
    return 0;
}
