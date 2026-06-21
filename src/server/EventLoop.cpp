#include "EventLoop.hpp"

namespace {
const int POLL_TIMEOUT_MS = 3000;
}  // namespace

void EventLoop::register_fd(int fd, int events, IEventHandler* handler) {
    pollfd polling_req;
    polling_req.fd = fd;
    polling_req.events = events;  // POLLIN;
    polling_req.revents = 0;
    _pollfds.push_back(polling_req);
    fd_to_eventHandler[fd] = handler;
}

// poll uses events and revents:
// events  = your question  → "is this fd readable? writable?"
// revents = the answer     → "yes readable / yes writable / hung up / error"
void EventLoop::run() {
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

        // Idle sweep: mark connections silent for too long as CLOSING.
        time_t now = time(NULL);
        for (std::map<int, Connexion*>::iterator it = _connexions.begin();
             it != _connexions.end(); ++it) {
            Connexion* c = it->second;
            if (c->state() != Connexion::CLOSING &&
                c->timed_out(now, CONNEXION_TIMEOUT_SEC)) {
                log_event("TIMEOUT Connexion Socket FD: " + utils::to_str(c->fd));
                c->mark_closing();
            }
        }

        // goes through connexions, clean them if their state is 'CLOSING',
        for (std::map<int, Connexion*>::iterator it = _connexions.begin();
             it != _connexions.end();) {
            Connexion* c = it->second;
            ++it;  // advance before drop_connexion() invalidates the current
                   // iterator
            if (c->state() == Connexion::CLOSING) drop_connexion(c);
        }
        for (int i = 0; i < _pollfds.size(); ++i) {
            int fd =
                    _pollfds[i].fd if (fd_to_eventHandler[fd].finished) unregister_fd(fd);
        }
    }
}

void EventLoop::unregister_fd(int fd) {
    fd_to_eventHandler.erase(fd);
    Log::event("CLOSED Event FD: " + utils::to_str(fd));
    close(fd);
    // delete fd_to_eventHandler[fd];
    // destructor closes the fd ?
    for (size_t i = 0; i < _pollfds.size(); i++) {
        if (_pollfds[i].fd == fd) {
            _pollfds.erase(_pollfds.begin() + i);
            break;
        }
    }
}

void EventLoop::handle_event(pollfd& pfd) {
    IEventHandler* h = fd_to_eventHandler[pfd.fd];
    int revents = pfd.revents;
    if (revents & (POLLHUP | POLLERR))
        h->on_hangup(pfd.fd);
    else if (revents & POLLIN)
        h->on_readable(pfd.fd);
    else if (revents & POLLOUT)
        h->on_writable(pfd.fd);
}
