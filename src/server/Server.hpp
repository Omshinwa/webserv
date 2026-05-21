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
    explicit Server();
    ~Server();

    void run(); // main poll loop, blocks forever

private:
    int _fd;
    int _port;
    std::vector<pollfd> _pollfds; // list of all the poll requests
    std::map<int, Connexion *> _connexions; // int fd -> Connexion*

    void append_to_poll(int fd);
    void accept_new_connexion();
    void drop_connexion(Connexion *c);
    void handle_event(pollfd &pfd);

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
