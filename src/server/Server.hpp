#ifndef ACCEPTOR_HPP
#define ACCEPTOR_HPP

#include <netinet/in.h>
#include <poll.h>

#include <map>
#include <string>
#include <vector>

#include "../config/Config.hpp"
#include "IEventHandler.hpp"

class Connexion;

class Acceptor {
    public:
        Acceptor(const std::vector<ServerConfig>& configs);
        ~Acceptor();

        void run();  // main poll loop, blocks forever

    private:
        // listen_fd -> the server configs sharing that host:port (virtual hosts)
        std::map<int, std::vector<ServerConfig> > _listeners;
        std::vector<pollfd> _pollfds;           // list of all the poll requests
        std::map<int, Connexion*> _connexions;  // int fd -> Connexion*
        std::map<int, Connexion*> is_cgi;

        std::map<int, IEventHandler*> _handlers;

        static int create_socket(const std::string& host, int port);
        void append_to_poll(int fd);
        void accept_new_connexion(int listen_fd);
        void drop_connexion(Connexion* c);
        void handle_event(pollfd& pfd);

        // LOG
        void log_debug(std::string s);
        void log_info(std::string s);
        void log_event(std::string s);
        void log_error(std::string s);

        // INNACCESSIBLE
        Acceptor();
        Acceptor(const Acceptor&);
        Acceptor& operator=(const Acceptor&);
};

std::ostream& operator<<(std::ostream& os, const sockaddr_in& addr);

#endif
