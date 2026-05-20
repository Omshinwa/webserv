#include "colorC.hpp"

#include <iostream>

namespace colorC {

const std::string red = "\033[31m";
const std::string green = "\033[32m";
const std::string yellow = "\033[33m";
const std::string blue = "\033[34m";
const std::string magenta = "\033[35m";
const std::string cyan = "\033[36m";
const std::string white = "\033[37m";
const std::string black = "\033[30m";

const std::string red_bg = "\033[41m";
const std::string green_bg = "\033[42m";
const std::string yellow_bg = "\033[43m";
const std::string blue_bg = "\033[44m";
const std::string magenta_bg = "\033[45m";
const std::string cyan_bg = "\033[46m";
const std::string white_bg = "\033[47m";
const std::string black_bg = "\033[40m";

const std::string bold = "\033[1m";
const std::string underline = "\033[4m";

const std::string reset = "\033[0m";
const std::string nl = "\033[0m\n";

const std::string c(int n)
{
    static const std::string palette[]
        = { red, green, yellow, blue, magenta, cyan };
    static const int N = sizeof(palette) / sizeof(palette[0]);
    return palette[n % N];
}

const std::string b(int n)
{
    static const std::string palette[]
        = { red_bg, green_bg, yellow_bg, blue_bg, magenta_bg, cyan_bg };
    static const int N = sizeof(palette) / sizeof(palette[0]);
    return c(n) + palette[n % N];
}

void print_err(const std::string s) { std::cerr << red << s << nl; }

void print(const int fd, const std::string s) { std::cout << c(fd) << s << nl; }

}
