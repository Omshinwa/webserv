#ifndef CGIPROCESS_H
#define CGIPROCESS_H

#include "../config/Config.hpp"
#include "RequestParser.hpp"

#include <unistd.h>

#include <string>


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
