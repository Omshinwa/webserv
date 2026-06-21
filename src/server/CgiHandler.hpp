#ifndef CGIHANDLER_H
#define CGIHANDLER_H

#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>

#include "../event/IEventHandler.hpp"
#include "../http/CgiProcess.hpp"
#include "../utils/Log.hpp"
#include "../utils/Utils.hpp"

class Connection;

class CgiHandler : public IEventHandler {
    public:
        CgiHandler(const RequestParser& req, const ServerConfig& config,
                   const std::string& interpreter, const std::string& script_path);
        CgiProcess cgi;
        Connection* _owner;  // who to notify on completion

        void on_readable();
        // read pipe; on EOF -> _owner->on_cgi_done(output, status)
        void on_writable();
        void on_tick(time_t now);

        std::string write_buffer;
        std::string read_buffer;
};

#endif
