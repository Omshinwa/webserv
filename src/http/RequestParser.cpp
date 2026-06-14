#include "RequestParser.hpp"

#include <cerrno>
#include <cstdlib>
#include <string>
#include <vector>

#include "../utils/Log.hpp"
#include "../utils/Utils.hpp"
#include "ResponseBuilder.hpp"

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

// The parser shares the buffer with Connexion

RequestParser::RequestParser(std::string& buffer)
        : state(INCOMPLETE_HEADER),
          status_code(0),
          config(NULL),
          content_length(0),
          buffer(buffer),
          scan_pos(0) {
    Log::debug("Request Parser Creation");
}

RequestParser::~RequestParser() { Log::debug("Request Parser Destructor"); }

///

// "GET / HTTP/1.1"
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
    // Log::info(key + " : " + value);
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

    if (header.find("host") == header.end()) {
        Log::error("Header error: no host key");
        state = ERROR;
        return;
    }

    // Parse Content-Length now if present, but defer the COMPLETE vs
    // INCOMPLETE_BODY decision to set_config(): we need the resolved config for
    // the body-size (413) check first. Either way the header is done, so we
    // hand off to the Connexion via AWAITING_CONFIG.
    if (header.find("content-length") == header.end()) {
        Log::info("No content length!");
    } else {
        Log::info("Content length: " + header["content-length"]);
        if (!parse_size_t(header["content-length"].c_str(), content_length)) {
            state = ERROR;
            return;
        }
    }
    state = AWAITING_CONFIG;

    Log::event("HEADER OK");
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
        // We only reach INCOMPLETE_BODY after set_config() ran the 413 check,
        // so the body-size limit has already been enforced here.
        if (buffer.size() < content_length) return;

        body = buffer.substr(0, content_length);
        state = COMPLETE;
    }
}

// Called by the Connexion with the virtual host it resolved from get_host().
// This is where we know client_max_body_size, so the 413 check lives here.
void RequestParser::set_config(const ServerConfig& cfg) {
    config = &cfg;
    if (state != AWAITING_CONFIG) return;

    // No Content-Length means no body, so the request is already complete.
    if (header.find("content-length") == header.end()) {
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
