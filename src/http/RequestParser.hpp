#ifndef REQUESTPARSER_H
#define REQUESTPARSER_H

#include "common.h"

class RequestParser {

public:
    enum State { INCOMPLETE, COMPLETE, ERROR };

    RequestParser(std::string);
    ~RequestParser();

private:
    State _state;

    // INACCESSIBLE
    RequestParser();
    RequestParser(const RequestParser &);
    RequestParser &operator=(const RequestParser &);
};

#endif