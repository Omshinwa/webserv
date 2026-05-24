#ifndef RESPONSEBUILDER_H
#define RESPONSEBUILDER_H

#include "RequestParser.hpp"

class ResponseBuilder {

public:
    // ResponseBuilder(RequestParser &);
    // ~ResponseBuilder();

    static std::string build(RequestParser &);

private:
    // INACCESSIBLE
    ResponseBuilder();
    ResponseBuilder(const ResponseBuilder &);
    ResponseBuilder &operator=(const ResponseBuilder &);
};

#endif