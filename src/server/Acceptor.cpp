#include "Acceptor.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <iostream>
#include <stdexcept>

#include "../utils/Log.hpp"
#include "../utils/Utils.hpp"
#include "Connexion.hpp"
#include "IEventHandler.hpp"
#include "signal.hpp"

namespace {

// How long poll() blocks before returning 0 so we can run housekeeping. A
// finite value (vs. -1 = block forever) guarantees the idle sweep below runs
// even when every connection is silent.
const int POLL_TIMEOUT_MS = 3000;

// Drop a connection after this many seconds with no successful recv/send.
const time_t CONNEXION_TIMEOUT_SEC = 10;

}  // namespace

// Group the configs by host:port; each unique pair gets one listening socket,
// and the configs sharing it become its virtual hosts.
Acceptor::Acceptor(const std::vector<ServerConfig>& configs) {
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

        log_event("Server Listening to: " + first.host + ":" + utils::to_str(first.port) +
                  ", fd:" + utils::to_str(fd));
    }
}

// create socket -> nonblock -> bind -> listen, returns the listen fd
int Acceptor::create_socket(const std::string& host, int port) {
    int fd;
    // create the socket
    {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1) {
            Log::error("Server socket error");
            throw std::runtime_error(std::strerror(errno));
        }

        int opt = 1;  // Allows to restart without TIME_WAIT
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        fcntl(fd, F_SETFL, O_NONBLOCK);  // non blocking for macos
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
        const int MAX_CONNEXION = 10;
        if (listen(fd, MAX_CONNEXION) < 0) {
            close(fd);
            std::cerr << Log::red_bg << "listen error" << Log::nl;
            throw std::runtime_error(std::strerror(errno));
        }
    }
    return fd;
}

Acceptor::~Acceptor() {
    Log::debug("~Destructor Acceptor");
    for (std::map<int, Connexion*>::iterator it = _connexions.begin();
         it != _connexions.end(); ++it) {
        delete it->second;  // destructor closes fd
    }
    for (std::map<int, std::vector<ServerConfig> >::iterator it = _listeners.begin();
         it != _listeners.end(); ++it) {
        close(it->first);
    }
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

void Acceptor::append_to_poll(int fd) {
    pollfd polling_req;
    polling_req.fd = fd;
    polling_req.events = POLLIN;
    polling_req.revents = 0;
    _pollfds.push_back(polling_req);
}

// poll uses events and revents:
// events  = your question  → "is this fd readable? writable?"
// revents = the answer     → "yes readable / yes writable / hung up / error"
void Acceptor::run() {
    log_event("Server running...");
    while (!webserv::g_stop) {
        // poll will set all the revents to 0, then
        // poll will BLOCK the process until an event triggers it
        int n = poll(&_pollfds[0], _pollfds.size(), POLL_TIMEOUT_MS);
        if (n < 0) {
            if (webserv::g_stop)  // signal interrupted
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

        // Idle sweep: mark connections silent for too long as CLOSING.
        time_t now = time(NULL);
        for (std::map<int, Connexion*>::iterator it = _connexions.begin();
             it != _connexions.end(); ++it) {
            Connexion* c = it->second;
            if (c->state() != Connexion::CLOSING &&
                c->timed_out(now, CONNEXION_TIMEOUT_SEC)) {
                log_event("TIMEOUT Connexion Socket FD: " + utils::to_str(c->fd));
                c->mark_closing();
            }
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

void Acceptor::handle_event(pollfd& pfd) {
    IEventHandler* h = _handlers[pfd.fd];
    int revents = pfd.revents;
    if (revents & (POLLHUP | POLLERR))
        h->on_hangup(pfd.fd);
    else if (revents & POLLIN)
        h->on_readable(pfd.fd);
    else if (revents & POLLOUT)
        h->on_writable(pfd.fd);
}

// Construct a Connexion and register it if no error
void Acceptor::accept_new_connexion(int listen_fd) {
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
        log_event("NEW Connexion Socket FD: " + utils::to_str(c->fd));
    } catch (const std::exception& e) {
        log_error(std::string("accept error: ") + e.what());
        return;
    }

    // add it to the Connexions map and the polls vector
    _connexions[c->fd] = c;
    append_to_poll(c->fd);
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

void Acceptor::log_info(std::string s) { std::cout << Log::color(0) << s << Log::nl; }

void Acceptor::log_event(std::string s) {
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

void Acceptor::log_error(std::string s) { Log::error(s); }

void Acceptor::log_debug(std::string s) { Log::debug(s); }

std::ostream& operator<<(std::ostream& os, const sockaddr_in& addr) {
    uint32_t ip = ntohl(addr.sin_addr.s_addr);  // host byte order
    os << "    Address:";
    os << ((ip >> 24) & 0xFF) << "." << ((ip >> 16) & 0xFF) << "." << ((ip >> 8) & 0xFF)
       << "." << (ip & 0xFF) << "\n";
    os << "    Port:   " << ntohs(addr.sin_port) << "\n";
    return os;
}
