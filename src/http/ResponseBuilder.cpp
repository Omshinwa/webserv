#include "ResponseBuilder.hpp"

#include <dirent.h>

#include <cerrno>
#include <cstring>
#include <sstream>

#include "../utils/Log.hpp"
#include "../utils/Utils.hpp"
#include "CgiProcess.hpp"

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
        case 431:
            return "Request Header Fields Too Large";
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

std::string mime_type(const std::string& path) {
    size_t dot = path.rfind('.');

    if (dot == std::string::npos) return "application/octet-stream";

    std::string ext = utils::to_lower(path.substr(dot + 1));
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "js") return "application/javascript";
    if (ext == "json") return "application/json";
    if (ext == "txt") return "text/plain";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "ico") return "image/x-icon";
    if (ext == "pdf") return "application/pdf";
    return "application/octet-stream";
}

std::string build_autoindex(const std::string& dirpath, const std::string& uri) {
    std::ostringstream oss;
    oss << "<!DOCTYPE html><html><head><title>Index of " << uri
        << "</title></head><body><h1>Index of " << uri << "</h1><hr><ul>";
    DIR* dir = opendir(dirpath.c_str());
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL) {
            std::string name = ent->d_name;
            if (name == ".") continue;
            std::string href = uri;
            if (!href.empty() && href[href.size() - 1] != '/') href += "/";
            href += name;
            if (utils::is_directory(utils::join_path(dirpath, name))) name += "/";
            oss << "<li><a href=\"" << href << "\">" << name << "</a></li>";
        }
        closedir(dir);
    }
    oss << "</ul><hr></body></html>";
    return oss.str();
}

bool path_matches(const std::string& uri, const std::string& path) {
    if (!utils::starts_with(uri, path)) return false;
    if (uri.size() == path.size()) return true;     // exact match
    if (path[path.size() - 1] == '/') return true;  // path already ends at a separator
    return uri[path.size()] == '/';                 // real segment boundary
}
}  // namespace

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
    if (best == NULL) {
        Log::error("Couldnt find the page, 404");
        status_code = 404;
    }

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

void ResponseBuilder::handle_get(RequestParser& req, const ServerConfig& config) {
    // check for `return`
    if (location->redirect_code != 0) {
        status_code = location->redirect_code;
        header["location"] = location->redirect_url;
        return;
    }
    // check for root + URI request
    const std::string& root = location->has_root ? location->root : config.root;
    std::string filepath = utils::join_path(root, req.URI);

    if (!utils::file_exists(filepath)) {
        status_code = 404;
        return;
    }

    // is it a directory? if so -> index
    // no index? do autoindex
    if (utils::is_directory(filepath)) {
        const std::string& index = location->has_index ? location->index : config.index;
        std::string index_path = utils::join_path(filepath, index);

        if (!index.empty() && utils::is_regular_file(index_path)) {
            filepath = index_path;  // serve the index file below
        } else {
            bool autoindex =
                    location->has_autoindex ? location->autoindex : config.autoindex;
            if (autoindex) {
                body = build_autoindex(filepath, req.URI);
                header["content-type"] = "text/html";
                header["content-length"] = utils::to_str(body.size());
                Log::event("Generated autoindex for `" + filepath + "`");
                return;
            }
            status_code = 403;  // directory, no index, no autoindex
            return;
        }
    }

    // Must be a regular file at this point.
    if (!utils::is_regular_file(filepath)) {
        status_code = 403;
        return;
    }

    // CGI: does this file's extension map to a configured handler?
    const std::string* interpreter = match_cgi(filepath);
    if (interpreter != NULL) {
        CgiProcess cgi(req, config, *interpreter);
        if (cgi.status_code >= 400)
            status_code = cgi.status_code;
        else
            parse_cgi_response(cgi.output);
        return;
    }

    // Static file: must be readable.
    if (!utils::is_readable(filepath)) {
        status_code = 403;
        return;
    }
    try {
        body = utils::read_file(filepath);
        header["content-type"] = mime_type(filepath);
        header["content-length"] = utils::to_str(body.size());
        Log::event("Successfully built response from `" + filepath + "`");
    } catch (const std::exception& e) {
        status_code = 500;
        Log::error(e.what());
        Log::error(std::strerror(errno));
    }
}

// Match the file's extension against this location's configured cgi handlers.
// Returns the interpreter ("" = run directly) or NULL when it isn't a CGI.
const std::string* ResponseBuilder::match_cgi(const std::string& filepath) const {
    for (std::map<std::string, std::string>::const_iterator it = location->cgi.begin();
         it != location->cgi.end(); ++it) {
        const std::string& ext = it->first;
        if (filepath.size() >= ext.size() &&
            filepath.compare(filepath.size() - ext.size(), ext.size(), ext) == 0)
            return &it->second;
    }
    return NULL;
}

void ResponseBuilder::handle_post(RequestParser& req, const ServerConfig& config) {
    const std::string& root = location->has_root ? location->root : config.root;
    std::string upload_path;

    if (location->has_upload) {
        std::string filename = req.URI;
        size_t slash = filename.rfind('/');
        if (slash != std::string::npos) filename = filename.substr(slash + 1);
        if (filename.empty()) {
            status_code = 400;
            return;
        }
        upload_path = utils::join_path(location->upload_dir, filename);
    } else {
        upload_path = utils::join_path(root, req.URI);
    }

    std::string dir = upload_path.substr(0, upload_path.rfind('/'));
    if (dir.empty()) dir = "/";

    if (!utils::file_exists(dir) || !utils::is_directory(dir)) {
        status_code = 404;
        return;
    }
    if (!utils::is_writable(dir)) {
        status_code = 403;
        return;
    }

    bool existed = utils::file_exists(upload_path);

    if (!utils::write_file(upload_path, req.body)) {
        status_code = 500;
        Log::error(std::strerror(errno));
        return;
    }

    if (existed) {
        status_code = 204;
    } else {
        status_code = 201;
        header["location"] = req.URI;
    }
    Log::event("POST: wrote " + utils::to_str(req.body.size()) + " bytes to `" +
               upload_path + "`");
}

void ResponseBuilder::handle_delete(RequestParser& req, const ServerConfig& config) {
    (void)config;  // unused
    (void)req;     // unused
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

// BORING FUNCTION TO PARSE THE CGI RESPONSE LIKE WE DID FOR THE REQUEST
void ResponseBuilder::parse_cgi_response(std::string raw) {
    // 1. find the header/body separator (tolerant, like you already do)
    size_t sep = raw.find("\r\n\r\n");
    std::string delim = "\r\n";
    if (sep == std::string::npos) {
        sep = raw.find("\n\n");
        delim = "\n";
    }
    // malformed: no blank line
    if (sep == std::string::npos) {
        status_code = 502;
        return;
    }

    // 2. split header block into the map (shared helper)
    std::vector<std::string> lines = utils::split(raw.substr(0, sep), delim);

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string k, v;
        if (utils::parse_http_header_line(lines[i], k, v)) header[k] = v;
    }

    // 3. body is just the rest
    body = raw.substr(sep + delim.size() * 2);
    header["content-length"] = utils::to_str(body.size());

    // 4. CGI-specific: honor "Status:" header -> response code
    status_code = 200;
    if (header.count("status")) {
        // usually a cgi response is something like `Status: 404 Not Found`,
        // we ignore the reason phrase
        const std::string& s = header["status"];
        std::string code_str = s.substr(0, s.find(' '));  // "404 Not Found" -> "404"
        if (!utils::parse_int(code_str, status_code)) status_code = 502;
        header.erase("status");  // <-- see Bug 2
    }

    // optionally: handle Location of the CGI
}

// build the HTTP message
std::string ResponseBuilder::build() {
    if (status_code >= 400) {
        std::string phrase = reason_phrase(status_code);
        std::string code = utils::to_str(status_code);
        std::ostringstream oss;
        oss << "<!DOCTYPE html>\r\n"
            << "<html>\r\n"
            << "<head><title>" << code << " " << phrase << "</title></head>\r\n"
            << "<body>\r\n"
            << "<center><h1>" << code << " " << phrase << "</h1></center>\r\n"
            << "<hr><center>webserv</center>\r\n"
            << "</body>\r\n"
            << "</html>\r\n";
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
