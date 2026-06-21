#ifndef CONNEXION_HPP
#define CONNEXION_HPP

#include <netinet/in.h>
#include <sys/types.h>

#include <ctime>
#include <string>
#include <vector>

#include "../config/Config.hpp"
#include "../http/RequestParser.hpp"
#include "CgiHandler.hpp"

class Connexion {
    public:
        enum State {
            READING,  // waiting for request bytes
            WRITING,  // response queued, sending it out
            CGI_RUNNING,
            CLOSING  // mark for removal from poll set
        };

        const int fd;
        // Client IP, set by the Server right after accept(); handed to the
        // request when the response is built (used for the CGI REMOTE_ADDR).
        std::string remote_addr;

        // configs: all server blocks sharing this listening socket; used to
        // resolve the virtual host once the request's Host header is known.
        Connexion(int listen_fd, const std::vector<ServerConfig>& configs);
        ~Connexion();

        State state() const;
        void mark_closing();

        // Idle-timeout helpers. touch() stamps the current time; it's called on
        // every successful recv/send. timed_out() reports whether the connection
        // has been silent for longer than idle_secs.
        void touch();
        bool timed_out(time_t now, time_t idle_secs) const;

        // Pull bytes from the socket into _recv_buf.
        // Returns bytes read, 0 on peer close, -1 on error.
        void do_recv();

        // Push bytes from _send_buf to the socket, advancing _send_offset.
        // Returns bytes sent, -1 on error. Flips state to CLOSING when buffer
        // drained.
        ssize_t do_send();

        // Once you've parsed a complete request, build the response and call this.
        void queue_response();
        void on_cgi_done();
        CgiHandler cgi;

    private:
        State _state;
        std::string _recv_buf;
        std::string _send_buf;
        size_t _send_offset;

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
        // we dont allow several Connexion for one fd
        Connexion();
        Connexion(const Connexion&);
        Connexion& operator=(const Connexion&);
};

#endif
