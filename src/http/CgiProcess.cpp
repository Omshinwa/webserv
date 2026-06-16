#include "CgiProcess.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

// also builds the env char**
void child_execve(RequestParser& req, std::string file) {
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
        strings.push_back("SCRIPT_NAME=" + req.URI);
        strings.push_back("PATH_INFO=" + req.URI);
        // strings.push_back("SCRIPT_FILENAME=" + file); // maybe i dont need it who knows
        strings.push_back("SERVER_NAME=");
        strings.push_back("SERVER_PORT=");
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
    // env in excve doesnt modify the strings so it's find to const cast
    cstrings.push_back(NULL);

    char* argv[2];
    argv[0] = const_cast<char*>(file.c_str());
    argv[1] = 0;
    execve(file.c_str(), argv, cstrings.data());
}

void child_fork(RequestParser& req, int fd[2], std::string file) {
    Log::debug("CGI child_fork running");
    if (dup2(fd[1], STDOUT_FILENO) == -1) {
        Log::error("DUP2 FAIL");
        Log::error(std::strerror(errno));
    }
    close(fd[0]);
    close(fd[1]);
    // modify directory
    if (chdir("./www/cgi-bin/")) {
        Log::error("CGI FAIL");
        Log::error(std::strerror(errno));
    }
    child_execve(req, file);
    Log::error("EXECVE FAIL");
    Log::error(std::strerror(errno));
    exit(1);

    (void)req;
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

CgiProcess::CgiProcess(RequestParser& req, std::string file) {
    pid_t pid;

    pipe(fd);
    pid = fork();
    if (pid == 0) {
        child_fork(req, fd, file);
    } else if (pid > 0)  // parent
    {
        close(fd[1]);

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
        Log::error("CGI FAIL");
        Log::error(std::strerror(errno));
        status_code = 500;
        return;
        // fork failed -> 500
    }
}
