#include "Connexion.hpp"

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>

#include "colorC.hpp"
#include "common.h" // for operator<<(ostream, sockaddr_in)

Connexion::Connexion(int fd, const sockaddr_in &addr)
    : _fd(fd)
    , _addr(addr)
    , _state(READING)
    , _send_offset(0)
{
    fcntl(_fd, F_SETFL, O_NONBLOCK);
    std::cout << colorC::b(_fd) << "NEW Client Socket FD: " << _fd
              << colorC::nl;
    std::cout << _addr;
}

Connexion::~Connexion()
{
    if (_fd >= 0)
        close(_fd);
}

int Connexion::fd() const { return _fd; }
Connexion::State Connexion::state() const { return _state; }
const sockaddr_in &Connexion::addr() const { return _addr; }

std::string &Connexion::recv_buf() { return _recv_buf; }
const std::string &Connexion::recv_buf() const { return _recv_buf; }

ssize_t Connexion::do_recv()
{
    char buf[4096];
    ssize_t n = recv(_fd, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        _recv_buf.append(buf, n);
        buf[n] = '\0';
        std::cout << colorC::c(_fd) << "   FD " << _fd << " said:\n"
                  << buf << colorC::reset;
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

    ssize_t n = send(_fd, data, left, 0);
    if (n > 0) {
        _send_offset += n;
        if (_send_offset >= _send_buf.size())
            _state = CLOSING; // HTTP/1.0-style: close after response
    }
    return n;
}

void Connexion::queue_response(const std::string &resp)
{
    _send_buf = resp;
    _send_offset = 0;
    _state = WRITING;
}
