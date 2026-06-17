#ifndef CGIPROCESS_H
#define CGIPROCESS_H

#include <unistd.h>

#include <string>

#include "../config/Config.hpp"
#include "RequestParser.hpp"

class CgiProcess {
    public:
    CgiProcess(RequestParser& res, const ServerConfig& config,
               const std::string& interpreter);
    std::string output;
    int status_code;

    //
    // // private
    //
    private:
    int fd[2];
};  // namespace CgiProcess

#endif
