#ifndef COMMON_H
#define COMMON_H

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
#include <map>
#include <stdexcept>
#include <vector>

#include "Log.hpp"
#include "util.hpp"

std::ostream &operator<<(std::ostream &os, const sockaddr_in &addr);

#endif