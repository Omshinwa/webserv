
#include "Connexion.hpp"

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include "../utils/Log.hpp"
#include "../utils/Utils.hpp"

// Connexion handles the buffers for reading and writing
// It creates the Request and Response

Connexion::Connexion(int fd)
        : fd(fd), _state(READING), _send_offset(0), request(_recv_buf) {
    fcntl(fd, F_SETFL, O_NONBLOCK);
}

Connexion::~Connexion() {
    Log::debug("~Destructor Connexion fd " + utils::to_str(fd));
    if (fd >= 0) close(fd);
}

Connexion::State Connexion::state() const { return _state; }

void Connexion::mark_closing() { _state = CLOSING; }

// return False: you should close the connexion
void Connexion::do_recv() {
    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        _recv_buf.append(buf, n);
        log_event("<    RECEIVED file descriptor " + utils::to_str(fd) + ":");
        log_info(buf);
        request.parse();
    } else if (n == 0) {
        _state = CLOSING;  // peer closed cleanly
    } else {
        log_error("recv returned a negative number");
        _state = CLOSING;  // some error
    }

    // Log::info("parse state: " + utils::to_str(request.state()));
    switch (request.state()) {
        case RequestParser::INCOMPLETE_HEADER:
            break;
        case RequestParser::INCOMPLETE_BODY:
            break;
        case RequestParser::COMPLETE:
            queue_response();
            break;
        case RequestParser::ERROR:
            queue_response();
            break;
    }

    return;
}

ssize_t Connexion::do_send() {
    if (_send_offset >= _send_buf.size()) return 0;

    const char* buf = _send_buf.data() + _send_offset;
    size_t left = _send_buf.size() - _send_offset;

    ssize_t n = send(fd, buf, left, 0);
    if (n > 0) {
        log_event(">    SENT to file descriptor " + utils::to_str(fd));

        if (n > 600)  // we only display up to 600 chars in the debug
        {
            log_info(_send_buf.substr(_send_offset, _send_offset + 600));
            log_info("...");
            log_info("...");
        } else
            log_info(buf);

        _send_offset += n;  // update the offset buffer
        if (_send_offset >= _send_buf.size())
            _state = CLOSING;  // HTTP/1.0-style: close after response
    }
    return n;
}

void Connexion::queue_response() {
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

void Connexion::log_info(std::string s) {
    Log::color_idx = fd;
    Log::info(s);
}

void Connexion::log_event(std::string s) {
    Log::color_idx = fd;
    Log::event(s);
}

void Connexion::log_error(std::string s) {
    Log::color_idx = fd;
    Log::error(s);
}
