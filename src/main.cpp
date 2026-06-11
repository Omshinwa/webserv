// #include "config/Config.hpp"
#include <iostream>

#include "server/Server.hpp"
#include "utils/Log.hpp"

int main() {
    try {
        Server server;
        server.run();
    } catch (const std::exception& e) {
        Log::error(e.what());
        return 1;
    }
    return 0;
}
