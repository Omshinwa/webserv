#include "server/Server.hpp"
#include "util/Log.hpp"

int main() {
  try {
    Server server;
    server.run();
  } catch (const std::exception& e) {
    Log::error(e.what());
    return 1;
  }
  return 0;
}
