#ifndef CGIHANDLER_H
#define CGIHANDLER_H

#include <sys/wait.h>
#include <unistd.h>

#include "../event/AEventHandler.hpp"
#include "CgiProcess.hpp"

class Connection;

class CgiHandler : public AEventHandler {
    public:
        CgiHandler(Reactor& reactor, RequestParser& req, const ServerConfig& config,
                   const std::string& interpreter, const std::string& script_path);
        ~CgiHandler();

        CgiProcess cgi;
        Connection* _owner;  // who to notify on completion

        void on_readable();  // drain stdout; on EOF reap + notify the Connection
        void on_writable();  // feed stdin from write_buffer, then close it (EOF)
        void on_tick(time_t now);

        std::string& write_buffer;
        std::string read_buffer;

    private:
        // INNACCESSIBLE
        CgiHandler();
        CgiHandler operator=(const CgiHandler&);
        CgiHandler(const CgiHandler&);

        void finish_writing();
        void complete();
};

#endif
