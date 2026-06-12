#include "signal.hpp"

#include <unistd.h>

volatile sig_atomic_t webserv::g_stop = 0;

void webserv::setup_signals() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

void webserv::signal_handler(int sig) {
    (void)sig;
    g_stop = 1;
}
