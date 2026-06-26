#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <map>
#include <string>
#include <vector>

struct LocationConfig {
        size_t client_max_body_size;
        std::vector<std::string> methods;
        std::string path;
        std::string root;
        std::string index;
        bool autoindex;
        int redirect_code;
        std::string redirect_url;
        std::string upload_dir;
        std::map<std::string, std::string> cgi;  // cgi extension [interpreter]

        bool has_root;
        bool has_index;
        bool has_autoindex;
        bool has_methods;
        bool has_upload;
        bool has_client_max_body_size;

        LocationConfig();
};

struct ServerConfig {
        std::string host;
        int port;
        std::vector<std::string> server_names;
        size_t client_max_body_size;
        std::map<int, std::string> error_pages;
        std::string root;
        std::string index;
        bool autoindex;
        std::vector<LocationConfig> locations;

        ServerConfig();
};

class Config {
    private:
        Config();
        Config(const Config& src);
        Config& operator=(const Config& src);
        ~Config();

    public:
        static std::vector<ServerConfig> parse(const std::string& path);
        static std::map<std::string, std::vector<ServerConfig> > group_by_host_port(
                const std::vector<ServerConfig>& configs);
};

#endif
