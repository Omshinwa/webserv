#include "CgiProcess.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "../utils/Log.hpp"

namespace {
void child_fork(int fd[2]) {
    if (dup2(fd[1], STDOUT_FILENO) == -1) {
        Log::error("DUP2 FAIL");
        Log::error(std::strerror(errno));
    }
    close(fd[0]);
    close(fd[1]);
    Log::debug("im a child");
    // modify directory
    if (chdir("./www/cgi-bin/")) {
        Log::error("CGI FAIL");
        Log::error(std::strerror(errno));
    }
    char* empty[1];
    empty[0] = 0;
    execve("test.py", empty, empty);
}

int interpret_status(int status) {
    if WIFEXITED (status)  // child called exit() normally
    {
        if (WEXITSTATUS(status) == 0) return 200;
    }
    return 502;  // signaled / killed
}
}  // namespace

CgiProcess::CgiProcess(RequestParser& res, const ServerConfig& config) {
    pid_t pid;

    pipe(fd);

    pid = fork();
    if (pid == 0) {
        child_fork(fd);
    } else if (pid > 0)  // parent
    {
        close(fd[1]);
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

        char buf[2048];
        ssize_t n = 1;
        while (n > 0) {
            n = read(fd[1], buf, sizeof(buf) - 1);
        }
        output += buf;
    } else {
        Log::error("CGI FAIL");
        Log::error(std::strerror(errno));
        status_code = 500;
        return;
        // fork failed -> 500
    }
    (void)res;
    (void)config;
}
