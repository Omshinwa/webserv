#ifndef CGIPROCESS_H
#define CGIPROCESS_H

#include <unistd.h>

#include <string>

#include "../config/Config.hpp"
#include "RequestParser.hpp"

class CgiProcess {
    public:
        CgiProcess(const RequestParser& req, const ServerConfig& config,
                   const std::string& interpreter, const std::string& script_path);
        std::string output;
        int exec_status;
        const RequestParser& req;
        const ServerConfig& config;
        const std::string& interpreter;
        const std::string& script_path;
        pid_t pid;
        //
        // // private
        //
    private:
        int fd[2];
        int in_fd[2];
        void child_fork();
        void child_execve();
};  // namespace CgiProcess

#endif
