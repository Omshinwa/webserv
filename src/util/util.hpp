#ifndef UTIL_HPP
#define UTIL_HPP

#include <sstream>
#include <string>
#include <vector>

namespace util {

template <typename T>
std::string to_string(T n) {
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

inline std::string to_lower(std::string s) {
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = std::tolower(static_cast<unsigned char>(s[i]));
    return s;
}

// remove leading and trailing spaces
inline std::string trim_spaces(std::string s) {
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

inline std::vector<std::string> split(std::string line, std::string del) {
    std::vector<std::string> res;

    std::size_t cut_pos;

    while ((cut_pos = line.find(del)) != std::string::npos) {
        res.push_back(line.substr(0, cut_pos));
        line = line.substr(cut_pos + del.length());
    }
    res.push_back(line.substr(0, cut_pos));
    return res;
}
}
#endif
