#ifndef CONNEXION_HPP
#define CONNEXION_HPP

#include <netinet/in.h>
#include <sys/types.h>

#include <string>

#include "../http/RequestParser.hpp"
#include "../http/ResponseBuilder.hpp"

class Connexion {
  public:
    enum State {
        READING,  // waiting for request bytes
        WRITING,  // response queued, sending it out
        CLOSING   // mark for removal from poll set
    };

    const int fd;

    explicit Connexion(int listen_fd);  // does accept(); throws on failure
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
