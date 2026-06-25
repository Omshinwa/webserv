#ifndef SERVER_HPP
#define SERVER_HPP

#include <netinet/in.h>
#include <poll.h>

#include <string>
#include <vector>

#include "../config/Config.hpp"
#include "../event/AEventHandler.hpp"

class Connection;

class Server : public AEventHandler {
    public:
        Server(Reactor& reactor, const std::vector<ServerConfig>& configs);
        ~Server();

        int fd;
        void on_readable();
        void on_writable();
        void on_tick(time_t now);

    private:
        const std::vector<ServerConfig>& configs;
        void accept_new_connection(int listen_fd);
        void create_socket(const std::string& host, int port);
        // LOG
        void log_info(std::string s);
        void log_event(std::string s);
        void log_error(std::string s);

        // INNACCESSIBLE
        Server();
        Server(const Server&);
        Server& operator=(const Server&);
};

std::ostream& operator<<(std::ostream& os, const sockaddr_in& addr);

#endif
