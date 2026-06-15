#ifndef CGIPROCESS_H
#define CGIPROCESS_H

#include <unistd.h>

#include "../config/Config.hpp"
#include "RequestParser.hpp"

class CgiProcess {
    public:
    CgiProcess(RequestParser& res, const ServerConfig& config);
    std::string output;

    //
    // // private
    //
    private:
    int status_code;
    int fd[2];
};  // namespace CgiProcess

#endif
