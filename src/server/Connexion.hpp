#ifndef CONNEXION_HPP
#define CONNEXION_HPP

#include <netinet/in.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include "../config/Config.hpp"
#include "../http/RequestParser.hpp"

class Connexion {
    public:
        enum State {
            READING,  // waiting for request bytes
            WRITING,  // response queued, sending it out
            CLOSING   // mark for removal from poll set
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

        // Pull bytes from the socket into _recv_buf.
        // Returns bytes read, 0 on peer close, -1 on error.
        void do_recv();

        // Push bytes from _send_buf to the socket, advancing _send_offset.
        // Returns bytes sent, -1 on error. Flips state to CLOSING when buffer
        // drained.
        ssize_t do_send();

        // Once you've parsed a complete request, build the response and call this.
        void queue_response();

    private:
        State _state;
        std::string _recv_buf;
        std::string _send_buf;
        size_t _send_offset;

        RequestParser request;

        const std::vector<ServerConfig>& _configs;
        const ServerConfig* _active;  // resolved virtual host, NULL until header parsed

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
