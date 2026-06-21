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
        std::string output;
        int exec_status;
        const RequestParser& req;
        const ServerConfig& config;
        const std::string& interpreter;
        const std::string& script_path;
        pid_t pid;
        int fd[2];
        int in_fd[2];
        //
        // // private
        //
    private:
        void child_fork();
        void child_execve();
};  // namespace CgiProcess

#endif
