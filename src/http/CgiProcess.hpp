#ifndef CGIPROCESS_H
#define CGIPROCESS_H

#include <unistd.h>

#include <string>

#include "../config/Config.hpp"
#include "RequestParser.hpp"

class CgiProcess {
    public:
        CgiProcess(const RequestParser& res, const ServerConfig& config,
                   const std::string& interpreter, const std::string& script_path);
        std::string output;
        int exec_status;

        //
        // // private
        //
    private:
        int fd[2];
};  // namespace CgiProcess

#endif
