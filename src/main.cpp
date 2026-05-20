#include "common.h"
#include "service.cpp"

std::ostream &operator<<(std::ostream &os, const sockaddr_in addr)
{
    uint32_t ip = ntohl(addr.sin_addr.s_addr); // host byte order
    os << "    Address:";
    os << ((ip >> 24) & 0xFF) << "." << ((ip >> 16) & 0xFF) << "."
       << ((ip >> 8) & 0xFF) << "." << (ip & 0xFF) << "\n";

    os << "    Port:   " << ntohs(addr.sin_port) << "\n";
    return os;
}

// Creates a socket
// binds it
// make it listen
// returns the fd
int create_socket()
{
    int fd;
    // CREATE SOCKET DESCRIPTOR
    {
        // AF_INET: IPv4 // AF_INET6: IPv6 // AF_UNIX: local Unix domain socket
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1) {
            std::cerr << colorC::red_bg << "socket error" << colorC::nl;
            throw std::runtime_error(std::strerror(errno));
        }
        std::cout << colorC::b(fd) << "NEW Server Socket FD: " << fd
                  << colorC::nl;

        // Allows to restart without TIME WAIT
        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        fcntl(fd, F_SETFL, O_NONBLOCK); // Make it non-blocking
    }

    // BIND
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
        std::cout << "Listening to: \n";
        std::cout << addr;
    }

    // LISTENING
    // Start listening. Hold at most 10 connections in the queue
    int MAX_CONNEXION = 10;
    if (listen(fd, MAX_CONNEXION) < 0) {
        std::cerr << colorC::red_bg << "listen error" << colorC::nl;
        throw std::runtime_error(std::strerror(errno));
    }
    return fd;
}

// Grab a connection from the queue
// returns the fd
int accept_socket(int fd)
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int connection_fd = accept(fd, (struct sockaddr *)&addr, &addrlen);
    if (connection_fd < 0) {
        std::cerr << colorC::red_bg << "accept error: " << std::strerror(errno)
                  << colorC::nl;
        return (connection_fd);
    }
    std::cout << colorC::b(connection_fd)
              << "NEW Client Socket FD: " << connection_fd << colorC::nl;
    std::cout << addr;

    fcntl(connection_fd, F_SETFL, O_NONBLOCK); // Make it non-blocking
    return (connection_fd);
}
// poll uses events and revents:
// events  = your question  → "is this fd readable? writable?"
// revents = the answer     → "yes readable / yes writable / hung up / error"
// You could either use epoll (linux only, faster/more optimized), or poll (scan
// is O(N) but unix portable)
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

            // 1. Errors first
            if (fds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                if (fds[i].fd == server_fd) {
                    // listening socket died — fatal
                    std::cerr << colorC::red_bg << "server fatal error"
                              << colorC::nl;
                    throw std::runtime_error("listening socket failed");
                }
                close(fds[i].fd);
                fds.erase(fds.begin() + i);
                i--;
                continue;
            }

            // 2. Reads
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == server_fd) {
                    // Listening socket is readable -> a client wants to connect
                    int client_fd = accept_socket(server_fd);
                    if (client_fd < 0)
                        continue;
                    pollfd new_pfd;
                    new_pfd.fd = client_fd;
                    new_pfd.events = POLLIN;
                    new_pfd.revents = 0;
                    fds.push_back(new_pfd);
                } else {
                    // A client fd is ready
                    if (!receive_from_client(fds[i].fd)) {
                        close(fds[i].fd);
                        fds.erase(fds.begin() + i);
                        i--; // adjust index after erase
                        continue;
                    }
                    // To switch to "I want to write next":
                    fds[i].events = POLLOUT;
                }
            }
            // 3. Writes
            if (fds[i].revents & POLLOUT) {
                // send() pending response
                if (!send_to_client(fds[i].fd) || HTTP_VER == 1.0) {
                    close(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    i--; // adjust index after erase
                    continue;
                }
                // when done: fds[i].events = POLLIN; this is HTTP/1.1 style
                if (HTTP_VER == 1.1)
                    fds[i].events = POLLIN;
            }
        }
    }
}

int main()
{
    std::cout << colorC::white_bg << "SERVER START" << colorC::nl;
    int server_fd = create_socket();
    poll_socket(server_fd);
    std::cout << colorC::white_bg << "PROGRAM END" << colorC::nl;
}