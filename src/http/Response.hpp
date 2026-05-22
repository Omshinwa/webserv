#ifndef RESPONSE_H
#define RESPONSE_H

#include "Request.hpp"

class Response {

public:
    Response(Request);
    ~Response();

    std::string Response::to_str();

private:
    // INACCESSIBLE
    Response();
    Response(const Response &);
    Response &operator=(const Response &);
};

#endif