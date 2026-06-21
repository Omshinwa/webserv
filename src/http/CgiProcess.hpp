#ifndef CGIPROCESS_H
#define CGIPROCESS_H

#include <unistd.h>

#include <string>

#include "../config/Config.hpp"
#include "RequestParser.hpp"

class CgiProcess {
    public:
        enum state { READING, WRITING };
        state state;
        CgiProcess(const RequestParser& req, const ServerConfig& config,
                   const std::string& interpreter, const std::string& script_path);
        // EOF on the child's stdout -> reap it and turn the exit status into an
        // HTTP status (200 / 502 / 504), stored in exec_status. Clears pid.
        void reap();
        // Force-kill a still-running child and reap it. No-op once pid is -1, so
        // it is safe to call again during teardown.
        void kill_child();

        int exec_status;
        const RequestParser& req;
        const ServerConfig& config;
        std::string interpreter;
        std::string script_path;
        pid_t pid;
        int fd[2];     // parent reads the child's stdout from fd[0]
        int in_fd[2];  // parent writes the child's stdin to in_fd[1]
        //
        // // private
        //
    private:
        void child_fork();
        void child_execve();
};  // namespace CgiProcess

#endif
