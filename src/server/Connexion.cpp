#include "Connexion.hpp"

#include <sys/socket.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include "common.h"

Connexion::Connexion(int fd)
    : fd(fd)
    , _state(READING)
    , _send_offset(0)
{
    fcntl(fd, F_SETFL, O_NONBLOCK);
}

Connexion::~Connexion()
{
    Log::debug("~Destructor Connexion fd " + to_string(fd));
    if (fd >= 0)
        close(fd);
}

Connexion::State Connexion::state() const { return _state; }

void Connexion::mark_closing() { _state = CLOSING; }

ssize_t Connexion::do_recv()
{
    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        _recv_buf.append(buf, n);
        buf[n] = '\0';
        log_info("<    file descriptor " + to_string(fd) + " sent: \n" + buf);
    } else if (n == 0) {
        _state = CLOSING; // peer closed cleanly
    }
    return n;
}

ssize_t Connexion::do_send()
{
    if (_send_offset >= _send_buf.size())
        return 0;

    const char *data = _send_buf.data() + _send_offset;
    size_t left = _send_buf.size() - _send_offset;

    ssize_t n = send(fd, data, left, 0);
    if (n > 0) {
        _send_offset += n;

        log_info(
            ">    sent to file descriptor " + to_string(fd) + ": \n" + data);
        // if (_send_offset >= _send_buf.size())
        //     _state = CLOSING; // HTTP/1.0-style: close after response
    }
    return n;
}

void Connexion::queue_response(const std::string &resp)
{
    _send_buf = resp;
    _send_offset = 0;
    _state = WRITING;
}

//  █████          ███████      █████████   █████████
// ▒▒███         ███▒▒▒▒▒███   ███▒▒▒▒▒███ ███▒▒▒▒▒███
//  ▒███        ███     ▒▒███ ███     ▒▒▒ ▒███    ▒▒▒
//  ▒███       ▒███      ▒███▒███         ▒▒█████████
//  ▒███       ▒███      ▒███▒███    █████ ▒▒▒▒▒▒▒▒███
//  ▒███      █▒▒███     ███ ▒▒███  ▒▒███  ███    ▒███
//  ███████████ ▒▒▒███████▒   ▒▒█████████ ▒▒█████████
// ▒▒▒▒▒▒▒▒▒▒▒    ▒▒▒▒▒▒▒      ▒▒▒▒▒▒▒▒▒   ▒▒▒▒▒▒▒▒▒
//
//

void Connexion::log_info(std::string s)
{
    std::cout << Log::c(fd) << s << Log::nl;
}

void Connexion::log_event(std::string s)
{
    std::cout << Log::b(fd) << s << Log::nl;
}