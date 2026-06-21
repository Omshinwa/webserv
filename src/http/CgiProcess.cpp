#include "CgiProcess.hpp"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>

#include "../utils/Log.hpp"
#include "../utils/Utils.hpp"

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

int interpret_status(int status) {
    if WIFEXITED (status)  // child called exit() normally
    {
        if (WEXITSTATUS(status) == 0) return 200;
        Log::debug("CGI returned -1 / error");
        return 502;
    }
    // our own alarm() fired and killed the runaway CGI -> Gateway Timeout
    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM) {
        Log::warning("CGI timed out (SIGALRM)");
        return 504;
    }
    Log::debug("CGI signaled / killed");
    return 502;  // signaled / killed
}
}  // namespace

CgiProcess::CgiProcess(const RequestParser& req, const ServerConfig& config,
                       const std::string& interpreter, const std::string& script_path)
        : req(req), config(config), interpreter(interpreter), script_path(script_path) {
    Log::event("CGI request");
    Log::debug("interpreter: [" + interpreter + "] script path: [" + script_path + "]");
    exec_status = 0;
    pid = -1;

    if (pipe(fd) == -1 || pipe(in_fd) == -1) {
        Log::error("CGI pipe FAIL");
        Log::error(std::strerror(errno));
        exec_status = 500;
        return;
    }

    // Flush before forking so the child doesn't inherit (and re-emit) our
    // buffered log output when it flushes stdout in child_fork().
    std::cout << std::flush;
    pid = fork();
    if (pid == 0) {
        child_fork();  // never returns
    } else if (pid > 0) {
        // Parent keeps fd[0] (reads the child's stdout) and in_fd[1] (writes the
        // child's stdin); the matching ends belong to the child. Both kept ends
        // go non-blocking so the event loop can drive them without ever stalling
        // the single thread — no read/write/waitpid loop here anymore.
        close(fd[1]);
        close(in_fd[0]);
        fcntl(fd[0], F_SETFL, O_NONBLOCK);
        fcntl(in_fd[1], F_SETFL, O_NONBLOCK);
    } else {
        close(fd[0]);
        close(fd[1]);
        close(in_fd[0]);
        close(in_fd[1]);
        Log::error("CGI fork FAIL");
        Log::error(std::strerror(errno));
        exec_status = 500;  // fork failed -> 500
    }
}

// Called by CgiHandler once the child closes its stdout (EOF). The child has
// already exited or is exiting, so this won't block long.
void CgiProcess::reap() {
    if (pid <= 0) {
        exec_status = 500;
        return;
    }
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        Log::error("WAITPID FAIL");
        Log::error(std::strerror(errno));
        exec_status = 500;
        pid = -1;
        return;
    }
    exec_status = interpret_status(status);
    pid = -1;
}

void CgiProcess::kill_child() {
    if (pid <= 0) return;
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    pid = -1;
}

void CgiProcess::child_fork() {
    // run the script from its own directory (same place the URI resolved to)
    size_t slash = script_path.rfind('/');
    std::string dir = (slash == std::string::npos) ? "." : script_path.substr(0, slash);
    if (chdir(dir.c_str())) {
        Log::error("chdir FAIL");
        Log::error(std::strerror(errno));
        exit(1);
    }

    // Log::debug("CGI child_fork running");
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
    child_execve();
}

// this builds the env char** in the stack
void CgiProcess::child_execve() {
    // we already chdir'd into the script's directory, so exec just the basename
    size_t slash = script_path.rfind('/');
    std::string file =
            (slash == std::string::npos) ? script_path : script_path.substr(slash + 1);

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
        strings.push_back("SCRIPT_NAME=");  // 42 tester bug
        // if (interpreter.find("php-cgi") != std::string::npos)
        // {
        //     strings.push_back("SCRIPT_FILENAME=" + file);  // for php-cgi
        // }
        strings.push_back("PATH_INFO=" + req.URI);

        // AUTH_TYPE, no need, we dont require any authentitication
        strings.push_back("REMOTE_ADDR=" + req.remote_addr);
        strings.push_back("REMOTE_HOST=" + req.remote_addr);

        std::string host = req.get_header("host");  // "localhost:8080"
        size_t colon = host.find(':');
        if (colon != std::string::npos) host = host.substr(0, colon);  // "localhost"
        if (host.empty())
            host = config.server_names.empty() ? config.host : config.server_names[0];
        strings.push_back("SERVER_NAME=" + host);

        strings.push_back("SERVER_PORT=" + utils::to_str(config.port));
        strings.push_back("SERVER_SOFTWARE=webserv/1.0");
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
    if (!interpreter.empty()) argv.push_back(const_cast<char*>(interpreter.c_str()));
    argv.push_back(const_cast<char*>(file.c_str()));
    argv.push_back(NULL);

    const char* path = interpreter.empty() ? file.c_str() : interpreter.c_str();
    execve(path, argv.data(), cstrings.data());

    Log::error("EXECVE FAIL");
    Log::error(std::strerror(errno));
    exit(1);
}
