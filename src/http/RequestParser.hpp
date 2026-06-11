#ifndef REQUESTPARSER_H
#define REQUESTPARSER_H

#include <map>
#include <string>
typedef std::map<std::string, std::string> t_dict;

class RequestParser {
    public:
    enum State { INCOMPLETE_HEADER, INCOMPLETE_BODY, COMPLETE, ERROR };

    RequestParser(std::string&);
    ~RequestParser();

    t_dict header;

    void parse();
    inline State state() const { return _state; };

    private:
    enum IncompleteState { HEADER, BODY };

    static const size_t MAX_HEADER_SIZE = 32000;

    State _state;

    int _status_code;

    //
    std::string method;
    std::string URI;
    std::string protocol;
    std::string body;

    size_t content_length;

    // A bunch of internal variables used to represent a request
    std::string& buffer;
    size_t scan_pos;  // we scan the buffer until we find \r\n\r\n

    void parse_header(std::string header_data, std::string delim);
    void parse_content_range();
    void parse_start_line(std::string line);
    void parse_header_line(std::string line);

    // LOG
    void log_info(std::string s);

    // INACCESSIBLE
    RequestParser();
    RequestParser(const RequestParser&);
    RequestParser& operator=(const RequestParser&);
};

#endif
