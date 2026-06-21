#include "EventLoop.hpp"

#include "../server/signal.hpp"

namespace {
const int POLL_TIMEOUT_MS = 3000;
}  // namespace

void EventLoop::register_fd(int fd, int events, IEventHandler* handler) {
    pollfd polling_req;
    polling_req.fd = fd;
    polling_req.events = events;  // POLLIN;
    polling_req.revents = 0;
    _pollfds.push_back(polling_req);
    fd_to_handler[fd] = handler;
}

// poll uses events and revents:
// events  = your question  → "is this fd readable? writable?"
// revents = the answer     → "yes readable / yes writable / hung up / error"
void EventLoop::run() {
    webserv::setup_signals();
    while (!webserv::g_stop) {
        // poll will set all the revents to 0, then
        // poll will BLOCK the process until an event triggers it
        int n = poll(&_pollfds[0], _pollfds.size(), POLL_TIMEOUT_MS);
        if (n < 0) {
            if (webserv::g_stop)  // signal interrupted
            {
                Log::error("SIGNAL INTERRUPT");
                continue;
            }
            Log::error("poll error");
            throw std::runtime_error(std::strerror(errno));
        }

        // goes through polls, handle events
        for (size_t i = 0; i < _pollfds.size(); i++) {
            if (_pollfds[i].revents == 0) continue;
            handle_event(_pollfds[i]);
        }

        // Idle
        time_t now = time(NULL);
        for (size_t i = 0; i < _pollfds.size(); ++i) {
            int fd = _pollfds[i].fd;
            fd_to_handler[fd]->on_tick(now);
        }

        for (size_t i = 0; i < _pollfds.size(); ++i) {
            int fd = _pollfds[i].fd;
            if (fd_to_handler[fd]->finished) unregister_fd(fd);
        }
    }
}

void EventLoop::set_events(int fd, short events) {
    for (size_t i = 0; i < _pollfds.size(); i++) {
        if (_pollfds[i].fd == fd) {
            _pollfds[i].events = events;
            return;
        }
    }
}

void EventLoop::unregister_fd(int fd) {
    fd_to_handler.erase(fd);
    Log::event("CLOSED Event FD: " + utils::to_str(fd));
    close(fd);
    // should the EventLoop close the fd, or the Connection...?
    for (size_t i = 0; i < _pollfds.size(); i++) {
        if (_pollfds[i].fd == fd) {
            _pollfds.erase(_pollfds.begin() + i);
            break;
        }
    }
}

void EventLoop::handle_event(pollfd& pfd) {
    IEventHandler* h = fd_to_handler[pfd.fd];
    int revents = pfd.revents;
    if (revents & POLLIN) h->on_readable();  // drains; read()==0 = graceful EOF
    if (revents & POLLOUT) h->on_writable();
    if (revents & (POLLERR | POLLNVAL))  // real breakage → abort / 502
        h->finished = true;
}
