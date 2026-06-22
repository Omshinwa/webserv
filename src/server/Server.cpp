#include "Server.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <iostream>
#include <stdexcept>

#include "../event/EventLoop.hpp"
#include "../event/IEventHandler.hpp"
#include "../utils/Log.hpp"
#include "../utils/Utils.hpp"
#include "Connection.hpp"

// One Server owns one listening socket. `configs` are the server blocks that
// share this host:port — i.e. the virtual hosts reachable through this socket.
// The caller (group_by_host_port) guarantees they all share host:port, so the
// first one defines the endpoint to bind.
Server::Server(EventLoop& event_loop, const std::vector<ServerConfig>& configs)
        : IEventHandler(event_loop), configs(configs) {
    const ServerConfig& first = configs[0];
    create_socket(first.host, first.port);
    log_event("Server Listening to: " + first.host + ":" + utils::to_str(first.port) +
              ", fd:" + utils::to_str(fd));
}

// create socket -> nonblock -> bind -> listen
// also sets the fd
void Server::create_socket(const std::string& host, int port) {
    // create the socket
    {
        this->fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1) {
            Log::error("Server socket error");
            throw std::runtime_error(std::strerror(errno));
        }

        int opt = 1;  // Allows to restart without TIME_WAIT
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        // The fd is made non-blocking when the loop registers it
        // (EventLoop::register_fd), before any accept() can run.
    }

    // binds it
    {
        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));  // zero — there are padding fields
        addr.sin_family = AF_INET;            // must match socket()'s domain
        addr.sin_port = htons(port);          // network byte order
        if (host.empty() || host == "0.0.0.0")
            addr.sin_addr.s_addr = htonl(INADDR_ANY);  // all interfaces
        else
            addr.sin_addr.s_addr = inet_addr(host.c_str());  // specific IP

        if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            close(fd);
            std::cerr << Log::red_bg << "bind error" << Log::nl;
            throw std::runtime_error(std::strerror(errno));
        }
    }

    // listen
    {
        const int MAX_CONNECTION = 10;
        if (listen(fd, MAX_CONNECTION) < 0) {
            close(fd);
            std::cerr << Log::red_bg << "listen error" << Log::nl;
            throw std::runtime_error(std::strerror(errno));
        }
    }
}

Server::~Server() {
    close(fd);  // listening fd
}

void Server::on_readable() { accept_new_connection(fd); }
void Server::on_writable() { Log::error("This should never display"); }
void Server::on_tick(time_t now) {
    // A server never hangs up.
    (void)now;
    return;
}

// Construct a Connection and register it if no error
void Server::accept_new_connection(int listen_fd) {
    Connection* c;

    try {
        sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int connection_fd = accept(listen_fd, (struct sockaddr*)&addr, &len);
        if (connection_fd < 0) throw std::runtime_error(std::strerror(errno));
        c = new Connection(event_loop, connection_fd, configs, addr);

        event_loop.register_fd(c->fd, POLLIN, c, true);
        log_event("NEW Connection Socket FD: " + utils::to_str(c->fd));
    } catch (const std::exception& e) {
        log_error(std::string("accept error: ") + e.what());
        return;
    }
}

void Server::log_info(std::string s) { std::cout << Log::color(0) << s << Log::nl; }

void Server::log_error(std::string s) { Log::error(s); }

void Server::log_event(std::string s) {
    // lets do rainbow color
    std::string bg = "\033[48;5;231m";
    std::cout << bg << "[" << Log::timestamp() << "] " << Log::bold;
    for (size_t i = 0; i < s.size(); ++i) {
        std::cout << Log::gradient(i % 32);
        // std::cout << Log::background(i * 3 + 1);
        std::cout << s[i];
    }
    std::cout << Log::nl;
}

std::ostream& operator<<(std::ostream& os, const sockaddr_in& addr) {
    uint32_t ip = ntohl(addr.sin_addr.s_addr);  // host byte order
    os << "    Address:";
    os << ((ip >> 24) & 0xFF) << "." << ((ip >> 16) & 0xFF) << "." << ((ip >> 8) & 0xFF)
       << "." << (ip & 0xFF) << "\n";
    os << "    Port:   " << ntohs(addr.sin_port) << "\n";
    return os;
}
