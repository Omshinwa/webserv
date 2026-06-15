#ifndef UTILS_HPP
#define UTILS_HPP

#include <sstream>
#include <string>
#include <vector>

namespace utils {

// string manipulation
std::string trim(const std::string& s);
std::string to_lower(std::string s);
std::string capitalize_header(std::string s);

std::vector<std::string> split(const std::string& s, const std::string& delim);
std::vector<std::string> split_any(const std::string& s, const std::string& chars);

bool starts_with(const std::string& s, const std::string& prefix);
bool ends_with(const std::string& s, const std::string& suffix);

template <typename T>
std::string to_str(T v) {
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

// numeric parsing - returns false on failure
bool parse_int(const std::string& s, int& out);
bool parse_size(const std::string& s, size_t& out);

bool parse_http_header_line(const std::string& line, std::string& key,
                            std::string& value);

// filesystem
bool file_exists(const std::string& path);
bool is_directory(const std::string& path);
bool is_regular_file(const std::string& path);
bool is_readable(const std::string& path);
bool is_writable(const std::string& path);
bool is_executable(const std::string& path);
bool write_file(const std::string& path, const std::string& content);
std::string read_file(const std::string& path);
std::string join_path(const std::string& a, const std::string& b);
std::string normalize_path(const std::string& path);

// net helpers
bool set_non_blocking(int fd);
bool parse_host_port(const std::string& s, std::string& host, int& port);
}  // namespace utils

#endif
