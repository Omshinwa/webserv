#include "ResponseBuilder.hpp"

#include <dirent.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
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

// Capitalize an HTTP header name: first letter and every letter following a
// '-' is upper-cased, the rest lower-cased. e.g. "content-type" -> "Content-Type".
std::string capitalize_header(std::string s) {
    bool at_word_start = true;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (at_word_start)
            s[i] = static_cast<char>(std::toupper(c));
        else
            s[i] = static_cast<char>(std::tolower(c));
        at_word_start = (s[i] == '-');
    }
    return s;
}
}  // namespace

// find the matching location config
void ResponseBuilder::find_location(RequestParser& req, const ServerConfig& config) {
    const LocationConfig* best = NULL;
    for (std::vector<LocationConfig>::const_iterator it = config.locations.begin();
         it != config.locations.end(); ++it) {
        // we need to find the location with the longest matching path
        if (path_matches(req.URI, it->path)) {
            if (best == NULL || it->path.size() > best->path.size()) best = &*it;
        }
    }
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

    if (handle_cgi(req, config, filepath)) {
        // handle_cgi runs the CGI synchronously and fills in status/body.
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

void ResponseBuilder::handle_post(RequestParser& req, const ServerConfig& config) {
    const std::string& root = location->has_root ? location->root : config.root;

    // POST to an existing CGI script runs it (the body is piped to the script's
    // stdin) rather than treating the request as a file upload.
    std::string script_path = utils::join_path(root, req.URI);
    if (utils::is_regular_file(script_path) && handle_cgi(req, config, script_path)) return;

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
    const std::string& root = location->has_root ? location->root : config.root;
    std::string filepath = utils::join_path(root, req.URI);

    if (!utils::file_exists(filepath)) {
        status_code = 404;
        return;
    }
    if (!utils::is_regular_file(filepath)) {
        status_code = 403;
        return;
    }

    // Check if the parent directory is writable and executable before attempting to
    // delete the file
    std::string parent;
    std::string::size_type slash = filepath.find_last_of('/');
    if (slash == std::string::npos)
        parent = ".";
    else if (slash == 0)
        parent = "/";
    else
        parent = filepath.substr(0, slash);

    if (access(parent.c_str(), W_OK | X_OK) != 0) {
        status_code = 403;
        return;
    }

    if (std::remove(filepath.c_str()) != 0) {
        status_code = 500;
        Log::error(std::strerror(errno));
        return;
    }

    status_code = 204;
    Log::event("DELETE: removed `" + filepath + "`");
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
        : waiting_for_cgi(false), protocol("HTTP/1.0"), location(NULL) {
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
        oss << capitalize_header(it->first) << ": " << it->second << "\r\n";
    }
    oss << "\r\n";
    oss << body;
    return oss.str();
}
