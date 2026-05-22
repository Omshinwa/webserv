#ifndef RESPONSEBUILDER_H
#define RESPONSEBUILDER_H

#include "RequestParser.hpp"

class ResponseBuilder {

public:
    ResponseBuilder(RequestParser);
    ~ResponseBuilder();

    std::string ResponseBuilder::to_str();

private:
    // INACCESSIBLE
    ResponseBuilder();
    ResponseBuilder(const ResponseBuilder &);
    ResponseBuilder &operator=(const ResponseBuilder &);
};

#endif