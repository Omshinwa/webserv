#ifndef AEVENTHANDLER_H
#define AEVENTHANDLER_H

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

class Reactor;

class AEventHandler {
    public:
        AEventHandler(Reactor& reactor) : finished(false), reactor(reactor) {}

        virtual ~AEventHandler() {}
        virtual void on_readable() = 0;
        virtual void on_writable() = 0;
        // should turn finished to true if enough time has passed
        virtual void on_tick(time_t now) = 0;
        bool finished;  // is it done?
        Reactor& reactor;

        inline void touch() { _last_activity = time(NULL); }
        time_t _last_activity;
};

#endif
