#ifndef REQUESTPARSER_H
#define REQUESTPARSER_H

#include <map>
#include <string>

#include "../config/Config.hpp"

typedef std::map<std::string, std::string> t_dict;

class RequestParser {
    public:
        enum State {
            INCOMPLETE_HEADER,
            AWAITING_CONFIG,
            INCOMPLETE_BODY,
            COMPLETE,
            ERROR
        };

        RequestParser(std::string&);
        ~RequestParser();

        void parse();
        inline State get_state() const { return state; };

        void set_config(const ServerConfig& cfg);
        std::string get_header(std::string s) const;
        const t_dict& get_header_ref() const { return header; }

        inline int get_status_code() const { return status_code; };

        std::string method;
        std::string URI;
        std::string protocol;
        std::string body;
        std::string remote_addr;  // client IP, filled in by the Connection

    private:
        static const size_t MAX_HEADER_SIZE = 32000;

        State state;
        t_dict header;
        int status_code;

        // Resolved by the Connection from the Host header; NULL until then.
        const ServerConfig* config;

        size_t content_length;  // parsed content length

        // A bunch of internal variables used to represent a request
        std::string& buffer;
        size_t scan_pos;  // we scan the buffer until we find \r\n\r\n

        void parse_header(std::string header_data, std::string delim);
        void parse_start_line(std::string line);
        void parse_header_line(std::string line);

        // INACCESSIBLE
        RequestParser();
        RequestParser(const RequestParser&);
        RequestParser& operator=(const RequestParser&);
};

#endif
