#include "Utils.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <climits>
#include <cstdlib>
#include <fstream>
#include <sstream>

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

bool parse_int(const std::string& s, int& out) {
    if (s.empty()) return false;

    char* end = NULL;
    long v = std::strtol(s.c_str(), &end, 10);

    if (end == s.c_str() || *end != '\0') return false;

    if (v < 0 || v > INT_MAX) return false;

    out = static_cast<int>(v);
    return true;
}

bool parse_size(const std::string& s, size_t& out) {
    if (s.empty()) return false;

    char* end = NULL;
    unsigned long mult = 1;
    unsigned long v = std::strtoul(s.c_str(), &end, 10);

    if (end == s.c_str()) return false;

    if ((*end == 'K' || *end == 'k') && *(end + 1) == '\0')
        mult = 1024UL;
    else if ((*end == 'M' || *end == 'm') && *(end + 1) == '\0')
        mult = 1024UL * 1024;
    else if ((*end == 'G' || *end == 'g') && *(end + 1) == '\0')
        mult = 1024UL * 1024 * 1024;
    else
        return false;

    if (v != 0 && mult > ULONG_MAX / v) return false;

    out = static_cast<size_t>((v * mult));
    return true;
}

bool file_exists(const std::string& path) {
    struct stat st;
    return (stat(path.c_str(), &st) == 0);
}

bool is_directory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

bool is_regular_file(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

bool is_readable(const std::string& path) {
    int read = access(path.c_str(), R_OK);
    if (read == 0) return true;
    return false;
}

bool is_writable(const std::string& path) {
    int write = access(path.c_str(), W_OK);
    if (write == 0) return true;
    return false;
}

bool is_executable(const std::string& path) {
    int exec = access(path.c_str(), X_OK);
    if (exec == 0) return true;
    return false;
}

bool write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path.c_str(), std::ios::binary | std::ios::trunc);

    if (!f.is_open()) return false;

    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    return f.good();
}

std::string read_file(const std::string& path) {
    std::ifstream f(path.c_str(), std::ios::binary);
    std::ostringstream oss;

    if (!f.is_open()) throw std::runtime_error("cannot open: " + path);

    oss << f.rdbuf();
    return oss.str();
}

std::string join_path(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;

    bool a_slash = a[a.size() - 1] == '/';
    bool b_slash = b[0] == '/';
    if (a_slash && b_slash) return a + b.substr(1);
    if (!a_slash && !b_slash) return a + "/" + b;
    return a + b;
}

std::string normalize_path(const std::string& path) {
    if (path.empty()) return "/";

    bool trailing_slash = (path.size() > 1 && path[path.size() - 1] == '/');
    std::vector<std::string> parts;
    std::vector<std::string> tokens = split_any(path, "/");

    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i] == ".") continue;
        if (tokens[i] == "..") {
            if (!parts.empty()) parts.pop_back();
        } else
            parts.push_back(tokens[i]);
    }

    std::string out = path[0] == '/' ? "/" : "";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out += "/";
        out += parts[i];
    }
    if (out.empty()) out = "/";
    if (trailing_slash && out != "/" && out[out.size() - 1] != '/') out += "/";
    return out;
}

bool set_non_blocking(int fd) { return (fcntl(fd, F_SETFL, O_NONBLOCK) != -1); }

bool parse_host_port(const std::string& s, std::string& host, int& port) {
    if (s.empty()) return false;

    size_t colon = s.find(':');
    if (colon == std::string::npos) {
        int p;
        if (parse_int(s, p) && p > 0 && p < 65536) {
            host = "0.0.0.0";
            port = p;
            return true;
        }
        host = s;
        port = 8080;
        return true;
    }
    host = s.substr(0, colon);
    if (host.empty()) host = "0.0.0.0";
    int p;
    if (!parse_int(s.substr(colon + 1), p) || p <= 0 || p >= 65536) return false;
    port = p;
    return true;
}
}  // namespace utils
