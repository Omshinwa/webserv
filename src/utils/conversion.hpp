#include <sstream>
#include <string>

template <typename T> std::string to_string(T n)
{
    std::ostringstream oss;
    oss << n;
    return oss.str();
}