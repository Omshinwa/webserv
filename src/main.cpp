// POSIX libraries
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h> // AF_INET, sockaddr_in (and for bind() next)
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <vector>

// my libraries
#include "colorC.hpp"

// FUNCTIONS:
// socket, bind, listen, accept, recv, send, close

std::ostream &operator<<(std::ostream &os, const sockaddr_in addr)
{
    uint32_t ip = ntohl(addr.sin_addr.s_addr); // host byte order
    os << "    Address:";
    os << ((ip >> 24) & 0xFF) << "." << ((ip >> 16) & 0xFF) << "."
       << ((ip >> 8) & 0xFF) << "." << (ip & 0xFF) << "\n";

    os << "    Port:   " << ntohs(addr.sin_port) << "\n";
    return os;
}

// Creates a socket, returns the fd
int create_socket()
{
    int fd;
    // AF_INET: IPv4 // AF_INET6: IPv6 // AF_UNIX: local Unix domain socket
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        std::cerr << colorC::red_bg << "socket error" << colorC::nl;
        throw std::runtime_error(std::strerror(errno));
    }
    std::cout << colorC::cyan << "Server Socket FD: " << fd << colorC::nl;

    // Allows to restart without TIME WAIT
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    return fd;
}

void bind_socket(int fd)
{
    struct sockaddr_in addr;

    std::memset(&addr, 0, sizeof(addr));
    // zero it first — there are padding fields
    addr.sin_family = AF_INET; // must match the socket()'s domain
    addr.sin_port = htons(8080); // port, in network byte order!
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // listen on all interfaces

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        std::cerr << colorC::red_bg << "bind error" << colorC::nl;
        throw std::runtime_error(std::strerror(errno));
    }
    std::cout << "BIND LISTENING: \n";
    std::cout << addr;

    // Start listening. Hold at most 10 connections in the queue
    if (listen(fd, 10) < 0) {
        std::cerr << colorC::red_bg << "listen error" << colorC::nl;
        throw std::runtime_error(std::strerror(errno));
    }
}

void do_something(char *buffer) { std::cout << "The message was: " << buffer; }

// poll uses events and revents:
// events  = your question  → "is this fd readable? writable?"
// revents = the answer     → "yes readable / yes writable / hung up / error"
void poll_socket(int server_fd)
{
    std::vector<pollfd> fds;

    // Start with the listening socket
    pollfd listen_pfd;
    listen_pfd.fd = server_fd;
    listen_pfd.events = POLLIN;
    listen_pfd.revents = 0;
    fds.push_back(listen_pfd);

    while (true) {
        int n = poll(&fds[0], fds.size(), -1); // -1 = wait forever
        if (n < 0) {
            std::cerr << colorC::red_bg << "poll error" << colorC::nl;
            throw std::runtime_error(std::strerror(errno));
        }

        // Iterate. Build a list of fds to remove (don't modify while
        // iterating).
        for (size_t i = 0; i < fds.size(); i++) {
            if (fds[i].revents == 0)
                continue;

            if (fds[i].fd == server_fd) {
                // Listening socket is readable -> a client wants to connect
                int client_fd = accept(server_fd, NULL, NULL);
                if (client_fd < 0)
                    continue;

                // Make it non-blocking
                fcntl(client_fd, F_SETFL, O_NONBLOCK);

                pollfd new_pfd;
                new_pfd.fd = client_fd;
                new_pfd.events = POLLIN;
                new_pfd.revents = 0;
                fds.push_back(new_pfd);
            } else {
                // A client fd is ready
                if (fds[i].revents & POLLIN) {
                    char buf[1024];
                    ssize_t n = recv(fds[i].fd, buf, sizeof(buf), 0);
                    if (n <= 0) {
                        close(fds[i].fd);
                        fds.erase(fds.begin() + i);
                        i--; // adjust index after erase
                        continue;
                    }
                    // ... do something with the bytes
                    buf[n] = '\0';
                    do_something(buf);
                    // To switch to "I want to write next":
                    // fds[i].events = POLLOUT;
                }
                if (fds[i].revents & POLLOUT) {
                    // send() pending response
                    // when done: fds[i].events = POLLIN;
                }
                if (fds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                    close(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    i--;
                }
            }
        }
    }
}

// Grab a connection from the queue
int accept_socket(int fd)
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int connection_fd = accept(fd, (struct sockaddr *)&addr, &addrlen);
    if (connection_fd < 0) {
        std::cerr << colorC::red_bg << "accept error" << colorC::nl;
        throw std::runtime_error(std::strerror(errno));
    }
    std::cout << addr;
    std::cout << colorC::cyan << "Client Socket FD: " << connection_fd
              << colorC::nl;
    return (connection_fd);
}

int main()
{
    int server_fd = create_socket();

    bind_socket(server_fd);

    int client_fd;
    client_fd = accept_socket(server_fd);
    // poll_socket(server_fd);
    while (1) {
        // Read from the connection, recv is the read equivalent but
        // specifically for sockets
        char buffer[100];
        ssize_t bytesRead = recv(client_fd, buffer, 100, 0);
        if (bytesRead < 0)
            throw std::runtime_error("read error");
        if (bytesRead == 0)
            break;
        buffer[bytesRead] = '\0';
        // null-terminate exactly at end of data
        do_something(buffer);

        // Send a message to the connection
        std::string response = "OK\n";
        send(client_fd, response.c_str(), response.size(), 0);
    }
    // Close the connections
    close(client_fd);
    close(server_fd);
    std::cout << colorC::white_bg << "PROGRAM END" << colorC::nl;
}