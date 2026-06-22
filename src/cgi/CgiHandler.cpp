#include "CgiHandler.hpp"

#include "../event/EventLoop.hpp"
#include "../server/Connection.hpp"

namespace {
const int CGI_TIMEOUT_SEC = 10;
}  // namespace

CgiHandler::CgiHandler(EventLoop& event_loop, RequestParser& req,
                       const ServerConfig& config, const std::string& interpreter,
                       const std::string& script_path)
        : IEventHandler(event_loop),
          cgi(req, config, interpreter, script_path),
          _owner(NULL),
          write_buffer(req.body),
          read_buffer("") {
    touch();
}

// Feed the request body to the child's stdin.
// Once it's all written (or there was none) close the write end so the child sees EOF.
void CgiHandler::on_writable() {
    if (finished) return;
    if (write_buffer.empty()) {
        finish_writing();
        return;
    }
    ssize_t n = write(cgi.in_fd[1], write_buffer.data(), write_buffer.size());
    // n <= 0: pipe full (EAGAIN) or the child is gone .
    // Either way retry next POLLOUT — EOF on the read side or the
    // timeout below will end things if the child really died.
    if (n <= 0) return;
    touch();
    write_buffer.erase(0, n);
    if (write_buffer.empty()) finish_writing();
}

void CgiHandler::finish_writing() {
    if (cgi.in_fd[1] == -1) return;
    event_loop.unregister_fd(cgi.in_fd[1]);  // closes the fd -> EOF to the child
    cgi.in_fd[1] = -1;
}

// Drain the child's stdout. read() == 0 is EOF: the script is done, so reap it,
// translate the exit status, and hand the buffered output to the Connection.
void CgiHandler::on_readable() {
    if (finished) return;

    char buf[4096];
    ssize_t n = read(cgi.fd[0], buf, sizeof(buf));
    if (n > 0) {
        touch();
        read_buffer.append(buf, n);
        return;
    }
    if (n == 0) {
        cgi.reap();  // EOF: waitpid + status -> exec_status
    } else {
        cgi.kill_child();       // pipe error: don't leave the child running
        cgi.exec_status = 502;  // Bad Gateway
    }
    complete();
}

void CgiHandler::on_tick(time_t now) {
    if (finished) return;
    if (now - _last_activity > CGI_TIMEOUT_SEC) {
        Log::warning("CGI timeout -> 504");
        cgi.kill_child();
        cgi.exec_status = 504;
        complete();
    }
}

void CgiHandler::complete() {
    // Stop polling our pipes (closing them) before handing back to the owner;
    // the -1 sentinels keep ~Connection from touching them a second time.
    if (cgi.fd[0] != -1) {
        event_loop.unregister_fd(cgi.fd[0]);
        cgi.fd[0] = -1;
    }
    if (cgi.in_fd[1] != -1) {
        event_loop.unregister_fd(cgi.in_fd[1]);
        cgi.in_fd[1] = -1;
    }
    finished = true;
    if (_owner) _owner->on_cgi_done(*this);
}
