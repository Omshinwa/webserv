#ifndef SERVER_HPP
#define SERVER_HPP

#include <netinet/in.h>
#include <poll.h>

#include <map>
#include <string>
#include <vector>

#include "../config/Config.hpp"
#include "../event/IEventHandler.hpp"

class Connection;

class Server {
    public:
        Server(const std::vector<ServerConfig>& configs);
        ~Server();

        void on_readable(int fd);
        void on_writable(int fd);
        void on_tick(time_t now);

        int fd;

    private:
        const std::vector<ServerConfig>& configs;
        void accept_new_connection(int listen_fd);
        void create_socket(const std::string& host, int port);
        // LOG
        void log_debug(std::string s);
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
