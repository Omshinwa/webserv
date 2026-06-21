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

#include "../include.hpp"
#include "../utils/Log.hpp"
#include "Connexion.hpp"
#include "IEventHandler.hpp"

class CgiHandler : public IEventHandler {
    public:
        CgiHandler(const RequestParser& req, const ServerConfig& config,
                   const std::string& interpreter, const std::string& script_path);
        ;
        CgiProcess cgi;
        Connexion* _owner;  // who to notify on completion

        void on_readable(
                int fd);  // read pipe; on EOF -> _owner->on_cgi_done(output, status)
        void on_writable(int fd);
        void on_hangup(int fd);

        std::string buffer;
};

#endif
