*This project has been created as part of the 42 curriculum by dasamuel and wiwu.*

# webserv

## Description

`webserv` is a non-blocking HTTP/1.0 server written from scratch in C++98. It serves static
websites, handles file uploads and deletions, executes CGI scripts (Python, PHP, shell), and
supports name-based virtual hosts — all driven by a single `poll()` event loop that multiplexes
every client and CGI pipe simultaneously.

The server is configured through an NGINX-inspired configuration file: you declare one or more
`server` blocks, each binding a `host:port` and defining per-route rules (allowed methods, roots,
index files, directory listing, redirections, upload directories, body-size limits, CGI handlers,
and custom error pages). A single process can listen on multiple ports and route requests to the
correct virtual host based on the `Host` header.

The architecture is layered: `Config` parses and validates the configuration; the `EventLoop`
owns every file descriptor and dispatches readiness events to `AEventHandler` subclasses
(`Server`, `Connection`, `CgiHandler`); `RequestParser` turns raw bytes into a structured request
through a small state machine; and `ResponseBuilder` produces the HTTP response, including the
CGI bridge. Resource ownership is centralised in the event loop, which frees every handler and
closes every descriptor on shutdown (verified leak-free under Valgrind).

## Instructions

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

If no configuration file is given, the server falls back to `configs/default.conf`.

```bash
./webserv configs/default.conf   # multi-port demo (8080 + 8081), CGI, uploads, redirects
./webserv configs/upload.conf    # single-server upload-focused config
```

Then open a browser at `http://127.0.0.1:8080/`, or test from the command line:

```bash
curl http://127.0.0.1:8080/                              # static GET
curl -X POST --data "hello" http://127.0.0.1:8080/uploads/note.txt   # upload
curl -X DELETE http://127.0.0.1:8080/uploads/note.txt    # delete
curl http://127.0.0.1:8080/cgi-bin/python.py             # CGI
```

Stop the server with `Ctrl-C` (SIGINT) for a clean, leak-free shutdown.

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

## Technical choices

- **One `poll()` for everything.** Listening sockets, client sockets, and CGI pipes share a
  single `poll()` call that watches read and write readiness simultaneously. No descriptor is
  ever read or written without first being reported ready, and `errno` is never inspected after
  a socket `read`/`write` to steer behaviour.
- **Centralised ownership.** The `EventLoop` is the sole owner of every handler it is told to
  own. Handlers flag themselves `finished`; the loop reaps them in a dedicated cleanup pass,
  closing their descriptors and deleting them once their last fd is gone. This deferred-deletion
  model avoids use-after-free during event dispatch.
- **CGI as an async handler.** A CGI request spawns a child via `fork`/`execve`, registers its
  two pipe ends with the loop, and idles the client socket until the script finishes — so a slow
  script never blocks other clients. A timeout kills runaway children.

## Resources

Classic references used while building the project:

- RFC 1945 — *Hypertext Transfer Protocol — HTTP/1.0*:
  http://abcdrfc.free.fr/rfc-vf/rfc1945.html
- RFC 3875 — *The Common Gateway Interface (CGI) Version 1.1*:
  https://fr-academic.com/dic.nsf/frwiki/399061
- `man 2 poll`, `man 2 socket`, `man 2 accept`, `man 2 fork`, `man 2 execve`,
  `man 7 socket` — POSIX system-call references
- NGINX `server` block configuration, used as inspiration for the config-file grammar:
  https://docs.nginx.com/nginx/admin-guide/basic-functionality/managing-configuration-files/

### Use of AI

AI tools were used as a reviewing and explanatory aid, not as a code generator for core logic:

- **Documentation:** drafting and structuring this README and the in-repo design notes.
- **Debugging assistance:** reasoning about specific failure modes (e.g. error-page redirect
  chains, body-size enforcement) and suggesting targeted tests to confirm behaviour.
