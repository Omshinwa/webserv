#include "Response.hpp"

#include <fstream>

#include "common.h"

Response::Response(Request req)
{
    // receive the request, build the response
}

std::string Response::to_str()
{
    const std::string filepath = "./www/index.html";
    std::ifstream file(filepath.c_str(), std::ios::binary);
    if (!file.is_open()) {
        std::string body = "<h1>500</h1><p>Failed to open local file.</p>\n";
        std::ostringstream resp;
        resp << "HTTP/1.1 500 Internal Server Error\r\n"
             << "Content-Type: text/html\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << body;
        Log::error("failed to open local file:");
        Log::error(std::strerror(errno));
        return resp.str();
    }

    std::ostringstream body_stream;
    body_stream << file.rdbuf();
    std::string body = body_stream.str();

    std::ostringstream resp;
    resp << "HTTP/1.1 200 OK\r\n"
         << "Content-Type: text/html\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Connection: close\r\n"
         << "\r\n"
         << body;
    std::cout << "Successfully built response from `" << filepath << "`\n";
    return resp.str();
}