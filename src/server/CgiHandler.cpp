#include "CgiHandler.hpp"

#include <stdexcept>

CgiHandler::CgiHandler(const RequestParser& req, const ServerConfig& config,
                       const std::string& interpreter, const std::string& script_path)
        : cgi(req, config, interpreter, script_path), buffer("") {}

// read pipe; on EOF -> _owner->on_cgi_done(output, status)
void CgiHandler::on_readable(int fd) {
    char buf[2048];
    ssize_t n;

    n = read(fd, buf, sizeof(buf));
    if (n < 0) throw std::runtime_error("Read error");
    buffer.append(buf, n);
    if (n == 0)  // EOF, we're done reading
    {
        Log::debug("WERE DONE READING");
        waitpid(cgi.pid, NULL, WNOHANG);
        _owner.on_cgi_done();
    }
}

void CgiHandler::on_writable(int fd) {}
