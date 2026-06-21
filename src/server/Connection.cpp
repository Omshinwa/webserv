
#include "Connexion.hpp"

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <ctime>

#include "../http/ResponseBuilder.hpp"
#include "../utils/Log.hpp"
#include "../utils/Utils.hpp"

// Connexion handles the buffers for reading and writing
// It creates the Request and Response
Connexion::Connexion(int fd, const std::vector<ServerConfig>& configs)
        : fd(fd),
          _state(READING),
          _send_offset(0),
          request(_recv_buf),
          _configs(configs),
          _active(NULL),
          _last_activity(time(NULL)) {
    fcntl(fd, F_SETFL, O_NONBLOCK);
}

Connexion::~Connexion() {
    if (fd >= 0) close(fd);
}

Connexion::State Connexion::state() const { return _state; }

void Connexion::mark_closing() { _state = CLOSING; }

void Connexion::touch() { _last_activity = time(NULL); }

bool Connexion::timed_out(time_t now, time_t idle_secs) const {
    return now - _last_activity > idle_secs;
}

void Connexion::on_readable() {
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
        _state = CLOSING;  // peer closed cleanly
    } else {
        log_error("recv returned a negative number");
        _state = CLOSING;  // some error
    }

    // Log::info("parse state: " + utils::to_str(request.get_state()));
    switch (request.get_state()) {
        case RequestParser::INCOMPLETE_HEADER:
            break;
        case RequestParser::AWAITING_CONFIG:
            // Transient: set_config() runs synchronously above, so by here the
            // state has already moved on. Listed to satisfy -Wswitch.
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

void Connexion::on_hangup()

void Connexion::on_writable() {
    if (_send_offset >= _send_buf.size()) return;

    const char* buf = _send_buf.data() + _send_offset;
    size_t left = _send_buf.size() - _send_offset;

    ssize_t n = send(fd, buf, left, 0);
    if (n > 0) {
        touch();
        log_event("> SENT to file descriptor " + utils::to_str(fd));
        log_info(buf);

        _send_offset += n;  // update the offset buffer
        if (_send_offset >= _send_buf.size())
            _state = CLOSING;  // HTTP/1.0-style: close after response
    }

    if (n < 0) _state = CLOSING;
}

void Connexion::queue_response() {
    // _active may be NULL if we errored before parsing the Host header (e.g. a
    // 400 on a malformed header); fall back to the default server block.
    const ServerConfig& config = _active ? *_active : _configs[0];
    request.remote_addr = remote_addr;
    ResponseBuilder response(request, config);

    // is it running a CGI?
    if (response.waiting_for_cgi) {
        _state = CGI_RUNNING;
        return;
    }
    on_cgi_done();
}

void Connexion::on_cgi_done() {
    ResponseBuilder response(cgi.buffer);
    _send_buf = response.build();
    _send_offset = 0;
    _state = WRITING;
}

const ServerConfig& Connexion::resolve_virtual_host(const std::string& host) const {
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

void Connexion::log_debug(std::string s) {
    Log::color_idx = fd;
    if (s.size() > 200) {  // only display the first 200 characters
        Log::debug(s.substr(0, 200) + "\n...");
    } else
        Log::debug(s);
}
void Connexion::log_info(std::string s) {
    Log::color_idx = fd;
    if (s.size() > 200) {  // only display the first 200 characters
        Log::info(s.substr(0, 200) + "\n...");
    } else
        Log::info(s);
}

void Connexion::log_event(std::string s) {
    Log::color_idx = fd;
    Log::event(s);
}

void Connexion::log_error(std::string s) {
    Log::color_idx = fd;
    Log::error(s);
}
