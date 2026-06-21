#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include "../include.hpp"

// allows for async fd, listen to events with poll() and dispatch it to the correct
// handlers
class EventLoop {
    public:
        void run();
        void handle_event(pollfd& pfd);

        void register_fd(int fd, int events, IEventHandler* handler);
        void unregister_fd(int fd);

    private:
        std::map<int, IEventHandler&> fd_to_eventHandler;
        std::vector<pollfd> _pollfds;  // list of all the poll requests
};

#endif
