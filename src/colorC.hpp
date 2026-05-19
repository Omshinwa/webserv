#ifndef COLORC_HPP
#define COLORC_HPP

#include <string>

namespace colorC {

extern const std::string red;
extern const std::string green;
extern const std::string yellow;
extern const std::string blue;
extern const std::string magenta;
extern const std::string cyan;
extern const std::string white;
extern const std::string black;

extern const std::string red_bg;
extern const std::string green_bg;
extern const std::string yellow_bg;
extern const std::string blue_bg;
extern const std::string magenta_bg;
extern const std::string cyan_bg;
extern const std::string white_bg;
extern const std::string black_bg;

extern const std::string bold;
extern const std::string underline;

extern const std::string reset;
extern const std::string nl;

const std::string &c(size_t n);

}

#endif
