#include "Server.hpp"
#include "colorC.hpp"
#include "common.h"

int main()
{
    std::cout << colorC::white_bg << "SERVER START" << colorC::nl;
    try {
        Server server(8080);
        server.run();
    } catch (const std::exception &e) {
        colorC::print_err(e.what());
        return 1;
    }
    std::cout << colorC::white_bg << "PROGRAM END" << colorC::nl;
    return 0;
}
