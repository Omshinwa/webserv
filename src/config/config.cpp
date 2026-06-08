#include "./config.hpp"
#include "../utils/Utils.hpp"

LocationConfig::LocationConfig()
    : autoindex(false),
      redirect_code(0),
      has_root(false),
      has_index(false),
      has_autoindex(false),
      has_methods(false),
      has_upload(false) {}


ServerConfig::ServerConfig()
    : host("0.0.0.0"),
      port(8080),
      client_max_body_size(1024 * 1024),
      root("./www"),
      index("index.html"),
      autoindex(false) {}



std::vector<ServerConfig>   parse(const std::string& path) {
    std::string src;

    try {
        src = utils::read_file(path);
    }
    catch (const std::exception& e) {
        throw std::runtime_error("cannot read config: " + path);
    }


}
