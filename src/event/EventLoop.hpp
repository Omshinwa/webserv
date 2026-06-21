#ifndef EVENTLOOP_H
#define EVENTLOOP_H
// ── standard library ────────────────────────────────────────────────────────
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ── POSIX / system ──────────────────────────────────────────────────────────
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// ── project ─────────────────────────────────────────────────────────────────

#include "../utils/Log.hpp"
#include "../utils/Utils.hpp"
//
#include "../config/Config.hpp"
#include "IEventHandler.hpp"

// allows for async fd, listen to events with poll() and dispatch it to the correct
// handlers
class EventLoop {
    public:
        void run();
        void handle_event(pollfd& pfd);

        // `owned` hands the handler's lifetime to the loop: once it is finished
        // and none of its fds remain registered, the loop deletes it. Pass false
        // for handlers someone else owns (the Servers live in main; a CgiHandler
        // is owned by its Connection).
        void register_fd(int fd, int events, IEventHandler* handler, bool owned = false);
        void unregister_fd(int fd);
        // Change what an already-registered fd is polled for, e.g. switch from
        // waiting on POLLIN (reading a request) to POLLOUT (sending a response).
        void set_events(int fd, short events);

    private:
        std::map<int, IEventHandler*> fd_to_handler;
        std::vector<pollfd> _pollfds;       // list of all the poll requests
        std::set<IEventHandler*> _owned;    // handlers the loop will delete

        // Does any still-registered fd map to this handler?
        bool has_registered_fd(IEventHandler* h) const;
};

#endif
