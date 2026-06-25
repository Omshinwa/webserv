#include "RequestParser.hpp"

#include <cerrno>
#include <cstdlib>
#include <string>
#include <vector>

#include "../utils/Log.hpp"
#include "../utils/Utils.hpp"

namespace {
// Just checking that its a valid positive number
bool parse_size_t(const char* s, size_t& output) {
    char* end;
    errno = 0;
    long cl = std::strtol(s, &end, 10);

    if (end == s
        // no digits at all ("", "abc")
        || *end != '\0'     // trailing garbage ("123abc", "12 34")
        || errno == ERANGE  // overflow (number too big for long)
        || cl < 0) {        // negative ("-5")
        return false;
    }
    output = static_cast<size_t>(cl);
    return true;
}
}  // namespace

// The parser shares the buffer with Connection
RequestParser::RequestParser(std::string& buffer)
        : state(INCOMPLETE_HEADER),
          status_code(0),
          config(NULL),
          content_length(0),
          buffer(buffer),
          scan_pos(0) {}

RequestParser::~RequestParser() {}

///

// parse the first line of a header:
// "GET / HTTP/1.0"
void RequestParser::parse_start_line(std::string line) {
    std::vector<std::string> items;

    items = utils::split(line, " ");
    if (items.size() != 3) {
        state = ERROR;
        Log::error("Start line error: " + line);
        return;
    }
    method = items[0];
    URI = items[1];
    protocol = items[2];

    // Split the request target into path + query string. Everything after the
    // first '?' is the query (handed to CGI as QUERY_STRING); file resolution
    // uses the path only, so "/cgi-bin/up.py?name=x" maps to the file
    // "/cgi-bin/up.py" the same way "/index.html?source=google" maps to
    // "/index.html".
    size_t qpos = URI.find('?');
    if (qpos != std::string::npos) {
        query_string = URI.substr(qpos + 1);
        URI = URI.substr(0, qpos);
    }

    URI = utils::normalize_path(URI);
    if (URI.empty() || URI[0] != '/' || URI.find("..") != std::string::npos) {
        state = ERROR;
        status_code = 400;
        return;
    }
}

// parse a single line
void RequestParser::parse_header_line(std::string line) {
    std::string key, value;

    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos) {
        state = ERROR;
        return;
    }
    key = line.substr(0, colon_pos);
    value = line.substr(colon_pos + 1);
    // if theres a space in the key = ERROR
    if (key.find_first_of(" ") != std::string::npos) {
        state = ERROR;
        return;
    }

    key = utils::to_lower(key);
    value = utils::trim(value);
    header[key] = value;
}

// Only call it when we have the full header in buffer
void RequestParser::parse_header(std::string header_data, std::string delim) {
    std::vector<std::string> lines;
    lines = utils::split(header_data, delim);

    for (std::vector<std::string>::iterator it = lines.begin(); it != lines.end(); it++) {
        if (it == lines.begin())
            parse_start_line(*it);
        else
            parse_header_line(*it);
        if (state == ERROR) {
            Log::error("ERROR parsing header on: " + *it);
            return;
        }
    }

    if (!header.count("host")) {
        Log::error("Header error: no host key");
        state = ERROR;
        return;
    }

    // Parse Content-Length now if present
    if (header.find("content-length") == header.end()) {
        Log::debug("No content length!");
    } else {
        Log::debug("Content length: " + header["content-length"]);
        if (!parse_size_t(header["content-length"].c_str(), content_length)) {
            state = ERROR;
            return;
        }
    }

    state = AWAITING_CONFIG;
    Log::debug("HEADER OK");
    Log::event("< " + method + " " + URI + " " + get_header("host"));
}

void RequestParser::parse() {
    if (state == INCOMPLETE_HEADER) {
        size_t header_end = std::string::npos;

        std::string delim = "";
        // we will try `\r\n\r\n` then `\n\n`
        // DELIM will take that value

        // Check for standard \r\n\r\n first
        size_t pos_crlf = buffer.find("\r\n\r\n", scan_pos);
        // Check for tolerant \n\n
        size_t pos_lf = buffer.find("\n\n", scan_pos);

        if (pos_crlf != std::string::npos && pos_crlf < pos_lf) {
            header_end = pos_crlf;
            delim = "\r\n";
        } else if (pos_lf != std::string::npos) {
            header_end = pos_lf;
            delim = "\n";
        }

        if (header_end == std::string::npos) {
            // if we read more than 4 bytes....
            if (buffer.length() > 4) {
                // scan_pos = length we read (minus 4, in case \r\n\r\n was
                // given cut)
                scan_pos = buffer.length() - 4;
                if (buffer.size() > MAX_HEADER_SIZE) {
                    state = ERROR;
                    status_code = 431;
                }
            }
            return;
        }

        parse_header(buffer.substr(0, header_end), delim);
        if (state == ERROR) {
            status_code = 400;
            return;
        }
        buffer = buffer.substr(header_end + delim.length() * 2);
    }
    if (state == INCOMPLETE_BODY) {
        if (buffer.size() < content_length) return;

        body = buffer.substr(0, content_length);
        state = COMPLETE;
    }
}

// Called by the Connection with the virtual host it resolved from get_host().
// This is where we know client_max_body_size, so the 413 check lives here.
void RequestParser::set_config(const ServerConfig& cfg) {
    config = &cfg;
    if (state != AWAITING_CONFIG) return;

    // No Content-Length means no body, so the request is already complete.
    // only true in HTTP 1.0, otherwise chunks
    if (!header.count("content-length")) {
        state = COMPLETE;
        return;
    }
    if (content_length > cfg.client_max_body_size) {
        state = ERROR;
        status_code = 413;
        Log::error("Body size " + utils::to_str(content_length) +
                   " exceeds client_max_body_size " +
                   utils::to_str(cfg.client_max_body_size));
        return;
    }
    state = INCOMPLETE_BODY;
}

std::string RequestParser::get_header(std::string key) const {
    t_dict::const_iterator it = header.find(key);
    if (it == header.end()) return "";
    return it->second;
}
