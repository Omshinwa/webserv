
#include <cstring>

#include "../cgi/CgiProcess.hpp"
#include "../utils/Utils.hpp"
#include "RequestParser.hpp"
#include "ResponseBuilder.hpp"

namespace {
// parse a single line in a http header
// "Content-Length: 50" -> header[content-length] = 50
bool parse_http_header_line(const std::string& line, std::string& key,
                            std::string& value) {
    // std::string key, value;

    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos) return false;
    key = line.substr(0, colon_pos);
    value = line.substr(colon_pos + 1);
    // if theres a space in the key = ERROR
    if (key.find_first_of(" ") != std::string::npos) return false;

    key = utils::to_lower(key);
    value = utils::trim(value);
    return true;
}

}  // namespace

// Construct the response from a finished CGI handler.
ResponseBuilder::ResponseBuilder(CgiHandler& handler)
        : waiting_for_cgi(false),
          protocol("HTTP/1.0"),
          location(NULL),
          config(handler.cgi.config),
          req(NULL) {  // CGI path: no request to re-route, so error_pages are skipped
    header["connection"] = "close";
    // fork/pipe failed, the script crashed (502), or it timed out (504):
    // build() turns the >= 400 status into an error page.
    if (handler.cgi.exec_status >= 400) {
        status_code = handler.cgi.exec_status;
        return;
    }
    parse_cgi_response(handler.read_buffer);
}

// Parse a raw CGI response (header block + body) into status_code/header/body.
void ResponseBuilder::parse_cgi_response(std::string raw) {
    // 1. find the header/body separator (tolerant, like you already do)
    size_t sep = raw.find("\r\n\r\n");
    std::string delim = "\r\n";
    if (sep == std::string::npos) {
        sep = raw.find("\n\n");
        delim = "\n";
    }
    // malformed: no blank line
    if (sep == std::string::npos) {
        status_code = 502;
        return;
    }

    // 2. split header block into the map (shared helper)
    std::vector<std::string> lines = utils::split(raw.substr(0, sep), delim);

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string k, v;
        if (parse_http_header_line(lines[i], k, v)) {
            if (k == "set-cookie")
                set_cookies.push_back(v);  // keep every cookie, not just the last
            else
                header[k] = v;
        }
    }

    // 3. body is just the rest
    body = raw.substr(sep + delim.size() * 2);
    header["content-length"] = utils::to_str(body.size());

    // 4. CGI-specific: honor "Status:" header -> response code
    status_code = 200;
    if (header.count("status")) {
        // usually a cgi response is something like `Status: 404 Not Found`,
        // we ignore the reason phrase
        const std::string& s = header["status"];
        std::string code_str = s.substr(0, s.find(' '));  // "404 Not Found" -> "404"
        if (!utils::parse_int(code_str, status_code)) status_code = 502;
        header.erase("status");
    }
    // optionally: handle Location of the CGI
}

// Also sets up the variables for the CGI Handler,
// returns it to the Connection.
bool ResponseBuilder::is_cgi_request(const std::string& filepath) {
    // is there an interpreter?
    const std::string* interpreter = NULL;
    for (std::map<std::string, std::string>::const_iterator it = location->cgi.begin();
         it != location->cgi.end(); ++it) {
        const std::string& ext = it->first;
        if (filepath.size() >= ext.size() &&
            filepath.compare(filepath.size() - ext.size(), ext.size(), ext) == 0) {
            interpreter = &it->second;
            break;
        }
    }
    if (interpreter == NULL) return false;
    // note: the interpreter can be equal to the empty string

    // Don't run it here — that would block the reactor. Flag the request as
    // CGI and stash what's needed; Connection::queue_response() forks the script
    // and registers its pipes so the reactor drives it asynchronously.
    waiting_for_cgi = true;
    cgi_interpreter = *interpreter;
    cgi_filepath = filepath;
    return true;
}

// Walk the URI path segments left to right and stop at the first one that
// resolves to a regular file on disk -- the CGI script. Everything after it is
// PATH_INFO (computed later in CgiProcess from req->URI), e.g.
// "/cgi-bin/up.py/foo/bar" resolves to the script "/cgi-bin/up.py". Returns
// true (and dispatches via is_cgi_request) only if that file is a CGI script.
bool ResponseBuilder::dispatch_cgi_path_info(const std::string& root) {
    const std::string& uri = req->URI;
    // Only proper prefixes ending at a '/' boundary; the full URI is handled by
    // the caller and is already known not to be a plain file.
    for (size_t i = 1; i < uri.size(); ++i) {
        if (uri[i] != '/') continue;
        std::string fs = map_path(uri.substr(0, i), root);
        if (!utils::is_regular_file(fs)) continue;
        return is_cgi_request(fs);  // first file wins; PATH_INFO is the remainder
    }
    return false;
}
