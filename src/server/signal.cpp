#include "signal.hpp"

#include <unistd.h>

volatile sig_atomic_t webserv::g_stop = 0;

void webserv::setup_signals() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    // Writing the request body to a CGI that has already exited would otherwise
    // raise SIGPIPE and kill the server; ignore it and handle EPIPE inline.
    signal(SIGPIPE, SIG_IGN);
}

void webserv::signal_handler(int sig) {
    (void)sig;
    g_stop = 1;
}
