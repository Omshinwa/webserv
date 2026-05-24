#include "Connexion.hpp"

#include <sys/socket.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include "common.h"

// Connexion handles the buffers for reading and writing
// It creates the Request and Response

Connexion::Connexion(int fd)
    : fd(fd)
    , _state(READING)
    , _send_offset(0)
    , request(_recv_buf)
{
    fcntl(fd, F_SETFL, O_NONBLOCK);
}

Connexion::~Connexion()
{
    Log::debug("~Destructor Connexion fd " + util::to_string(fd));
    if (fd >= 0)
        close(fd);
}

Connexion::State Connexion::state() const { return _state; }

void Connexion::mark_closing() { _state = CLOSING; }

// return False: you should close the connexion
void Connexion::do_recv()
{
    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        _recv_buf.append(buf, n);
        log_event("<    file descriptor " + util::to_string(fd) + " sent:");
        log_info(buf);
        request.parse();
    } else if (n == 0) {
        _state = CLOSING; // peer closed cleanly
    } else {
        log_error("recv returned a negative number");
        _state = CLOSING; // some error
    }

    Log::info("parse state: " + util::to_string(request.state()));
    switch (request.state()) {
    case RequestParser::INCOMPLETE_HEADER:
        break;
    case RequestParser::INCOMPLETE_BODY:
        break;
    case RequestParser::COMPLETE:
        _state = CLOSING;
        queue_response();
        break;
    case RequestParser::ERROR:
        _state = CLOSING;
        queue_response();
        break;
    }
    return;
}

ssize_t Connexion::do_send()
{
    if (_send_offset >= _send_buf.size())
        return 0;

    const char *buf = _send_buf.data() + _send_offset;
    size_t left = _send_buf.size() - _send_offset;

    ssize_t n = send(fd, buf, left, 0);
    if (n > 0) {
        _send_offset += n;

        log_event(">    sent to file descriptor " + util::to_string(fd));
        log_info(buf);
        // if (_send_offset >= _send_buf.size())
        //     _state = CLOSING; // HTTP/1.0-style: close after response
    }
    return n;
}

void Connexion::queue_response()
{
    _send_buf = ResponseBuilder::build(request);
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
    Log::color_idx = fd;
    Log::info(s);
}

void Connexion::log_event(std::string s)
{
    Log::color_idx = fd;
    Log::event(s);
}

void Connexion::log_error(std::string s)
{
    Log::color_idx = fd;
    Log::error(s);
}