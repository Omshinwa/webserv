#ifndef SIGNAL_H
#define SIGNAL_H

#include <csignal>

namespace webserv {
extern volatile sig_atomic_t g_stop;

void setup_signals();
void signal_handler(int signal);
}  // namespace webserv

#endif
