*This project has been created as part of the 42 curriculum by dasamuel and wiwu.*

# webserv

## Description

`webserv` is a non-blocking HTTP/1.0 server written from scratch in C++98. It serves static
websites, handles file uploads and deletions, executes CGI scripts (Python, PHP, shell), and
supports name-based virtual hosts — all driven by a **one `poll()`-based reactor** that multiplexes
every client and CGI pipe simultaneously.

The server is configured through an NGINX-inspired configuration file: you declare one or more
`server` blocks, each binding a `host:port` and defining per-route rules. 
A single process can listen on multiple ports and route requests to the correct virtual host based on the `Host` header.

## Features

- **HTTP/1.0** protocol implementation. `GET`, `POST`, `DELETE` methods implemented.
- **Single-threaded, non-blocking** I/O via a `poll()` reactor — no fd is
  ever read or written in a blocking call.
- **Multiple servers & virtual hosts**: several `server` blocks, resolved by
  `host:port` and `server_name`.
- **Static file serving** with a configurable `root` and `index`.
- **File uploads** to a configurable `upload_dir`.
- **Directory listing** (`autoindex on`).
- **Custom error pages** (`error_page`).
- **Redirections** (`return 301`/`302 <url>`).
- **Request body size limit** (`client_max_body_size`, returns `413`).
- **Asynchronous CGI** (e.g. Python, shell, PHP, compiled binaries) wired into
  the same reactor — the CGI child's stdin/stdout are non-blocking pipes
  registered as events, so a slow script never blocks the server.


## Feature list

- HTTP methods: `GET`, `POST`, `DELETE` (with per-route allow-lists; `405` otherwise)
- Static file serving with MIME-type detection
- File upload and deletion to configurable directories
- CGI execution by file extension (Python, PHP, shell), with full request environment
- Directory listing (autoindex) and default index files
- HTTP redirections (`301` / `302`)
- Custom and generated default error pages
- Client body-size limits (`413`)
- Multiple listening ports and name-based virtual hosts
- Single non-blocking `poll()` loop; per-connection and per-CGI timeouts

## Instructions

### Requirements

- A C++ compiler supporting **C++98** (`c++` / `g++` / `clang++`)
- `make`
- A POSIX system (Linux/macOS)

### Build

```bash
make            # builds ./webserv
make clean      # removes object files
make fclean     # removes objects and the binary
make re         # full rebuild
```

The project compiles with `c++ -Wall -Wextra -Werror -std=c++98` and performs no unnecessary
relinking.

### Run

```bash
./webserv [configuration_file]
```

If no configuration file is given, `configs/default.conf` is used.

Inside is an example default configuration file to serve our example website.

If the program is correctly running, you'll see a message in the form of:

```
[12:25:44] Server Listening to: 127.0.0.1:8080, fd:3
```
That means the server is correctly running. Then open a browser at `http://127.0.0.1:8080/` (or whichever port you configured it to).

### Stopping the server

Stop the server with `Ctrl-C` (SIGINT) for a clean, leak-free shutdown.

## Configuration

The server needs an nginx-like configuration file. Each `server { }` block
describes one virtual host, and each `location { }` block overrides behaviour
for a URL prefix.

For example if I wanted to serve files in my `./www/website/` folder, I would write a configuration file as such: 

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
| `cgi <extension> [interp]`   | location         | Run files with `<ext>` as CGI (optional interpreter).  |
| `return <code> <url>`  | location         | Issue a redirect.                                      |

See [`configs/default.conf`](configs/default.conf) for an example.

## Project structure

```
src/
├── main.cpp            # entry point: parse config, build servers, run loop
├── config/             # configuration file parser (Config)
├── event/              # poll() event loop + signal handling (Reactor)
├── server/             # listening sockets (Server) and clients (Connection)
├── http/               # request parsing and response building
├── cgi/                # asynchronous CGI execution
└── utils/              # logging and string helpers
configs/                # example configuration files
www/                    # sample websites, CGI scripts, error pages
docs/                   # design notes and manual
```

## Technical description

### Reactor

The whole server is driven by **one `poll()`-based reactor**. Every file
descriptor the program cares about (listening sockets, client connections, and
CGI pipes) is registered with the reactor as an *event handler*. An *event* is
simply a file descriptor becoming readable or writable; the reactor wakes up,
dispatches the event to the handler that owns the fd, and goes back to sleep.
This means a single thread serves many simultaneous clients without ever
blocking.

- **Centralised ownership.** The `Reactor` is the sole owner of every handler it is told to
  own. Handlers flag themselves `finished`; the loop reaps them in a dedicated cleanup pass,
  closing their descriptors and deleting them once their last fd is gone. This deferred-deletion
  model avoids use-after-free during event dispatch.
- **CGI as an async handler.** A CGI request spawns a child via `fork`/`execve`, registers its
  two pipe ends with the loop, and idles the client socket until the script finishes — so a slow
  script never blocks other clients. A timeout kills runaway children.

## Resources

Classic references used while building this project:

- **RFC 1945** — *Hypertext Transfer Protocol — HTTP/1.0* :  http://abcdrfc.free.fr/rfc-vf/rfc1945.html (FR)
https://datatracker.ietf.org/doc/html/rfc1945 (ENG)
- **RFC 3875** — *The Common Gateway Interface (CGI) Version 1.1* : https://fr-academic.com/dic.nsf/frwiki/399061 (FR)
https://datatracker.ietf.org/doc/html/rfc3875 (ENG)
- **Beej's Guide to Network Programming** — <https://beej.us/guide/bgnet/>
- Linux man pages: `socket(2)`, `bind(2)`, `listen(2)`, `accept(2)`,
  `poll(2)`, `fcntl(2)`, `recv(2)`, `send(2)`, `fork(2)`, `execve(2)`,
  `pipe(2)`, `waitpid(2)`
- **NGINX documentation** — for configuration-file syntax and semantics:
  <https://nginx.org/en/docs/>
- **MDN Web Docs — HTTP** — <https://developer.mozilla.org/en-US/docs/Web/HTTP>
- **Wikipedia** - https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
- **IBM** - <a href="https://www.ibm.com/docs/en/i/7.4.0?topic=programming-how-sockets-work">HOW-SOCKETS-WORK</a> ; <a href="https://www.ibm.com/docs/en/i/7.2.0?topic=designs-example-nonblocking-io-select">NONBLOCKING-IO-SELECT</a> (Note that these were originally written as documentation of IBM i, the OS of IBM)

Less classical references used:

<a href="https://www.youtube.com/watch?v=D26sUZ6DHNQ"> - **99% of Developers Don't Get Sockets (Youtube)** </a>


### Use of AI

AI assistance  was used as a support tool, not as a replacement for
our own implementation. Concretely, it helped with:

- **Documentation**: drafting and structuring this `README.md` and the notes in
  `docs/`.
- **Explanations**: clarifying socket lifecycle states, the semantics of
  `poll()` flags (`POLLIN`/`POLLOUT`/`POLLHUP`/`POLLERR`), and CGI environment
  conventions, etc...
- **Debugging**: handling edge cases in the reactor, such as treating
  `POLLHUP` as readable and handling a client disconnecting mid-CGI.
- **Code review**: sanity-checking C++98 compliance and resource ownership
  (which object owns/frees each fd and handler).

All architectural decisions, the configuration parser, the reactor, and the
HTTP/CGI logic were designed, written, and validated by us.
