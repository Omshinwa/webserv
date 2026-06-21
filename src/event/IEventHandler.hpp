#ifndef IEVENTHANDLER_H
#define IEVENTHANDLER_H

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

class EventLoop;

class IEventHandler {
    public:
        // No default ctor: every subclass is forced to pass the loop through
        // its own initializer list (: IEventHandler(event_loop)), or it won't
        // compile. That's how the child is "forced" to provide event_loop.
        IEventHandler(EventLoop& event_loop) : finished(false), event_loop(event_loop) {}

        virtual ~IEventHandler() {}
        virtual void on_readable() = 0;
        virtual void on_writable() = 0;
        virtual void on_tick(time_t now) = 0;

        bool finished;  // is it done?
        EventLoop& event_loop;

        inline void touch() { _last_activity = time(NULL); }
        time_t _last_activity;
};

#endif
