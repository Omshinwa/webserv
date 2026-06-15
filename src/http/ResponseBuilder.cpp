#include "ResponseBuilder.hpp"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>

#include "../utils/Log.hpp"
#include "../utils/Utils.hpp"

namespace {
std::string reason_phrase(int status_code) {
    switch (status_code) {
        // 2xx Success
        case 200:
            return "OK";
        case 201:
            return "Created";
        case 204:
            return "No Content";
        // 3xx Redirection
        case 301:
            return "Moved Permanently";
        case 302:
            return "Found";
        // case 303:
        //     return "See Other";
        case 304:
            return "Not Modified";
        // case 307: return "Temporary Redirect";
        // 4xx Client Error
        case 400:
            return "Bad Request";
        case 401:
            return "Unauthorized";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 408:
            return "Request Timeout";
        case 409:
            return "Conflict";
        case 411:
            return "Length Required";
        case 413:
            return "Payload Too Large";
        case 414:
            return "URI Too Long";
        case 415:
            return "Unsupported Media Type";
        // case 431: return "Request Header Fields Too Large";
        // 5xx Server Error
        case 500:
            return "Internal Server Error";
        case 501:
            return "Not Implemented";
        case 502:
            return "Bad Gateway";
        case 503:
            return "Service Unavailable";
        case 504:
            return "Gateway Timeout";
        case 505:
            return "HTTP Version Not Supported";
        default:
            return "Unknown";
    }
}

bool path_matches(const std::string& uri, const std::string& path) {
    if (!utils::starts_with(uri, path)) return false;
    // exact match, or the next char is a '/' (real segment boundary)
    return uri.size() == path.size() || uri[path.size()] == '/';
}
}  // namespace

ResponseBuilder::ResponseBuilder(RequestParser& req, const ServerConfig& config)
        : protocol("HTTP/1.0"), location(NULL) {
    header["connection"] = "close";

    if (req.get_status_code() != 0)  // theres an error
    {
        status_code = req.get_status_code();
        return;
    } else
        status_code = 200;

    find_location(req, config);
    if (status_code >= 400) return;
    check_methods(req);
    if (status_code >= 400) return;
    handle_method(req, config);  // GET / POST / DELETE dispatch
}

ResponseBuilder::~ResponseBuilder() {}

void ResponseBuilder::find_location(RequestParser& req, const ServerConfig& config) {
    // find the matching location:
    const LocationConfig* best = NULL;
    for (std::vector<LocationConfig>::const_iterator it = config.locations.begin();
         it != config.locations.end(); ++it) {
        // if (it->path == req.URI) break;  // too strict
        // we need to find the location with the longest matching path
        // with the URI
        // note that we cant just compare the beginning of strings
        // pages/match != pages/match2 but they would both match
        // location `pages/match`
        if (path_matches(req.URI, it->path)) {
            if (best == NULL || it->path.size() > best->path.size()) best = &*it;
        }
    }
    // couldnt find the location
    if (best == NULL) status_code = 404;
    location = best;
}

void ResponseBuilder::check_methods(RequestParser& req) {
    // find the matching method:
    std::vector<std::string>::const_iterator it;
    for (it = location->methods.begin(); it != location->methods.end(); ++it) {
        if (req.method == *it) break;
    }
    // couldnt find the method
    if (it == location->methods.end()) status_code = 405;
}

void ResponseBuilder::handle_method(RequestParser& req, const ServerConfig& config) {
    if (req.method == "GET")
        handle_get(req, config);
    else if (req.method == "POST")
        handle_post(req, config);
    else if (req.method == "DELETE")
        handle_delete(req, config);
    // 501 if a method you don't implement at all slipped past check_methods
    else
        status_code = 501;
}

void ResponseBuilder::handle_get(RequestParser& res, const ServerConfig& config) {
    // check for `return`
    // check for root + URI request
    // is it a directory? if so -> index
    // no index? do autoindex

    // if error -> check if config has error pages and that files reads OK
    // serve it
    // otherwise use the default generated

    const std::string filepath = "./www/index.html";

    try {
        body = utils::read_file(filepath);
        Log::event("Successfully built response from `" + filepath + "`");
    } catch (const std::exception& e) {
        status_code = 500;
        Log::error(e.what());
        Log::error(std::strerror(errno));
    }

    (void)res;
    (void)config;
}
void ResponseBuilder::handle_post(RequestParser&, const ServerConfig&) {
    // upload_dir
    // TODO
}
void ResponseBuilder::handle_delete(RequestParser&, const ServerConfig&) {
    // TODO
}

// build the HTTP message
std::string ResponseBuilder::build() {
    if (status_code >= 400) {
        std::ostringstream oss;
        oss << "<html><head><title>" << status_code << " " << reason_phrase(status_code)
            << "</title></head><body><center><h1>" << status_code << " "
            << reason_phrase(status_code)
            << "</h1></center><hr><center>webserv</center></body></html>";
        body = oss.str();
        header["content-type"] = "text/html";
        header["content-length"] = utils::to_str(body.size());
    }

    std::ostringstream oss;
    oss << protocol << " " << status_code << " " << reason_phrase(status_code) << "\r\n";
    for (t_dict::const_iterator it = header.begin(); it != header.end(); ++it) {
        oss << utils::capitalize_header(it->first) << ": " << it->second << "\r\n";
    }
    oss << "\r\n";
    oss << body;
    return oss.str();
}
