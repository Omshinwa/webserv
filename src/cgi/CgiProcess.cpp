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
#include <stdexcept>
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
    Log::warning("CGI signaled / killed");
    return 502;
}
}  // namespace

CgiProcess::CgiProcess(const RequestParser& req, const ServerConfig& config,
                       const std::string& interpreter, const std::string& script_path)
        : req(req), config(config), interpreter(interpreter), script_path(script_path) {
    Log::event("CGI request");
    Log::debug("interpreter: [" + interpreter + "] script path: [" + script_path + "]");
    exec_status = 0;
    pid = -1;

    if (pipe(in_fd) == -1) {
        Log::error("CGI pipe FAIL");
        Log::error(std::strerror(errno));
        exec_status = 500;
        return;
    }
    if (pipe(out_fd) == -1) {
        Log::error("CGI pipe FAIL");
        Log::error(std::strerror(errno));
        close(in_fd[0]);
        close(in_fd[1]);
        exec_status = 500;
        return;
    }

    // Flush before forking so the child doesn't inherit (and re-emit) our
    // buffered log output when it flushes stdout in child_fork().
    std::cout << std::flush;
    pid = fork();
    if (pid == 0) {
        child_fork();  // never returns
    }
    // parent
    close(in_fd[0]);
    close(out_fd[1]);
    if (pid < 0) {  // failed
        close(in_fd[1]);
        close(out_fd[0]);
        Log::error("CGI fork FAIL");
        Log::error(std::strerror(errno));
        exec_status = 500;  // fork failed -> 500
    }
}

CgiProcess::~CgiProcess() {}

// Called by CgiHandler once the child closes its stdout (EOF).
// -> reap it and turn the exit status into an HTTP status (200 / 502 / 504), stored in
// exec_status. Clears pid. The child has already exited or is exiting, so this won't
// block long.
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

// Force-kill a still-running child and reap it.
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
        throw std::runtime_error("CGI child fork fail");
    }

    // Log::debug("CGI child_fork running");
    if (dup2(in_fd[0], STDIN_FILENO) == -1) {
        Log::error("DUP2 FAIL");
        Log::error(std::strerror(errno));
        throw std::runtime_error("CGI child fork fail");
    }
    std::cout << std::flush;
    if (dup2(out_fd[1], STDOUT_FILENO) == -1) {
    if (dup2(out_fd[1], STDOUT_FILENO) == -1) {
        Log::error("DUP2 FAIL");
        Log::error(std::strerror(errno));
        throw std::runtime_error("CGI child fork fail");
    }

    // out_fd[1] / in_fd[0]: after dup2 these are duplicated onto stdout/stdin. The child
    // out_fd[1] / in_fd[0]: after dup2 these are duplicated onto stdout/stdin. The child
    // only needs 0 and 1, so the original numbered copies are redundant and get closed
    // for hygiene.
    close(out_fd[0]);
    close(out_fd[1]);
    close(out_fd[0]);
    close(out_fd[1]);
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
        strings.push_back("QUERY_STRING=" + req.query_string);
        strings.push_back("SERVER_PROTOCOL=HTTP/1.0");
        strings.push_back("GATEWAY_INTERFACE=CGI/1.1");
        strings.push_back("SCRIPT_NAME=");  // 42 tester bug
        // if (interpreter.find("php-cgi") != std::string::npos) {
        strings.push_back("SCRIPT_FILENAME=" + file);
        strings.push_back("REDIRECT_STATUS=200");
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
            if (it->first == "content-length" || it->first == "content-type") continue;
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
    throw std::runtime_error("CGI child fork fail");
    // be careful that there's no catch between this and the main() catch
}
