#include "RequestParser.hpp"

#include <string>
#include <vector>

#include "ResponseBuilder.hpp"
#include "utils/Log.hpp"
#include "utils/Utils.hpp"

// The parser shares the buffer with Connexion
RequestParser::RequestParser(std::string& buffer)
        : _state(INCOMPLETE), _status_code(0), buffer(buffer), scan_pos(0) {
    Log::debug("Request Parser Creation");
}

RequestParser::~RequestParser() { Log::debug("Request Parser Destructor"); }

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
    Log::event("PARSE OBJECT:");
    Log::info("method: " + method);
    Log::info("URI: " + URI);
    Log::info("protocol: " + protocol);
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
    if (key.find_first_of(" ") != std::string::npos) {
        _state = ERROR;
        return;
    }

    key = utils::to_lower(key);
    value = utils::trim(value);
    header[key] = value;
    Log::info(key + " : " + value);
}

// Only call it when we have the full header in buffer
void RequestParser::parse_header() {
    std::vector<std::string> lines;
    lines = utils::split(buffer, "\r\n");

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

    Log::event("HEADER OK");
}

void RequestParser::parse() {
    const std::string header_delim = "\r\n\r\n";

    // we dont use split(), i'm remembering scan_pos for the last time
    // we checked, slight optimization
    size_t header_end = buffer.find(header_delim, scan_pos);
    if (header_end == std::string::npos) {
        if (buffer.length() > header_delim.length()) {
            scan_pos = buffer.length() - header_delim.length();
            if (buffer.size() > MAX_HEADER_SIZE) _state = ERROR;
        }
        return;
    }
    buffer.resize(header_end);
    parse_header();
    if (_state == ERROR) {
        _status_code = 400;
        return;
    }

    _state = COMPLETE;

    // TODO: read X bytes from the body
}
