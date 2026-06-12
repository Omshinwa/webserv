#ifndef RESPONSEBUILDER_H
#define RESPONSEBUILDER_H

#include "RequestParser.hpp"

// Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
class ResponseBuilder {
    public:
    // ResponseBuilder(RequestParser &);
    // ~ResponseBuilder();

    static std::string build(RequestParser&);
    t_dict header;

    private:
    std::string protocol;
    int status_code;
    std::string reason_phrase;

    // INACCESSIBLE
    ResponseBuilder();
    ResponseBuilder(const ResponseBuilder&);
    ResponseBuilder& operator=(const ResponseBuilder&);
};

#endif
