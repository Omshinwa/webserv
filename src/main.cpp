#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ── POSIX / system ──────────────────────────────────────────────────────────
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// ── project ─────────────────────────────────────────────────────────────────

#include "utils/Log.hpp"
#include "utils/Utils.hpp"
//
#include "config/Config.hpp"
//

#include "event/EventLoop.hpp"
#include "event/AEventHandler.hpp"
//
#include "cgi/CgiHandler.hpp"
#include "cgi/CgiProcess.hpp"
#include "http/RequestParser.hpp"
#include "http/ResponseBuilder.hpp"
#include "server/Connection.hpp"
#include "server/Server.hpp"

int main(int ac, char** av) {
    if (ac > 2) {
        std::cerr << "Usage: " << av[0] << " [config_file]" << std::endl;
        return 1;
    }

    try {
        std::string path = (ac == 2) ? av[1] : "configs/default.conf";
        std::vector<ServerConfig> configs = Config::parse(path);

        // One listening socket per unique host:port. `groups` must outlive the
        // Servers: each Server keeps a reference into it for virtual-host lookup.
        // The Servers are owned by the loop (registered with owned=true) and freed
        // in ~EventLoop. event_loop is declared *after* groups, so it is destroyed
        // first — the Servers die before the groups they reference. Keep that order.
        std::map<std::string, std::vector<ServerConfig> > groups =
                Config::group_by_host_port(configs);
        EventLoop event_loop;

        for (std::map<std::string, std::vector<ServerConfig> >::iterator it =
                     groups.begin();
             it != groups.end(); ++it) {
            Server* s = new Server(event_loop, it->second);
            event_loop.register_fd(s->fd, POLLIN, s, true);
        }

        event_loop.run();
    } catch (const std::exception& e) {
        Log::error(e.what());
        return 1;
    }
    return 0;
}
