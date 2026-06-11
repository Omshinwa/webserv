#include "Log.hpp"
#include "Server.hpp"
#include "common.h"

int main()
{
    try {
        Server server;
        server.run();
    } catch (const std::exception &e) {
        Log::error(e.what());
        return 1;
    }
    return 0;
}
