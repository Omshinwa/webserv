#include "./Config.hpp"

#include <set>

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

namespace {
struct Token {
        enum Type { WORD, OPEN_BRACE, CLOSE_BRACE, SEMICOLON, END_OF_FILE };

        Type type;
        std::string value;
        int line;

        Token() : type(END_OF_FILE), line(0) {}
        Token(Type t, const std::string& v, int l) : type(t), value(v), line(l) {}
};

class Tokenizer {
    private:
        const std::string& _src;
        size_t _pos;
        int _line;

        void skip_comment() {
            while (_pos < _src.size() && _src[_pos] != '\n') ++_pos;
        }

        Token read_word() {
            size_t start = _pos;
            int line = _line;

            while (_pos < _src.size()) {
                char c = _src[_pos];
                if (std::isspace(static_cast<unsigned char>(c))) break;
                if (c == '{' || c == '}' || c == ';' || c == '#') break;
                ++_pos;
            }
            return Token(Token::WORD, _src.substr(start, _pos - start), line);
        }

    public:
        Tokenizer(const std::string& src) : _src(src), _pos(0), _line(1) {}

        std::vector<Token> tokenize() {
            std::vector<Token> tokens;

            while (_pos < _src.size()) {
                char c = _src[_pos];
                if (std::isspace(static_cast<unsigned char>(c))) {
                    if (c == '\n') ++_line;
                    ++_pos;
                    continue;
                }
                if (c == '#') {
                    skip_comment();
                    continue;
                }
                if (c == '{') {
                    tokens.push_back(Token(Token::OPEN_BRACE, "{", _line));
                    ++_pos;
                    continue;
                }
                if (c == '}') {
                    tokens.push_back(Token(Token::CLOSE_BRACE, "}", _line));
                    ++_pos;
                    continue;
                }
                if (c == ';') {
                    tokens.push_back(Token(Token::SEMICOLON, ";", _line));
                    ++_pos;
                    continue;
                }
                tokens.push_back(read_word());
            }
            tokens.push_back(Token(Token::END_OF_FILE, "", _line));
            return tokens;
        }
};

class Parser {
    private:
        const std::vector<Token>& _tokens;
        size_t _pos;

        const Token& peek() { return _tokens[_pos]; }

        void fail(const std::string& msg) {
            throw std::runtime_error("config error at line " +
                                     utils::to_str(peek().line) + ": " + msg);
        }

        void expect(Token::Type t, const std::string& msg) {
            if (peek().type != t)
                fail("expected " + msg + ", got '" + peek().value + "'");
            ++_pos;
        }

        std::vector<std::string> parse_args() {
            std::vector<std::string> args;
            while (peek().type == Token::WORD) {
                args.push_back(peek().value);
                ++_pos;
            }
            expect(Token::SEMICOLON, "';' after directive");
            return args;
        }

        LocationConfig parse_location() {
            LocationConfig loc;

            if (peek().type != Token::WORD) fail("expected path after 'location'");
            loc.path = peek().value;
            ++_pos;
            expect(Token::OPEN_BRACE, "'{' after location path");
            while (peek().type != Token::CLOSE_BRACE) {
                if (peek().type == Token::END_OF_FILE)
                    fail("unexpected EOF inside location block");
                if (peek().type != Token::WORD) fail("expected directive");
                std::string name = peek().value;
                ++_pos;
                std::vector<std::string> args = parse_args();
                if (name == "methods" || name == "allow_methods" ||
                    name == "limit_except") {
                    if (args.empty()) fail("'methods' expects at least one method");
                    for (size_t i = 0; i < args.size(); ++i) {
                        std::string m = args[i];
                        if (m != "GET" && m != "POST" && m != "DELETE")
                            fail("invalid method '" + m + "'");
                    }
                    loc.methods = args;
                    loc.has_methods = true;
                } else if (name == "root") {
                    if (args.size() != 1) fail("'root' expects one argument");
                    loc.root = args[0];
                    loc.has_root = true;
                } else if (name == "index") {
                    if (args.size() != 1) fail("'index' expects one argument");
                    loc.index = args[0];
                    loc.has_index = true;
                } else if (name == "autoindex") {
                    if (args.size() != 1 || (args[0] != "on" && args[0] != "off"))
                        fail("'autoindex' expects 'on' or 'off'");
                    loc.autoindex = (args[0] == "on");
                    loc.has_autoindex = true;
                } else if (name == "return" || name == "redirect") {
                    if (args.size() == 1) {
                        loc.redirect_code = 302;
                        loc.redirect_url = args[0];
                    } else if (args.size() == 2) {
                        int code;
                        if (!utils::parse_int(args[0], code) || code < 300 || code > 399)
                            fail("invalid redirect code '" + args[0] + "'");
                        loc.redirect_code = code;
                        loc.redirect_url = args[1];
                    } else
                        fail("'return' expects [code] url");
                } else if (name == "upload_dir" || name == "upload_store") {
                    if (args.size() != 1) fail("'upload_dir' expects one argument");
                    loc.upload_dir = args[0];
                    loc.has_upload = true;
                } else if (name == "cgi") {
                    if (args.size() < 1 || args.size() > 2)
                        fail("'cgi' expects EXTENSION [INTERPRETER]");
                    if (args[0].empty() || args[0][0] != '.')
                        fail("'cgi' extension must start with '.'");
                    loc.cgi[args[0]] = (args.size() == 2) ? args[1] : "";
                } else
                    fail("unknown location directive '" + name + "'");
            }
            ++_pos;
            return loc;
        }

        ServerConfig parse_server() {
            ServerConfig srv;
            bool listen_seen = false;
            while (peek().type != Token::CLOSE_BRACE) {
                if (peek().type == Token::END_OF_FILE)
                    fail("unexpected EOF inside server block");
                if (peek().type != Token::WORD) fail("expected directive");
                std::string name = peek().value;
                int line = peek().line;
                ++_pos;
                if (name == "location") {
                    srv.locations.push_back(parse_location());
                    continue;
                }
                std::vector<std::string> args = parse_args();
                if (name == "listen") {
                    if (listen_seen)
                        fail("duplicate 'listen' in server block at line " +
                             utils::to_str(line));
                    listen_seen = true;
                    if (args.size() != 1) fail("'listen' expects one argument");
                    if (!utils::parse_host_port(args[0], srv.host, srv.port))
                        fail("invalid listen value '" + args[0] + "'");
                } else if (name == "server_name")
                    srv.server_names = args;
                else if (name == "root") {
                    if (args.size() != 1) fail("'root' expects one argument");
                    srv.root = args[0];
                } else if (name == "index") {
                    if (args.size() != 1) fail("'index' expects one argument");
                    srv.index = args[0];
                } else if (name == "client_max_body_size") {
                    if (args.size() != 1 ||
                        !utils::parse_size(args[0], srv.client_max_body_size))
                        fail("invalid client_max_body_size");
                } else if (name == "autoindex") {
                    if (args.size() != 1 || (args[0] != "on" && args[0] != "off"))
                        fail("'autoindex' expects 'on' or 'off'");
                    srv.autoindex = (args[0] == "on");
                } else if (name == "error_page") {
                    if (args.size() < 2) fail("'error_page' expects codes and a path");
                    std::string page = args.back();
                    for (size_t i = 0; i + 1 < args.size(); ++i) {
                        int code;
                        if (!utils::parse_int(args[i], code) || code < 100 || code > 599)
                            fail("invalid error_page code '" + args[i] + "'");
                        srv.error_pages[code] = page;
                    }
                } else
                    fail("unknown directive '" + name + "'");
            }
            ++_pos;
            if (!listen_seen) fail("server block missing 'listen' directive");
            return srv;
        }

    public:
        Parser(const std::vector<Token>& tokens) : _tokens(tokens), _pos(0) {}

        std::vector<ServerConfig> parse() {
            std::vector<ServerConfig> servers;

            while (peek().type != Token::END_OF_FILE) {
                if (peek().type != Token::WORD || peek().value != "server")
                    fail("expected 'server' block");
                ++_pos;
                expect(Token::OPEN_BRACE, "'{' after 'server'");
                servers.push_back(parse_server());
            }
            return servers;
        }
};
}  // namespace

std::vector<ServerConfig> Config::parse(const std::string& path) {
    std::string src;

    try {
        src = utils::read_file(path);
    } catch (const std::exception& e) {
        throw std::runtime_error("cannot read config: " + path);
    }

    Tokenizer tk(src);
    std::vector<Token> tokens = tk.tokenize();
    Parser p(tokens);
    std::vector<ServerConfig> servers = p.parse();

    if (servers.empty())
        throw std::runtime_error("config error: no server blocks defined");

    std::set<std::string> seen;

    for (size_t i = 0; i < servers.size(); ++i) {
        const ServerConfig& s = servers[i];
        std::vector<std::string> names = s.server_names;

        if (names.empty()) names.push_back("");
        for (size_t j = 0; j < names.size(); ++j) {
            std::string key = s.host + ":" + utils::to_str(s.port) + "#" + names[j];
            if (!seen.insert(key).second)
                throw std::runtime_error("config error: duplicate listen+server_name '" +
                                         key + "'");
        }
    }
    return servers;
}

// Group server blocks by "host:port" so each unique endpoint gets its own
// listening socket. Blocks that share an endpoint travel together as that
// socket's virtual hosts; the first one listed stays first and acts as the
// default server for that endpoint.
std::map<std::string, std::vector<ServerConfig> > Config::group_by_host_port(
        const std::vector<ServerConfig>& configs) {
    std::map<std::string, std::vector<ServerConfig> > groups;
    for (size_t i = 0; i < configs.size(); ++i) {
        std::string key = configs[i].host + ":" + utils::to_str(configs[i].port);
        groups[key].push_back(configs[i]);
    }
    return groups;
}
