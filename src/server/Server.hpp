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

    void _setup_listening_socket();
    void _accept_new();
    void _drop(size_t idx); // close client at pollfds[idx]
    void _handle_event(size_t &i); // dispatches POLLIN/POLLOUT for one fd

    static std::string _build_response();

    Server(const Server &);
    Server &operator=(const Server &);
};

#endif
