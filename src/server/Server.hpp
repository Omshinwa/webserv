#ifndef SERVER_HPP
#define SERVER_HPP

#include <netinet/in.h>
#include <poll.h>

#include <map>
#include <string>
#include <vector>

class Connexion;

class Server {
public:
    explicit Server(int port);
    ~Server();

    void run(); // main poll loop, blocks forever

private:
    int _fd;
    int _port;
    std::vector<pollfd> _pollfds;
    std::map<int, Connexion *> _connexions;

    void setup_listening_socket();
    void accept_new();
    void drop(Connexion *c); // close client, remove from _connexions and _pollfds
    void handle_event(size_t &i); // dispatches POLLIN/POLLOUT for one fd

    static std::string build_response();

    // LOG

    void log_debug(std::string s);
    void log_info(std::string s);
    void log_event(std::string s);
    void log_error(std::string s);

    // INNACCESSIBLE

    Server(const Server &);
    Server &operator=(const Server &);
};

#endif
