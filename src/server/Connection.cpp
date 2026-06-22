
#include "Connection.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <ctime>

#include "../event/EventLoop.hpp"
#include "../http/ResponseBuilder.hpp"
#include "../utils/Log.hpp"
#include "../utils/Utils.hpp"

namespace {
const time_t CONNECTION_TIMEOUT_SEC = 10;
}  // namespace

// Connection handles the buffers for reading and writing
// It creates the Request and Response
Connection::Connection(EventLoop& event_loop, int fd,
                       const std::vector<ServerConfig>& configs, sockaddr_in addr)
        : IEventHandler(event_loop),
          fd(fd),
          cgi(NULL),
          _send_offset(0),
          request(_recv_buf),
          _configs(configs),
          _active(NULL) {
    touch();
    uint32_t ip = ntohl(addr.sin_addr.s_addr);  // host byte order
    remote_addr = utils::to_str((ip >> 24) & 0xFF) + "." +
                  utils::to_str((ip >> 16) & 0xFF) + "." +
                  utils::to_str((ip >> 8) & 0xFF) + "." + utils::to_str(ip & 0xFF);
}

Connection::~Connection() {
    // The CgiHandler is owned by the event loop, not by us. If one is still in
    // flight when we're torn down (client gone mid-CGI, or shutdown), just drop
    // its back-pointer so it won't call on_cgi_done() into freed memory;
    if (cgi) cgi->_owner = NULL;
}

void Connection::on_tick(time_t now) {
    if (cgi && !cgi->finished) return;

    bool timed_out = (now - _last_activity > CONNECTION_TIMEOUT_SEC);
    if (timed_out) {
        log_event("TIMEOUT Connection Socket FD: " + utils::to_str(fd));
        finished = true;
    }
}

void Connection::on_readable() {
    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        touch();
        buf[n] = '\0';
        _recv_buf.append(buf, n);
        log_info("< RECEIVED file descriptor " + utils::to_str(fd) + ":");
        log_debug(buf);
        request.parse();

        // Once the header is parsed the parser enters AWAITING_CONFIG: the Host
        // is known, so we resolve the virtual host and hand the config back
        // (which drives the COMPLETE / INCOMPLETE_BODY transition and the 413
        // check). Then re-parse to consume the body if it's present.
        if (request.get_state() == RequestParser::AWAITING_CONFIG) {
            _active = &resolve_virtual_host(request.get_header("host"));
            request.set_config(*_active);
            request.parse();
        }
    } else if (n == 0) {
        this->finished = true;  // peer closed cleanly
    } else {
        log_error("recv returned a negative number");
        this->finished = true;  // some error
    }

    switch (request.get_state()) {
        case RequestParser::INCOMPLETE_HEADER:
            break;
        case RequestParser::AWAITING_CONFIG:
            break;
        case RequestParser::INCOMPLETE_BODY:
            break;
        case RequestParser::COMPLETE:
            queue_response();
            break;
        case RequestParser::ERROR:
            queue_response();
            break;
    }

    return;
}

void Connection::on_writable() {
    if (_send_offset >= _send_buf.size()) return;

    const char* buf = _send_buf.data() + _send_offset;
    size_t left = _send_buf.size() - _send_offset;

    ssize_t n = send(fd, buf, left, 0);
    if (n > 0) {
        touch();
        log_event("> SENT to file descriptor " + utils::to_str(fd));
        log_info(buf);

        _send_offset += n;
        // did we send everything
        if (_send_offset >= _send_buf.size())
            finished = true;  // HTTP/1.0-style: close after response
    } else if (n == 0) {
        this->finished = true;
    } else {
        log_error("send returned a negative number");
        this->finished = true;
    }
}

void Connection::queue_response() {
    // _active may be NULL if we errored before parsing the Host header;
    // fall back to the default server block.
    const ServerConfig& config = _active ? *_active : _configs[0];
    request.remote_addr = remote_addr;
    ResponseBuilder response(request, config);

    // Async CGI: hand off to the event loop. The CgiHandler will call
    // on_cgi_done() once the script exits; nothing here blocks.
    if (response.waiting_for_cgi) {
        start_cgi(response.cgi_interpreter, response.cgi_filepath, config);
        return;
    }

    _send_buf = response.build();
    _send_offset = 0;
    // Response is ready: stop waiting to read, start waiting to write. on_writable
    // drains _send_buf and sets finished once the whole response has gone out.
    event_loop.set_events(fd, POLLOUT);
}

void Connection::start_cgi(const std::string& interpreter, const std::string& filepath,
                           const ServerConfig& config) {
    cgi = new CgiHandler(event_loop, request, config, interpreter, filepath);
    cgi->_owner = this;

    // fork()/pipe() failed -> answer immediately (exec_status is a 5xx). The
    // pipes were never registered, so the loop never took ownership: build the
    // error response and free the handler ourselves (~CgiHandler nulls cgi).
    if (cgi->cgi.exec_status >= 400) {
        on_cgi_done(*cgi);
        delete cgi;
        cgi = NULL;
        return;
    }

    // Poll both pipe ends: read the script's stdout, write its stdin. Hand the
    // handler's lifetime to the loop (owned=true): once it's finished with no
    // fds left, the loop closes the pipes and frees it. The client socket goes
    // idle (events 0) until the CGI completes and calls back into on_cgi_done().
    event_loop.register_fd(cgi->cgi.fd[0], POLLIN, cgi, true);
    event_loop.register_fd(cgi->cgi.in_fd[1], POLLOUT, cgi, true);
    event_loop.set_events(fd, 0);
}

void Connection::on_cgi_done(CgiHandler& cgi) {
    touch();
    ResponseBuilder response(cgi);
    _send_buf = response.build();
    _send_offset = 0;
    event_loop.set_events(fd, POLLOUT);
}

const ServerConfig& Connection::resolve_virtual_host(const std::string& host) const {
    // Strip any :port suffix from the Host header before matching.
    std::string name = host.substr(0, host.find(':'));

    for (size_t i = 0; i < _configs.size(); i++) {
        const std::vector<std::string>& names = _configs[i].server_names;
        for (size_t j = 0; j < names.size(); j++)
            if (names[j] == name) return _configs[i];
    }
    return _configs[0];  // default server for this socket
}

//  █████          ███████      █████████   █████████
// ▒▒███         ███▒▒▒▒▒███   ███▒▒▒▒▒███ ███▒▒▒▒▒███
//  ▒███        ███     ▒▒███ ███     ▒▒▒ ▒███    ▒▒▒
//  ▒███       ▒███      ▒███▒███         ▒▒█████████
//  ▒███       ▒███      ▒███▒███    █████ ▒▒▒▒▒▒▒▒███
//  ▒███      █▒▒███     ███ ▒▒███  ▒▒███  ███    ▒███
//  ███████████ ▒▒▒███████▒   ▒▒█████████ ▒▒█████████
// ▒▒▒▒▒▒▒▒▒▒▒    ▒▒▒▒▒▒▒      ▒▒▒▒▒▒▒▒▒   ▒▒▒▒▒▒▒▒▒
//
//

void Connection::log_debug(std::string s) {
    Log::color_idx = fd;
    if (s.size() > 200) {  // only display the first 200 characters
        Log::debug(s.substr(0, 200) + "\n...");
    } else
        Log::debug(s);
}
void Connection::log_info(std::string s) {
    Log::color_idx = fd;
    if (s.size() > 200) {  // only display the first 200 characters
        Log::info(s.substr(0, 200) + "\n...");
    } else
        Log::info(s);
}

void Connection::log_event(std::string s) {
    Log::color_idx = fd;
    Log::event(s);
}

void Connection::log_error(std::string s) {
    Log::color_idx = fd;
    Log::error(s);
}
