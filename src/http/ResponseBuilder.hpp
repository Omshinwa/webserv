#ifndef RESPONSEBUILDER_H
#define RESPONSEBUILDER_H

#include "../config/Config.hpp"
#include "RequestParser.hpp"

// Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
//
class ResponseBuilder {
    public:
    ResponseBuilder(RequestParser&, const ServerConfig&);
    ~ResponseBuilder();

    std::string build();

    //
    // // private
    //

    private:
    std::string protocol;
    int status_code;
    t_dict header;
    std::string body;

    const LocationConfig* location;

    // methods

    void find_location(RequestParser& req, const ServerConfig& config);
    void check_methods(RequestParser& req);
    void handle_method(RequestParser& req, const ServerConfig& config);
    void handle_get(RequestParser& req, const ServerConfig& config);
    void handle_post(RequestParser& req, const ServerConfig& config);
    void handle_delete(RequestParser& req, const ServerConfig& config);
    //
    void parse_cgi_response(std::string cgi_response);

    // INACCESSIBLE
    ResponseBuilder();
    ResponseBuilder(const ResponseBuilder&);
    ResponseBuilder& operator=(const ResponseBuilder&);
};

#endif
