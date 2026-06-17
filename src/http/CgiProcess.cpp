#include "CgiProcess.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>

#include "../utils/Log.hpp"
#include "../utils/Utils.hpp"
#include "RequestParser.hpp"

namespace {

// takes some text in http header format and converts it to
// environment variables format
// eg. user-agent -> USER_AGENT
std::string httpheader_to_envvar_format(std::string header) {
    for (size_t i = 0; i < header.size(); ++i) {
        header[i] =
                static_cast<char>(std::toupper(static_cast<unsigned char>(header[i])));
        if (header[i] == '-') header[i] = '_';
    }
    return "HTTP_" + header;
}

// this builds the env char**
void child_execve(RequestParser& req, const ServerConfig& config,
                  const std::string& interpreter) {
    std::string file;
    file = req.URI;
    if (file.find("?") != std::string::npos) file = utils::split(req.URI, "?")[0];
    if (file.find("/") != std::string::npos)
        file = utils::split(req.URI, "/")[utils::split(req.URI, "/").size() - 1];

    // this block setups the envs
    std::vector<std::string> strings;
    {
        strings.push_back("REQUEST_METHOD=" + req.method);
        if (req.body.size()) {
            strings.push_back("CONTENT_LENGTH=" + req.get_header("content-length"));
            strings.push_back("CONTENT_TYPE=" + req.get_header("content-type"));
        }
        if (req.URI.find("?") != std::string::npos)
            strings.push_back("QUERY_STRING=" + utils::split(req.URI, "?")[1]);
        else
            strings.push_back("QUERY_STRING=");
        strings.push_back("SERVER_PROTOCOL=HTTP/1.0");
        strings.push_back("GATEWAY_INTERFACE=CGI/1.1");
        // NOTE: the 42 cgi_tester binary returns 500 "PATH_INFO incorrect"
        // whenever SCRIPT_NAME is non-empty, so we leave it empty here.
        strings.push_back("SCRIPT_NAME=");
        strings.push_back("PATH_INFO=" + req.URI);
        strings.push_back("SCRIPT_FILENAME=" + file);  // maybe i dont need it who knows
        strings.push_back("SERVER_NAME=");
        strings.push_back("SERVER_PORT=" + utils::to_str(config.port));
        strings.push_back("SERVER_SOFTWARE=");
    }
    // this blocks add the header variables
    {
        for (t_dict::const_iterator it = req.get_header_ref().begin();
             it != req.get_header_ref().end(); ++it) {
            strings.push_back(httpheader_to_envvar_format(it->first) + "=" + it->second);
        }
    }
    std::vector<char*> cstrings;
    for (size_t i = 0; i < strings.size(); ++i)
        cstrings.push_back(const_cast<char*>(strings[i].c_str()));
    // env in excve doesnt modify the strings so it's fine to const cast
    cstrings.push_back(NULL);

    // interpreter set -> [interpreter, script]; empty -> run the file directly
    std::vector<char*> argv;
    if (!interpreter.empty())
        argv.push_back(const_cast<char*>(interpreter.c_str()));
    argv.push_back(const_cast<char*>(file.c_str()));
    argv.push_back(NULL);

    const char* path = interpreter.empty() ? file.c_str() : interpreter.c_str();
    execve(path, argv.data(), cstrings.data());

    Log::error("EXECVE FAIL");
    Log::error(std::strerror(errno));
    exit(1);
}

void child_fork(RequestParser& req, const ServerConfig& config, int fd[2], int in_fd[2],
                const std::string& interpreter) {
    // modify directory
    if (chdir("./www/cgi-bin/")) {
        Log::error("chdir FAIL");
        Log::error(std::strerror(errno));
        exit(1);
    }

    Log::debug("CGI child_fork running");
    if (dup2(in_fd[0], STDIN_FILENO) == -1) {
        Log::error("DUP2 FAIL");
        Log::error(std::strerror(errno));
        exit(1);
    }
    std::cout << std::flush;
    if (dup2(fd[1], STDOUT_FILENO) == -1) {
        Log::error("DUP2 FAIL");
        Log::error(std::strerror(errno));
        exit(1);
    }
    close(fd[0]);
    close(fd[1]);
    close(in_fd[0]);
    close(in_fd[1]);

    child_execve(req, config, interpreter);
}

int interpret_status(int status) {
    if WIFEXITED (status)  // child called exit() normally
    {
        if (WEXITSTATUS(status) == 0) return 200;
        Log::debug("CGI returned -1 / error");
        return 502;
    }
    Log::debug("CGI signaled / killed");
    return 502;  // signaled / killed
}
}  // namespace

CgiProcess::CgiProcess(RequestParser& req, const ServerConfig& config,
                       const std::string& interpreter) {
    pid_t pid;
    int in_fd[2];

    pipe(fd);
    pipe(in_fd);
    pid = fork();
    if (pid == 0) {
        child_fork(req, config, fd, in_fd, interpreter);
    } else if (pid > 0)  // parent
    {
        close(fd[1]);
        close(in_fd[0]);

        // feed the request body to the child's stdin, then close to send EOF
        if (req.body.size()) write(in_fd[1], req.body.data(), req.body.size());
        close(in_fd[1]);

        char buf[2048];
        ssize_t n;
        while ((n = read(fd[0], buf, sizeof(buf))) > 0) output.append(buf, n);

        Log::debug("CGI generated:\n" + output);

        int status;
        // pid_t r = waitpid(pid, &status, WNOHANG);
        // WNOHANG = don't block if child not done
        if (waitpid(pid, &status, 0) == -1) {
            Log::error("WAITPID FAIL");
            Log::error(std::strerror(errno));
            status_code = 500;
            return;
        }
        status_code = interpret_status(status);

    } else {
        close(fd[0]);
        close(fd[1]);
        close(in_fd[0]);
        close(in_fd[1]);
        Log::error("CGI FAIL");
        Log::error(std::strerror(errno));
        status_code = 500;
        return;
        // fork failed -> 500
    }
}
