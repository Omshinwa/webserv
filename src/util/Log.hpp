#ifndef LOG_H
#define LOG_H

#include <string>

class Log {
  public:
    static int color_idx;

    static const std::string red;
    static const std::string green;
    static const std::string yellow;
    static const std::string blue;
    static const std::string magenta;
    static const std::string cyan;
    static const std::string white;
    static const std::string black;

    static const std::string red_bg;
    static const std::string green_bg;
    static const std::string yellow_bg;
    static const std::string blue_bg;
    static const std::string magenta_bg;
    static const std::string cyan_bg;
    static const std::string white_bg;
    static const std::string black_bg;

    static const std::string bold;
    static const std::string underline;

    static const std::string reset;
    static const std::string nl;

    // FUNCTIONS

    static const std::string c(int n);
    static const std::string b(int n);

    static std::string rgb_to_ansi(int r, int g, int b);

    static void debug(const std::string s);
    static void info(const std::string s);
    static void event(const std::string s);
    static void error(const std::string s);

  private:
    Log();  // not instantiable
};

#endif
