#ifndef INCLUDE_HPP
#define INCLUDE_HPP

// Debug convenience header: pulls in every project header (and the common
// standard-library ones) in one shot. Drop `#include "include.hpp"` at the top
// of any .cpp while hacking so you don't have to chase individual headers.
//
// NOT for committed code — include only what each file actually needs once the
// dust settles. Paths are relative to src/ (same as main.cpp).

// ── standard library ────────────────────────────────────────────────────────
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ── POSIX / system ──────────────────────────────────────────────────────────
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// ── project ─────────────────────────────────────────────────────────────────

#include "utils/Log.hpp"
#include "utils/Utils.hpp"
//
#include "config/Config.hpp"
//

#include "event/EventLoop.hpp"
#include "event/IEventHandler.hpp"
//
#include "cgi/CgiHandler.hpp"
#include "cgi/CgiProcess.hpp"
#include "http/RequestParser.hpp"
#include "http/ResponseBuilder.hpp"
#include "server/Connection.hpp"
#include "server/Server.hpp"
#include "server/signal.hpp"

// Log, Utils
//    └─> Config
//       └─> Request
//          └─> CGIProcess
//             └─> CGI Handler
//
//               Response
//                  └─> Client
//                         └─> Server
//                                └─> Handler ──> Cgi
//                                       └─> main

#endif
