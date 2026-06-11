#ifndef SERVER_HPP
# define SERVER_HPP

# include "../config/Config.hpp"

# include <vector>
# include <map>
# include <string>

# include <poll.h>

class   Connexion;

class   Server
{
    private:
        int                         _fd;
        int                         _port;
        std::vector<pollfd>         _pollfds;
        std::map<int, Connexion*>   _connexions;

        void    append_to_poll(int fd);
        void    accept_new_connexion();
        void    drop_connexion(Connexion* c);
        void    handle_event(pollfd& pfd);

        void    log_debug(std::string s);
        void    log_info(std::string s);
        void    log_event(std::string s);
        void    log_error(std::string s);

        Server(const Server&);
        Server& operator=(const Server&);

    public:
        Server(const std::vector<ServerConfig>& configs);
        ~Server();

        void    run();
};

#endif
