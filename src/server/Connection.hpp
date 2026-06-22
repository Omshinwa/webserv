#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <netinet/in.h>
#include <sys/types.h>

#include <ctime>
#include <string>
#include <vector>

#include "../cgi/CgiHandler.hpp"
#include "../config/Config.hpp"
#include "../event/IEventHandler.hpp"
#include "../http/RequestParser.hpp"

class Connection : public IEventHandler {
    public:
        const int fd;
        // Client IP, set by the Server right after accept(); handed to the
        // request when the response is built (used for the CGI REMOTE_ADDR).
        std::string remote_addr;

        // configs: all server blocks sharing this listening socket; used to
        // resolve the virtual host once the request's Host header is known.
        Connection(EventLoop& event_loop, int listen_fd,
                   const std::vector<ServerConfig>& configs, sockaddr_in addr);
        ~Connection();

        void on_readable();
        void on_writable();
        void on_tick(time_t now);

        // Once you've parsed a complete request, build the response and call this.
        void queue_response();
        // Spawn the CGI script and register its pipes with the event loop; the
        // client socket idles until on_cgi_done() fires.
        void start_cgi(const std::string& interpreter, const std::string& filepath,
                       const ServerConfig& config);
        void on_cgi_done();
        // Owned, async CGI handler. NULL until a CGI request is dispatched; the
        // synchronous CGI path (CgiProcess in ResponseBuilder) leaves it unused.
        CgiHandler* cgi;

    private:
        std::string _recv_buf;
        std::string _send_buf;
        size_t _send_offset;
        size_t recv_offset;

        RequestParser request;

        const std::vector<ServerConfig>& _configs;
        const ServerConfig* _active;  // resolved virtual host, NULL until header parsed

        // touch() / _last_activity (the idle-timeout clock) come from IEventHandler.

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
