
#include "Utils.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cctype>
#include <cstdlib>

// STRING UTILS
namespace utils {

// remove the spaces from `s`
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");

    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string to_lower(std::string s) {
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    return s;
}

std::vector<std::string> split(const std::string& s, const std::string& delim) {
    std::vector<std::string> out;

    if (delim.empty()) {
        out.push_back(s);
        return out;
    }
    size_t start = 0;
    size_t pos;
    while ((pos = s.find(delim, start)) != std::string::npos) {
        out.push_back(s.substr(start, pos - start));
        start = pos + delim.size();
    }
    out.push_back(s.substr(start));
    return out;
}

std::vector<std::string> split_any(const std::string& s, const std::string& chars) {
    std::vector<std::string> out;
    size_t i = 0;
    size_t start;

    while (i < s.size()) {
        while (i < s.size() && chars.find(s[i]) != std::string::npos) i++;
        start = i;
        while (i < s.size() && chars.find(s[i]) == std::string::npos) i++;
        if (start < i) out.push_back(s.substr(start, i - start));
    }
    return out;
}

bool starts_with(const std::string& s, const std::string& prefix) {
    if (prefix.size() > s.size()) return false;

    for (size_t i = 0; i < prefix.size(); ++i) {
        if (s[i] != prefix[i]) return false;
    }
    // more idiomatic is just using .compare()
    // return s.compare(0, prefix.size(), prefix) == 0;
    return true;
}

bool ends_with(const std::string& s, const std::string& suffix) {
    if (suffix.size() > s.size()) return false;

    size_t startIndex = s.size() - suffix.size();

    for (size_t i = 0; i < suffix.size(); ++i) {
        if (s[startIndex + i] != suffix[i]) return false;
    }
    return true;
}

// specific utils

// Capitalize an HTTP header name: first letter and every letter following a
// '-' is upper-cased, the rest lower-cased. e.g. "content-type" -> "Content-Type".
std::string capitalize_header(std::string s) {
    bool at_word_start = true;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (at_word_start)
            s[i] = static_cast<char>(std::toupper(c));
        else
            s[i] = static_cast<char>(std::tolower(c));
        at_word_start = (s[i] == '-');
    }
    return s;
}

// parse a single line in a http header
// from "Content-Length: 50"
// -> header[content-length] = 50
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

}  // namespace utils
