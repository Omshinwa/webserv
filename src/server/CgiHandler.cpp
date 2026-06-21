#include "CgiHandler.hpp"

#include <stdexcept>

#include "Connection.hpp"

namespace {
int CONNECTION_TIMEOUT_SEC = 5;
};

CgiHandler::CgiHandler(const RequestParser& req, const ServerConfig& config,
                       const std::string& interpreter, const std::string& script_path)
        : cgi(req, config, interpreter, script_path) {
    read_buffer = "";
    write_buffer = "";
}

// read pipe; on EOF -> _owner->on_cgi_done(output, status)
void CgiHandler::on_readable() {
    char buf[2048];
    ssize_t n;

    n = read(cgi.fd[0], buf, sizeof(buf));
    if (n < 0) throw std::runtime_error("Read error");
    read_buffer.append(buf, n);
    if (n == 0)  // EOF, we're done reading
    {
        Log::debug("WERE DONE READING");
        waitpid(cgi.pid, NULL, WNOHANG);
        _owner->on_cgi_done();
    }
}

void CgiHandler::on_writable() {
    ssize_t n;

    n = write(cgi.in_fd[1], write_buffer.data(), write_buffer.size());
    if (n < 0) throw std::runtime_error("Write error");
    write_buffer = write_buffer.substr(0, n);
    if (n == 0)  // EOF, we're done writing
    {
        Log::debug("WERE DONE WRITING");
        cgi.state = CgiProcess::WRITING;
    }
}

void CgiHandler::on_tick(time_t now) {
    bool timed_out = (now - _last_activity > CONNECTION_TIMEOUT_SEC);
    if (timed_out) {
        Log::warning("TIMEOUT CGI");
        finished = true;
    }
}
