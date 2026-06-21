#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <netinet/in.h>
#include <sys/types.h>

#include <ctime>
#include <string>
#include <vector>

#include "../config/Config.hpp"
#include "../http/RequestParser.hpp"
#include "../include.hpp"
#include "../event/IEventHandler.hpp"

typedef struct cgi_communication {
        std::string remote_addr;

} t_cgi_com;

class Connection : public IEventHandler {
    public:
        const int fd;
        // Client IP, set by the Server right after accept(); handed to the
        // request when the response is built (used for the CGI REMOTE_ADDR).
        std::string remote_addr;

        // configs: all server blocks sharing this listening socket; used to
        // resolve the virtual host once the request's Host header is known.
        Connection(int listen_fd, const std::vector<ServerConfig>& configs);
        ~Connection();

        void touch();

        void on_readable(int fd);
        void on_writable(int fd);
        void on_tick(time_t now);

        // Once you've parsed a complete request, build the response and call this.
        void queue_response();
        void on_cgi_done();
        CgiHandler cgi;

    private:
        std::string _recv_buf;
        std::string _send_buf;
        size_t _send_offset;
        size_t recv_offset;

        RequestParser request;

        const std::vector<ServerConfig>& _configs;
        const ServerConfig* _active;  // resolved virtual host, NULL until header parsed

        time_t _last_activity;  // last successful recv/send; drives the idle timeout

        // Pick the server block whose server_names matches the Host header,
        // falling back to the first (default) server for this socket.
        const ServerConfig& resolve_virtual_host(const std::string& host) const;

        // LOG
        void log_debug(std::string s);
        void log_info(std::string s);
        void log_event(std::string s);
        void log_error(std::string s);

        // INNACCESSIBLE
        // we dont allow several Connection for one fd
        Connection();
        Connection(const Connection&);
        Connection& operator=(const Connection&);
};

#endif
