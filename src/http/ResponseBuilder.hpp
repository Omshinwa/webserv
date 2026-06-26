#ifndef RESPONSEBUILDER_H
#define RESPONSEBUILDER_H

#include <vector>

#include "../cgi/CgiHandler.hpp"
#include "../config/Config.hpp"
#include "RequestParser.hpp"

// Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
class ResponseBuilder {
    public:
        ResponseBuilder(RequestParser& req, const ServerConfig& config);
        ResponseBuilder(CgiHandler&);
        ~ResponseBuilder() {};

        std::string build();

        bool waiting_for_cgi;
        std::string cgi_interpreter;
        std::string cgi_filepath;

    private:
        std::string protocol;
        int status_code;
        t_dict header;
        // Set-Cookie is the one header that legitimately repeats, so it can't
        // live in the single-value header map; each cookie gets its own line.
        std::vector<std::string> set_cookies;
        std::string body;
        const LocationConfig* location;
        const ServerConfig& config;
        // the request can be modified (we modify its URI for redirection like
        // error_pages)
        RequestParser* req;

        // methods
        void find_location();
        void check_methods();
        void handle_method();
        void handle_get();
        void handle_post();
        void handle_delete();
        bool is_cgi_request(const std::string& filepath);
        // Walk the URI to find a CGI script with trailing PATH_INFO
        bool dispatch_cgi_path_info(const std::string& root);
        // Map a URI (or prefix) to a filesystem path, stripping the location
        // prefix before joining `root` (alias semantics).
        std::string map_path(const std::string& uri, const std::string& root) const;

        void parse_cgi_response(std::string raw);

        // INACCESSIBLE
        ResponseBuilder();
        ResponseBuilder(const ResponseBuilder& src);
        ResponseBuilder& operator=(const ResponseBuilder& src);
};

#endif
