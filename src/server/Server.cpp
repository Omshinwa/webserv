#include "Server.hpp"

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "Connexion.hpp"
#include "Log.hpp"
#include "common.h"

std::ostream &operator<<(std::ostream &os, const sockaddr_in &addr)
{
    uint32_t ip = ntohl(addr.sin_addr.s_addr); // host byte order
    os << "    Address:";
    os << ((ip >> 24) & 0xFF) << "." << ((ip >> 16) & 0xFF) << "."
       << ((ip >> 8) & 0xFF) << "." << (ip & 0xFF) << "\n";
    os << "    Port:   " << ntohs(addr.sin_port) << "\n";
    return os;
}

Server::Server(int port)
    : _fd(-1)
    , _port(port)
{
    _setup_listening_socket();

    pollfd listen_pfd;
    listen_pfd.fd = _fd;
    listen_pfd.events = POLLIN;
    listen_pfd.revents = 0;
    _pollfds.push_back(listen_pfd);
}

Server::~Server()
{
    Log::debug("~Destructor Server fd " + to_string(_fd));
    for (std::map<int, Connexion *>::iterator it = _connexions.begin();
        it != _connexions.end(); ++it) {
        delete it->second; // destructor closes fd
    }
    if (_fd >= 0)
        close(_fd);
}

// create socket -> setsockopt -> nonblock -> bind -> listen
void Server::_setup_listening_socket()
{
    // AF_INET: IPv4 // AF_INET6: IPv6 // AF_UNIX: local Unix domain socket
    _fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_fd == -1) {
        std::cerr << Log::red_bg << "socket error" << Log::nl;
        throw std::runtime_error(std::strerror(errno));
    }
    std::cout << Log::b(_fd) << "NEW Server Socket FD: " << _fd << Log::nl;

    // Allows to restart without TIME_WAIT
    int opt = 1;
    setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    fcntl(_fd, F_SETFL, O_NONBLOCK);

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr)); // zero — there are padding fields
    addr.sin_family = AF_INET; // must match socket()'s domain
    addr.sin_port = htons(_port); // network byte order
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // listen on all interfaces

    if (bind(_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        std::cerr << Log::red_bg << "bind error" << Log::nl;
        throw std::runtime_error(std::strerror(errno));
    }
    std::cout << "Listening to: \n" << addr;

    const int MAX_CONNEXION = 10;
    if (listen(_fd, MAX_CONNEXION) < 0) {
        std::cerr << Log::red_bg << "listen error" << Log::nl;
        throw std::runtime_error(std::strerror(errno));
    }
}

// poll uses events and revents:
// events  = your question  → "is this fd readable? writable?"
// revents = the answer     → "yes readable / yes writable / hung up / error"
void Server::run()
{
    std::cout << Log::white_bg << "SERVER START" << Log::nl;
    while (true) {
        int n = poll(&_pollfds[0], _pollfds.size(), -1); // -1 = wait forever
        if (n < 0) {
            if (errno == EINTR) // signal interrupted, just retry
                continue;
            std::cerr << Log::red_bg << "poll error" << Log::nl;
            throw std::runtime_error(std::strerror(errno));
        }

        for (size_t i = 0; i < _pollfds.size(); i++) {
            if (_pollfds[i].revents == 0)
                continue;
            _handle_event(i);
        }
    }
}

void Server::_handle_event(size_t &i)
{
    pollfd &pfd = _pollfds[i];

    // 1. Errors first
    if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
        if (pfd.fd == _fd) {
            std::cerr << Log::red_bg << "server fatal error" << Log::nl;
            throw std::runtime_error("listening socket failed");
        }
        _drop(i);
        i--;
        return;
    }

    // 2. Reads
    if (pfd.revents & POLLIN) {
        if (pfd.fd == _fd) {
            _accept_new();
        } else {
            Connexion *c = _connexions[pfd.fd];
            if (c->do_recv() <= 0) {
                _drop(i);
                i--;
                return;
            }
            // request received → build response, switch to write
            c->queue_response(_build_response());
            pfd.events = POLLOUT;
        }
    }

    // 3. Writes
    if (pfd.revents & POLLOUT) {
        Connexion *c = _connexions[pfd.fd];
        if (c->do_send() < 0) {
            _drop(i);
            i--;
            return;
        }
        if (c->state() == Connexion::CLOSING || !USE_HTTP_1_1) {
            _drop(i);
            i--;
            return;
        }
        // HTTP/1.1: response sent but keep-alive — flip back to reading
        if (USE_HTTP_1_1)
            pfd.events = POLLIN;
    }
}

void Server::_accept_new()
{
    sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int client_fd = accept(_fd, (struct sockaddr *)&addr, &addrlen);
    if (client_fd < 0) {
        std::cerr << Log::red_bg << "accept error: " << std::strerror(errno)
                  << Log::nl;
        return;
    }

    _connexions[client_fd] = new Connexion(client_fd, addr);

    pollfd new_pfd;
    new_pfd.fd = client_fd;
    new_pfd.events = POLLIN;
    new_pfd.revents = 0;
    _pollfds.push_back(new_pfd);
}

void Server::_drop(size_t idx)
{
    int fd = _pollfds[idx].fd;
    std::map<int, Connexion *>::iterator it = _connexions.find(fd);
    if (it != _connexions.end()) {
        delete it->second; // destructor closes the fd
        _connexions.erase(it);
    } else {
        close(fd);
    }
    _pollfds.erase(_pollfds.begin() + idx);
}

std::string Server::_build_response()
{
    const std::string filepath = "./www/index.html";
    std::ifstream file(filepath.c_str(), std::ios::binary);
    if (!file.is_open()) {
        std::string body = "<h1>500</h1><p>Failed to open local file.</p>\n";
        std::ostringstream resp;
        resp << "HTTP/1.1 500 Internal Server Error\r\n"
             << "Content-Type: text/html\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << body;
        Log::error("failed to open local file:");
        Log::error(std::strerror(errno));
        return resp.str();
    }

    std::ostringstream body_stream;
    body_stream << file.rdbuf();
    std::string body = body_stream.str();

    std::ostringstream resp;
    resp << "HTTP/1.1 200 OK\r\n"
         << "Content-Type: text/html\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Connection: close\r\n"
         << "\r\n"
         << body;
    std::cout << "Successfully built response from `" << filepath << "`\n";
    return resp.str();
}
