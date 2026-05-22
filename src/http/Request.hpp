#ifndef REQUEST_H
#define REQUEST_H

#include "common.h"

class Request {

public:
    Request(std::string);
    ~Request();

private:
    Request();

    // INACCESSIBLE
    Request();
    Request(const Request &);
    Request &operator=(const Request &);
};

#endif