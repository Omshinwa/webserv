#include "EventLoop.hpp"

#include "signal.hpp"

namespace {
const int POLL_TIMEOUT_MS = 3000;
}  // namespace

// Close and free every handler the loop owns (in-flight Connections and the
// listening Servers), which also tears down any CGI pipes and reaps children.
// A handler can span several fds (a CgiHandler owns two pipes), so unregister
// every owned fd first, then delete each owned handler exactly once.
EventLoop::~EventLoop() {
    std::set<IEventHandler*> dead = _owned;
    for (size_t i = 0; i < _pollfds.size();) {
        if (_owned.count(fd_to_handler[_pollfds[i].fd]))
            unregister_fd(_pollfds[i].fd);
        else
            ++i;
    }
    for (std::set<IEventHandler*>::iterator it = dead.begin(); it != dead.end(); ++it)
        delete *it;
    _owned.clear();
}

// events = POLLIN or POLLOUT;
void EventLoop::register_fd(int fd, int events, IEventHandler* handler, bool owned) {
    // Take ownership first: if a push_back below throws, ~EventLoop still frees it.
    if (owned) _owned.insert(handler);
    pollfd polling_req;
    polling_req.fd = fd;
    polling_req.events = events;
    polling_req.revents = 0;
    _pollfds.push_back(polling_req);
    fd_to_handler[fd] = handler;
    fcntl(fd, F_SETFL, O_NONBLOCK);
}

bool EventLoop::has_registered_fd(IEventHandler* h) const {
    for (std::map<int, IEventHandler*>::const_iterator it = fd_to_handler.begin();
         it != fd_to_handler.end(); ++it)
        if (it->second == h) return true;
    return false;
}

void EventLoop::unregister_fd(int fd) {
    fd_to_handler.erase(fd);
    close(fd);
    for (size_t i = 0; i < _pollfds.size(); i++) {
        if (_pollfds[i].fd == fd) {
            _pollfds.erase(_pollfds.begin() + i);
            break;
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
                Log::event("SIGNAL INTERRUPT");
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

        // Idle / timeout pass.
        // CgiHandler spans two fds and so gets ticked twice; the second tick is a no-op
        // once the first flipped it finished.
        time_t now = time(NULL);
        for (size_t i = 0; i < _pollfds.size(); ++i)
            fd_to_handler[_pollfds[i].fd]->on_tick(now);

        // Reap finished fds, and delete owned handlers once their last fd is
        // gone. A handler can span several fds (a CgiHandler owns two pipes), so
        // the has_registered_fd() guard it.
        // Safe ONLY because no destructor unregisters fds (mutates _pollfds);
        for (size_t i = 0; i < _pollfds.size();) {
            IEventHandler* h = fd_to_handler[_pollfds[i].fd];
            if (h->finished) {
                unregister_fd(_pollfds[i].fd);
                if (_owned.count(h) && !has_registered_fd(h)) {
                    _owned.erase(h);
                    delete h;
                }
            } else {
                ++i;
            }
        }
    }
}

void EventLoop::handle_event(pollfd& pfd) {
    IEventHandler* h = fd_to_handler[pfd.fd];
    int revents = pfd.revents;
    // POLLHUP = peer closed its end (e.g. a CGI child that exited)
    // POLLERR = broken pipe/socket.
    // Route both through on_readable so the handler reacts itself
    // (read()==0 is a graceful EOF, read()<0 a clean abort)
    // POLLNVAL means the fd isn't even open.
    if (revents & (POLLIN | POLLHUP | POLLERR)) h->on_readable();
    if (revents & POLLOUT) h->on_writable();
    if (revents & POLLNVAL) h->finished = true;
}
