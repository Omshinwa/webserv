*This project has been created as part of the 42 curriculum by dasamuel, wiwu.*

# webserv

A small, non-blocking **HTTP/1.0 web server** written in C++98, configured
through an nginx-inspired configuration file.

## Description

`webserv` is a from-scratch HTTP server. Its goal is to understand how a real
web server works at the system-call level: opening listening sockets, accepting
clients, parsing HTTP requests, building responses, and serving content — all
while never blocking on a single file descriptor.

The whole server is driven by **one `poll()`-based event loop**. Every file
descriptor the program cares about (listening sockets, client connections, and
CGI pipes) is registered with the loop as an *event handler*. An *event* is
simply a file descriptor becoming readable or writable; the loop wakes up,
dispatches the event to the handler that owns the fd, and goes back to sleep.
This means a single thread serves many simultaneous clients without ever
blocking.

High-level component flow:

```
Log, Utils
   └─> Config
          └─> RequestParser, ResponseBuilder
                 └─> Connection
                        └─> Server
                               └─> EventLoop ──> CgiHandler / CgiProcess
                                      └─> main
```

## Features

- **HTTP/1.0** request parsing and response building.
- **Single-threaded, non-blocking** I/O via a `poll()` event loop — no fd is
  ever read or written in a blocking call.
- **Multiple servers & virtual hosts**: several `server` blocks, resolved by
  `host:port` and `server_name`.
- **HTTP methods**: `GET`, `POST`, `DELETE`.
- **Static file serving** with a configurable `root` and `index`.
- **Directory listing** (`autoindex on`).
- **File uploads** to a configurable `upload_dir`.
- **Custom error pages** (`error_page`).
- **Redirections** (`return 301`/`302 <url>`).
- **Request body size limit** (`client_max_body_size`, returns `413`).
- **Asynchronous CGI** (e.g. Python, shell, PHP, compiled binaries) wired into
  the same event loop — the CGI child's stdin/stdout are non-blocking pipes
  registered as events, so a slow script never blocks the server.

## Instructions

### Requirements

- A C++ compiler supporting **C++98** (`c++` / `g++` / `clang++`)
- `make`
- A POSIX system (Linux/macOS)

### Build

```sh
make            # build ./webserv
make clean      # remove object files
make fclean     # remove objects and the binary
make re         # rebuild from scratch
```

### Run

```sh
./webserv [config_file]
```

If no configuration file is given, `configs/default.conf` is used.

```sh
./webserv configs/default.conf
```

Then open a browser (or use `curl`) against one of the configured
`host:port` pairs, e.g. <http://127.0.0.1:8080>.

```sh
curl -v http://127.0.0.1:8080/
curl -X POST --data-binary @file http://127.0.0.1:8080/uploads
curl http://127.0.0.1:8080/cgi-bin/python.py
```

## Configuration

The server reads an nginx-like configuration file. Each `server { }` block
describes one virtual host, and each `location { }` block overrides behaviour
for a URL prefix.

```nginx
server {
    listen 127.0.0.1:8080;
    server_name localhost;

    root www/website;
    index index.html;
    client_max_body_size 10000;
    autoindex off;
    error_page 404 /errors/404.html;

    location / {
        methods GET;
    }

    location /cgi-bin/ {
        methods GET POST;
        cgi .py;            # run *.py through the matching interpreter
        cgi .sh;
        cgi .php php;       # optional explicit interpreter
    }

    location /uploads {
        methods GET POST DELETE;
        upload_dir www/website/uploads;
        autoindex on;
    }

    location /google {
        methods GET;
        return 302 https://www.google.com/;
    }
}
```

| Directive              | Scope            | Description                                            |
|------------------------|------------------|--------------------------------------------------------|
| `listen <host:port>`   | server           | Address and port to bind.                              |
| `server_name`          | server           | Virtual host name(s) matched against the `Host` header.|
| `root`                 | server/location  | Directory served for this scope.                       |
| `index`                | server/location  | Default file served for a directory.                   |
| `client_max_body_size` | server           | Max request body in bytes (else `413`).                |
| `error_page <code> <p>`| server           | Custom error page for a status code.                   |
| `autoindex on\|off`    | server/location  | Enable/disable directory listing.                      |
| `methods`              | location         | Allowed HTTP methods (`GET POST DELETE`).              |
| `upload_dir`           | location         | Destination directory for uploaded files.              |
| `cgi <ext> [interp]`   | location         | Run files with `<ext>` as CGI (optional interpreter).  |
| `return <code> <url>`  | location         | Issue a redirect.                                      |

See [`configs/default.conf`](configs/default.conf) for a complete example.

## Project structure

```
src/
├── main.cpp            # entry point: parse config, build servers, run loop
├── config/             # configuration file parser (Config)
├── event/              # poll() event loop + signal handling (EventLoop)
├── server/             # listening sockets (Server) and clients (Connection)
├── http/               # request parsing and response building
├── cgi/                # asynchronous CGI execution
└── utils/              # logging and string helpers
configs/                # example configuration files
www/                    # sample websites, CGI scripts, error pages
docs/                   # design notes and a sockets/HTTP/poll manual
```

Additional notes on the internals (sockets, socket fd states, `poll`, and HTTP)
are available in [`docs/manual.md`](docs/manual.md) and
[`docs/design_doc.md`](docs/design_doc.md).

## Resources

Classic references used while building this project:

- **RFC 1945** — *Hypertext Transfer Protocol — HTTP/1.0*
- **RFC 3875** — *The Common Gateway Interface (CGI) Version 1.1*
- **Beej's Guide to Network Programming** — <https://beej.us/guide/bgnet/>
- Linux man pages: `socket(2)`, `bind(2)`, `listen(2)`, `accept(2)`,
  `poll(2)`, `fcntl(2)`, `recv(2)`, `send(2)`, `fork(2)`, `execve(2)`,
  `pipe(2)`, `waitpid(2)`
- **nginx documentation** — for configuration-file syntax and semantics:
  <https://nginx.org/en/docs/>
- **MDN Web Docs — HTTP** — <https://developer.mozilla.org/en-US/docs/Web/HTTP>

### Use of AI

AI assistance (Claude) was used as a support tool, not as a replacement for
our own implementation. Concretely, it helped with:

- **Documentation**: drafting and structuring this `README.md` and the notes in
  `docs/`.
- **Explanations**: clarifying socket lifecycle states, the semantics of
  `poll()` flags (`POLLIN`/`POLLOUT`/`POLLHUP`/`POLLERR`), and CGI environment
  conventions.
- **Debugging**: rubber-ducking edge cases in the event loop, such as treating
  `POLLHUP` as readable and handling a client disconnecting mid-CGI.
- **Code review**: sanity-checking C++98 compliance and resource ownership
  (which object owns/frees each fd and handler).

All architectural decisions, the configuration parser, the event loop, and the
HTTP/CGI logic were designed, written, and validated by us.
