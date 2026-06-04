#ifndef UTILS_HPP
#define UTILS_HPP

#include <sstream>
#include <string>
#include <vector>

namespace utils {

    // string manipulation
    std::string trim(const std::string& s);
    std::string to_lower(std::string s);

    std::vector<std::string>    split(const std::string& s, const std::string& delim);
    std::vector<std::string>    split_any(const std::string& s, const std::string& chars);

    bool    starts_with(const std::string& s, const std::string& prefix);
    bool    ends_with(const std::string& s, const std::string& suffix);

    template <typename T>
    std::string to_str(T v) {
        std::ostringstream oss;
        oss << v;
        return oss.str();
    }

    
}  // namespace Utils

#endif
