#include <fstream>
#include <sstream>

#include "common.h"

// this file defines what we do when the polling is ready

// recv = read

// serv = write

// when the sockets are ready, we handle received data
// return false if an error occured
bool receive_from_client(int fd)
{
    // first read from the fd:
    char buf[1024];
    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n < 0)
        std::cerr << colorC::red_bg << "poll error" << std::strerror(errno)
                  << colorC::nl;
    if (n <= 0)
        return false;
    // ... do something with the bytes
    buf[n] = '\0';
    std::cout << colorC::b(fd) << "  > received from FD " << fd << colorC::nl;
    colorC::print(fd, buf);
    return true;
}

#include <unistd.h>

#include <cerrno>
#include <cstring>
bool send_to_client(int fd)
{
    std::string body = "Hello, world!\n";
    std::ostringstream resp;
    resp << "HTTP/1.1 200 OK\r\n"
         << "Content-Type: text/plain\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "\r\n"
         << body;
    send(fd, resp.str().c_str(), resp.str().size(), 0);
    std::cout << colorC::b(fd) << "  > sent to FD " << fd << colorC::nl;
    colorC::print(fd, resp.str().c_str());

    // std::string filepath = "./www/response";
    // std::ifstream file(filepath.c_str());
    // // note: if using fstream, gotta `std::ios::in | std::ios::binary);`
    // // std::ios::binary (so \r\n doesn't get translated on text mode reads) ?
    // if (!file.is_open()) {
    //     colorC::print_err("failed to open local file:");
    //     colorC::print_err(std::strerror(errno));
    //     return false;
    // }
    // char buf[4096]; // 4096 is a typical page size, not 1028
    // while (file.read(buf, sizeof(buf)) || file.gcount() > 0) {
    //     std::streamsize n = file.gcount();
    //     ssize_t sent = send(fd, buf, n, 0);
    //     std::cout << colorC::c(fd) << "  > sent to FD " << fd;
    //     colorC::print(fd, buf);
    //     if (sent < 0) { /* handle error */
    //         break;
    //     }
    //     // in real code: loop if sent < n (partial send)
    // }
    std::cout << "Successfully served file \n";
    return true;
}