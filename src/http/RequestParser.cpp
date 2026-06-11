#include "RequestParser.hpp"

#include <cstdlib>
#include <string>
#include <vector>

#include "ResponseBuilder.hpp"
#include "utils/Log.hpp"
#include "utils/Utils.hpp"

// The parser shares the buffer with Connexion

RequestParser::RequestParser(std::string& buffer)
        : _state(INCOMPLETE_HEADER), _status_code(0), buffer(buffer), scan_pos(0) {
    Log::debug("Request Parser Creation");
}

RequestParser::~RequestParser() { Log::debug("Request Parser Destructor"); }

///

// "GET / HTTP/1.1"
void RequestParser::parse_start_line(std::string line) {
    std::vector<std::string> items;

    items = utils::split(line, " ");
    if (items.size() != 3) {
        _state = ERROR;
        Log::error("Start line error: " + line);
        return;
    }
    method = items[0];
    URI = items[1];
    protocol = items[2];
    // Log::event("PARSE OBJECT:");
    // Log::info("method: " + method);
    // Log::info("URI: " + URI);
    // Log::info("protocol: " + protocol);
}

void RequestParser::parse_header_line(std::string line) {
    std::string key, value;

    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos) {
        _state = ERROR;
        return;
    }
    key = line.substr(0, colon_pos);
    value = line.substr(colon_pos + 1);
    // if theres a space in the key = ERROR
    if (key.find_first_of(" ") != std::string::npos) {
        _state = ERROR;
        return;
    }

    key = utils::to_lower(key);
    value = utils::trim(value);
    header[key] = value;
    // Log::info(key + " : " + value);
}

// Just checking that its a valid positive number
void RequestParser::parse_content_range() {
    char* end;
    long cl = std::strtol(header["content-length"].c_str(), &end, 10);

    errno = 0;
    if (end == header["content-length"].c_str()
        // no digits at all ("", "abc")
        || *end != '\0'     // trailing garbage ("123abc", "12 34")
        || errno == ERANGE  // overflow (number too big for long)
        || cl < 0) {        // negative ("-5")
        _state = ERROR;
        return;
    }
    content_length = static_cast<size_t>(cl);
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
        if (_state == ERROR) {
            Log::error("ERROR parsing header on: " + *it);
            break;
        }
    }

    if (header.find("host") == header.end()) {
        Log::error("Header error: no host key");
        _state = ERROR;
        return;
    }

    // if theres no content-length key, we have the full request
    if (header.find("content-length") == header.end()) {
        Log::info("No content length!");
        _state = COMPLETE;
    } else {
        Log::info("Content length: " + header["content-length"]);
        _state = INCOMPLETE_BODY;
        parse_content_range();
    }

    Log::event("HEADER OK");
}

void RequestParser::parse() {
    if (_state == INCOMPLETE_HEADER) {
        size_t header_end = std::string::npos;

        std::string delim = "";
        // we will successively try \r\n\r\n then \n\n and set
        // DELIM to that one

        // Check for standard \r\n\r\n first
        size_t pos_crlf = buffer.find("\r\n\r\n", scan_pos);
        // Check for tolerant \n\n
        size_t pos_lf = buffer.find("\n\n", scan_pos);

        if (pos_crlf != std::string::npos &&
            (pos_lf == std::string::npos || pos_crlf < pos_lf)) {
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
                if (buffer.size() > MAX_HEADER_SIZE) _state = ERROR;
            }
            return;
        }

        parse_header(buffer.substr(0, header_end), delim);
        buffer = buffer.substr(header_end + delim.length() * 2);
        if (_state == ERROR) {
            _status_code = 400;
            return;
        }
    }
    if (_state == INCOMPLETE_BODY) {
        if (buffer.size() < content_length) return;

        body = buffer.substr(0, content_length);
        _state = COMPLETE;
    }
}
