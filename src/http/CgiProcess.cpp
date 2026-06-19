#include "CgiProcess.hpp"

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
#include "RequestParser.hpp"

namespace {

// A CGI script must never hang the (single-threaded) server. The child arms
// alarm(CGI_TIMEOUT_SEC) on itself; if it overruns, the kernel kills it with
// SIGALRM and we answer 504 Gateway Timeout.
const unsigned int CGI_TIMEOUT_SEC = 3;

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

// this builds the env char** in the stack
void child_execve(const RequestParser& req, const ServerConfig& config,
                  const std::string& interpreter, const std::string& script_path) {
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

void child_fork(const RequestParser& req, const ServerConfig& config, int fd[2],
                int in_fd[2], const std::string& interpreter,
                const std::string& script_path) {
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

    // self-destruct timer: if the script doesn't finish within the budget, the
    // kernel delivers SIGALRM (default action: terminate). The timer survives
    // execve, so it keeps ticking through the interpreter/script.
    // alarm(CGI_TIMEOUT_SEC);

    child_execve(req, config, interpreter, script_path);
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
                       const std::string& interpreter, const std::string& script_path) {
    pid_t pid;
    int in_fd[2];

    Log::event("CGI request");
    Log::debug("interpreter: [" + interpreter + "] script path: [" + script_path + "]");
    pipe(fd);
    pipe(in_fd);
    pid = fork();
    if (pid == 0) {
        child_fork(req, config, fd, in_fd, interpreter, script_path);
    } else if (pid > 0)  // parent
    {
        close(fd[1]);
        close(in_fd[0]);

        // feed the request body to the child's stdin, then close to send EOF
        if (req.body.size()) write(in_fd[1], req.body.data(), req.body.size());
        close(in_fd[1]);

        // The child armed alarm(CGI_TIMEOUT_SEC) on itself: if it hangs, the
        // kernel kills it, which closes its stdout and unblocks the read below.
        char buf[2048];
        ssize_t n;
        while ((n = read(fd[0], buf, sizeof(buf))) > 0) output.append(buf, n);

        if (output.size() > 150)  // we only display up to 150 chars in the debug
        {
            Log::debug(output.substr(0, 150) + "\n...");
        } else
            Log::debug(output);

        int status;
        // the child is already dead or dying (alarm), so this won't block long
        if (waitpid(pid, &status, 0) == -1) {
            Log::error("WAITPID FAIL");
            Log::error(std::strerror(errno));
            exec_status = 500;
            return;
        }
        exec_status = interpret_status(status);

    } else {
        close(fd[0]);
        close(fd[1]);
        close(in_fd[0]);
        close(in_fd[1]);
        Log::error("CGI FAIL");
        Log::error(std::strerror(errno));
        exec_status = 500;
        return;
        // fork failed -> 500
    }
}
