#include "Server.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include "../utils/Log.hpp"
#include "../utils/Utils.hpp"
#include "Connexion.hpp"
#include "signal.hpp"

// Server::Server(const std::vector<ServerConfig>& configs) : _configs(configs) {}

std::ostream& operator<<(std::ostream& os, const sockaddr_in& addr);

// Group the configs by host:port; each unique pair gets one listening socket,
// and the configs sharing it become its virtual hosts.
Server::Server(const std::vector<ServerConfig>& configs) {
    std::map<std::string, std::vector<ServerConfig> > groups;
    for (size_t i = 0; i < configs.size(); i++) {
        std::string key = configs[i].host + ":" + utils::to_str(configs[i].port);
        groups[key].push_back(configs[i]);
    }

    for (std::map<std::string, std::vector<ServerConfig> >::iterator it = groups.begin();
         it != groups.end(); ++it) {
        const ServerConfig& first = it->second[0];
        int fd = create_socket(first.host, first.port);
        _listeners[fd] = it->second;
        append_to_poll(fd);
    }
}

// create socket -> setsockopt -> nonblock -> bind -> listen, returns the listen fd
int Server::create_socket(const std::string& host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        Log::error("Server socket error");
        throw std::runtime_error(std::strerror(errno));
    }
    log_event("NEW Server Socket FD: " + utils::to_str(fd));

    int opt = 1;  // Allows to restart without TIME_WAIT
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    fcntl(fd, F_SETFL, O_NONBLOCK);  // non blocking for macos

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
    std::cout << "Listening to: \n" << addr;

    const int MAX_CONNEXION = 10;
    if (listen(fd, MAX_CONNEXION) < 0) {
        close(fd);
        std::cerr << Log::red_bg << "listen error" << Log::nl;
        throw std::runtime_error(std::strerror(errno));
    }

    return fd;
}

void Server::append_to_poll(int fd) {
    pollfd polling_req;
    polling_req.fd = fd;
    polling_req.events = POLLIN;
    polling_req.revents = 0;
    _pollfds.push_back(polling_req);
}

Server::~Server() {
    Log::debug("~Destructor Server");
    for (std::map<int, Connexion*>::iterator it = _connexions.begin();
         it != _connexions.end(); ++it) {
        delete it->second;  // destructor closes fd
    }
    for (std::map<int, std::vector<ServerConfig> >::iterator it = _listeners.begin();
         it != _listeners.end(); ++it) {
        close(it->first);
    }
}

// poll uses events and revents:
// events  = your question  → "is this fd readable? writable?"
// revents = the answer     → "yes readable / yes writable / hung up / error"
void Server::run() {
    log_event("SERVER RUNNING");
    while (!webserv::g_stop) {
        // poll will set all the revents to 0, then
        // poll will BLOCK the process until an event triggers it
        int n = poll(&_pollfds[0], _pollfds.size(), -1);
        if (n < 0) {
            if (errno == EINTR)  // signal interrupted
            {
                log_error("SIGNAL INTERRUPT");
                continue;
            }
            log_error("poll error");
            throw std::runtime_error(std::strerror(errno));
        }

        // goes through polls, handle events
        for (size_t i = 0; i < _pollfds.size(); i++) {
            if (_pollfds[i].revents == 0) continue;
            handle_event(_pollfds[i]);
        }

        // goes through connexions, clean them if their state is 'CLOSING',
        for (std::map<int, Connexion*>::iterator it = _connexions.begin();
             it != _connexions.end();) {
            Connexion* c = it->second;
            ++it;  // advance before drop_connexion() invalidates the current
                   // iterator
            if (c->state() == Connexion::CLOSING) drop_connexion(c);
        }
    }
    log_event("SHUTTING DOWN");
}

//  ██████████ █████   █████ ██████████ ██████   █████ ███████████
// ▒▒███▒▒▒▒▒█▒▒███   ▒▒███ ▒▒███▒▒▒▒▒█▒▒██████ ▒▒███ ▒█▒▒▒███▒▒▒█
//  ▒███  █ ▒  ▒███    ▒███  ▒███  █ ▒  ▒███▒███ ▒███ ▒   ▒███  ▒
//  ▒██████    ▒███    ▒███  ▒██████    ▒███▒▒███▒███     ▒███
//  ▒███▒▒█    ▒▒███   ███   ▒███▒▒█    ▒███ ▒▒██████     ▒███
//  ▒███ ▒   █  ▒▒▒█████▒    ▒███ ▒   █ ▒███  ▒▒█████     ▒███
//  ██████████    ▒▒███      ██████████ █████  ▒▒█████    █████
// ▒▒▒▒▒▒▒▒▒▒      ▒▒▒      ▒▒▒▒▒▒▒▒▒▒ ▒▒▒▒▒    ▒▒▒▒▒    ▒▒▒▒▒

//  █████   █████                          █████ ████
// ▒▒███   ▒▒███                          ▒▒███ ▒▒███
//  ▒███    ▒███   ██████   ████████    ███████  ▒███   ██████
//  ▒███████████  ▒▒▒▒▒███ ▒▒███▒▒███  ███▒▒███  ▒███  ███▒▒███
//  ▒███▒▒▒▒▒███   ███████  ▒███ ▒███ ▒███ ▒███  ▒███ ▒███████
//  ▒███    ▒███  ███▒▒███  ▒███ ▒███ ▒███ ▒███  ▒███ ▒███▒▒▒
//  █████   █████▒▒████████ ████ █████▒▒████████ █████▒▒██████
// ▒▒▒▒▒   ▒▒▒▒▒  ▒▒▒▒▒▒▒▒ ▒▒▒▒ ▒▒▒▒▒  ▒▒▒▒▒▒▒▒ ▒▒▒▒▒  ▒▒▒▒▒▒
//
//

void Server::handle_event(pollfd& pfd) {
    // 1. Handle Errors
    if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
        if (_listeners.count(pfd.fd)) {  // the poll is a listening socket
            log_error("server fatal error");
            throw std::runtime_error("listening socket failed");
        }
        _connexions[pfd.fd]->mark_closing();
        return;
    }

    // 2. Reads
    if (pfd.revents & POLLIN) {
        if (_listeners.count(pfd.fd)) {  // the poll is a listening socket
            accept_new_connexion(pfd.fd);
        } else {
            Connexion* c = _connexions[pfd.fd];
            c->do_recv();
            if (c->state() == Connexion::CLOSING) return;
            pfd.events = POLLOUT;
            // request received → build response, switch to write
        }
    }

    // 3. Writes
    if (pfd.revents & POLLOUT) {
        Connexion* c = _connexions[pfd.fd];
        if (c->do_send() < 0) c->mark_closing();
        if (c->state() == Connexion::CLOSING) return;
        // HTTP/1.1: response sent but keep-alive — flip back to reading
        pfd.events = POLLIN;
    }
}

// Construct a Connexion and register it if no error
void Server::accept_new_connexion(int listen_fd) {
    Connexion* c;

    try {
        sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int connexion_fd = accept(listen_fd, (struct sockaddr*)&addr, &len);
        if (connexion_fd < 0) throw std::runtime_error(std::strerror(errno));
        c = new Connexion(connexion_fd, _listeners[listen_fd]);

        uint32_t ip = ntohl(addr.sin_addr.s_addr);  // host byte order
        c->remote_addr = utils::to_str((ip >> 24) & 0xFF) + "." +
                         utils::to_str((ip >> 16) & 0xFF) + "." +
                         utils::to_str((ip >> 8) & 0xFF) + "." + utils::to_str(ip & 0xFF);
        log_event("NEW Client Socket FD: " + utils::to_str(c->fd));
    } catch (const std::exception& e) {
        log_error(std::string("accept error: ") + e.what());
        return;
    }

    // add it to the Connexions map and the polls vector
    _connexions[c->fd] = c;
    append_to_poll(c->fd);
}

// Remove the Connexion from the list of connexions, deletes it (close the fd)
// and remove the associated poll_fd from the vector.
void Server::drop_connexion(Connexion* c) {
    int fd = c->fd;
    _connexions.erase(fd);
    log_event("CLOSED Client Socket FD: " + utils::to_str(c->fd));
    delete c;  // destructor closes the fd
    for (size_t i = 0; i < _pollfds.size(); i++) {
        if (_pollfds[i].fd == fd) {
            _pollfds.erase(_pollfds.begin() + i);
            break;
        }
    }
}

//  ██████████   ██████████ ███████████  █████  █████   █████████
// ▒▒███▒▒▒▒███ ▒▒███▒▒▒▒▒█▒▒███▒▒▒▒▒███▒▒███  ▒▒███   ███▒▒▒▒▒███
//  ▒███   ▒▒███ ▒███  █ ▒  ▒███    ▒███ ▒███   ▒███  ███     ▒▒▒
//  ▒███    ▒███ ▒██████    ▒██████████  ▒███   ▒███ ▒███
//  ▒███    ▒███ ▒███▒▒█    ▒███▒▒▒▒▒███ ▒███   ▒███ ▒███    █████
//  ▒███    ███  ▒███ ▒   █ ▒███    ▒███ ▒███   ▒███ ▒▒███  ▒▒███
//  ██████████   ██████████ ███████████  ▒▒████████   ▒▒█████████
// ▒▒▒▒▒▒▒▒▒▒   ▒▒▒▒▒▒▒▒▒▒ ▒▒▒▒▒▒▒▒▒▒▒    ▒▒▒▒▒▒▒▒     ▒▒▒▒▒▒▒▒▒
//
//

void Server::log_info(std::string s) { std::cout << Log::color(0) << s << Log::nl; }

void Server::log_event(std::string s) {
    std::cout << Log::background(0) << s << Log::nl;
}

void Server::log_error(std::string s) { Log::error(s); }

void Server::log_debug(std::string s) { Log::debug(s); }

std::ostream& operator<<(std::ostream& os, const sockaddr_in& addr) {
    uint32_t ip = ntohl(addr.sin_addr.s_addr);  // host byte order
    os << "    Address:";
    os << ((ip >> 24) & 0xFF) << "." << ((ip >> 16) & 0xFF) << "." << ((ip >> 8) & 0xFF)
       << "." << (ip & 0xFF) << "\n";
    os << "    Port:   " << ntohs(addr.sin_port) << "\n";
    return os;
}
