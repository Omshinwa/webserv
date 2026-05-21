#include "Log.hpp"

#include <iostream>
#include <sstream>

const std::string Log::red = "\033[31m";
const std::string Log::green = "\033[32m";
const std::string Log::yellow = "\033[33m";
const std::string Log::blue = "\033[34m";
const std::string Log::magenta = "\033[35m";
const std::string Log::cyan = "\033[36m";
const std::string Log::white = "\033[37m";
const std::string Log::black = "\033[30m";

const std::string Log::red_bg = "\033[41m";
const std::string Log::green_bg = "\033[42m";
const std::string Log::yellow_bg = "\033[43m";
const std::string Log::blue_bg = "\033[44m";
const std::string Log::magenta_bg = "\033[45m";
const std::string Log::cyan_bg = "\033[46m";
const std::string Log::white_bg = "\033[47m";
const std::string Log::black_bg = "\033[40m";

const std::string Log::bold = "\033[1m";
const std::string Log::underline = "\033[4m";

const std::string Log::reset = "\033[0m";
const std::string Log::nl = "\033[0m\n";

// get a color %n
const std::string Log::c(int n)
{
    static const std::string palette[]
        = { black, red, green, yellow, blue, magenta, cyan };
    static const int N = sizeof(palette) / sizeof(palette[0]);
    return palette[n % N];
}

// get a background color %n
const std::string Log::b(int n)
{
    static const std::string palette[] = { black_bg, red_bg, green_bg,
        yellow_bg, blue_bg, magenta_bg, cyan_bg };
    static const int N = sizeof(palette) / sizeof(palette[0]);
    return c(n) + palette[n % N];
}

// 24bit RGB version (not supported everywhere)
// std::string Log::rgb_to_ansi(int r, int g, int b)
// {
//     std::ostringstream oss;
//     oss << "\033[38;2;" << r << ";" << g << ";" << b << "m";
//     return oss.str();
// }

// auto downscale from 0-255 -> 0-5
std::string Log::rgb_to_ansi(int r, int g, int b)
{
    // map 0-255 -> 0-5 (each step is 51 wide)
    int ri = r * 6 / 256;
    int gi = g * 6 / 256;
    int bi = b * 6 / 256;
    int index = 16 + 36 * ri + 6 * gi + bi;

    std::ostringstream oss;
    oss << "\033[38;5;" << index << "m";
    return oss.str();
}

void Log::debug(const std::string s)
{
    std::cout << rgb_to_ansi(255, 240, 240) << s << nl;
}

void Log::error(const std::string s) { std::cerr << red << s << nl; }