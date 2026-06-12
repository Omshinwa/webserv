# Log levels

# **What you'll actually learn, ranked by how much of the project it is**

1. **Event-driven network programming.** Sockets, poll(), state machines per connection, buffering reads until you have a complete request, buffering writes until the kernel accepts them. This is ~60% of the difficulty.
2. **HTTP as a wire protocol.** Parsing a text-based request, building a correct response, handling chunked bodies, getting status codes right. ~20%.
3. **Process management for CGI.** fork + execve + pipe to run a child process, then — and this is the tricky part — wiring the pipe fds into your same poll loop so the CGI's stdout doesn't block your server. ~15%.
4. **Config parsing.** NGINX-style config file (server blocks, location blocks, methods, redirects, upload dirs, CGI extensions). ~5%, mostly tedium.

HTTP-message   = start-line CRLF
                   *( field-line CRLF )
                   CRLF
                   [ message-body ]

start-line     = request-line / status-line

---

**request-line**   = `method SP request-target SP HTTP-version`

`GET        /path/to/resource      HTTP/1.1`

**status-line** = `HTTP-version SP status-code SP [ reason-phrase ]`


# project file structure

webserv/
├── Makefile
├── README.md
├── configs/              # nginx-style .conf files
│   └── default.conf
├── docs/                 # already there
├── src/
│   ├── main.cpp
│   ├── server/          # Server, event loop, Connection
│   ├── http/            # Request parser, Response builder
│   ├── config/          # config file parser
│   ├── cgi/             # CGI execution
│   └── utils/           # logging, colors, helpers
├── www/                  # default document root
│   ├── index.html
│   ├── errors/          # 404.html, 500.html
│   └── cgi-bin/
└── tests/                # curl/siege scripts, sample requests


Makefile, *.{h, hpp}, *.cpp, *.tpp, *.ipp,
configuration files

All functionality must be implemented in C++ 98.
execve
pipe
strerror, gai_strerror, errno,
dup, dup2, fork,

socketpair, htons, htonl, ntohs, ntohl,

select, poll, epoll (epoll_create, epoll_ctl, epoll_wait),
kqueue (kqueue, kevent),
socket,
accept,
listen,
send,
recv,
chdir, bind, connect,
getaddrinfo, freeaddrinfo
setsockopt, getsockname,
getprotobyname, fcntl,

close, read, write, waitpid,
kill, signal, access, stat, open, opendir, readdir
and closedir.

# Q: do i need to read the RFCs?
# Q: telnet isnt installed?

https://en.wikipedia.org/wiki/List_of_HTTP_status_codes


* to connect in the terminal, use:
`nc localhost 8080`

followed by those lines
GET / HTTP/1.1
Host: localhost
then double Enter

1. let's make a hello world TCP server that accepts one connexion and echoes back.
2. non-blocking + poll() — handle multiple clients simultaneously
3. HTTP parsing
4. serve a static file
5. config file
6. CGI
7. Polish

# Webserv roadmap

## Step 1 — Hello World TCP server [DONE]
- `socket()` → `bind()` → `listen()` → `accept()` → `recv()` → `send()` → `close()`
- Blocking, one client at a time. Just to learn the sockets API.

## Step 2 — Non-blocking + `poll()`
Goal: one thread handles N clients simultaneously. No HTTP yet — keep it an echo server.

- Make every socket non-blocking (`fcntl(fd, F_SETFL, O_NONBLOCK)`).
- Build a `pollfd` array, start with just `server_fd` watching `POLLIN`.
- Main loop: `poll(fds, nfds, -1)` → iterate → dispatch per fd:
  - `server_fd` readable → `accept()`, add new `client_fd` watching `POLLIN`.
  - `client_fd` readable → `recv()`, buffer it. When ready to reply, switch to `POLLOUT`.
  - `client_fd` writable → `send()` buffered response. When done, back to `POLLIN` or close.
  - `recv()` returns 0 → client closed → `close(fd)`, remove from array.
- **Never check `errno` after read/write** (subject forbids it). `-1` = close the connection.
- Keep per-connection state in something like `std::map<int, ConnectionState>` (read buffer, write buffer, parse progress).

**Done when:** two `nc` terminals can chat with the server simultaneously.

## Step 3 — HTTP parsing
- Read until `\r\n\r\n` (end of headers).
- Parse request line: `METHOD SP target SP HTTP/version CRLF`.
- Parse headers into `std::map<std::string, std::string>`.
- If `Content-Length` is present, read that many body bytes.
- If `Transfer-Encoding: chunked`, decode chunks (can defer to CGI step).
- Reply with a hardcoded `HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, world!`.

**Done when:** `curl -v http://localhost:8080/` shows the response.

## Step 4 — Serve a static file
- For GET, map URL path to a file on disk (e.g. `/index.html` → `./www/index.html`).
- Read the file, send it. Set `Content-Type` from extension (`.html`, `.css`, `.png`, ...).
- Status codes:
  - 200 OK
  - 404 if file doesn't exist
  - 403 if can't open
  - 405 if method not allowed for the route
- Default error pages.

**Done when:** a real browser renders a page with images/CSS.

## Step 5 — Config file
Parse an NGINX-style config. Must support at minimum:

- Multiple server blocks, each with `listen <ip>:<port>`.
- Per-route (location) settings:
  - Allowed HTTP methods
  - Root directory
  - Default file (index)
  - Autoindex on/off
  - Upload directory
  - CGI extension
  - Redirect
- Max client body size.
- Default error pages.

Drives everything in steps 3-4 at runtime.

## Step 6 — CGI (hardest single piece)
For routes mapped to a CGI extension (e.g. `.php`, `.py`):

- `fork()` + `execve()` the CGI interpreter.
- Two `pipe()`s: stdin (server → CGI body) and stdout (CGI → server response).
- Set env vars per RFC 3875: `REQUEST_METHOD`, `CONTENT_LENGTH`, `CONTENT_TYPE`, `QUERY_STRING`, `PATH_INFO`, `SCRIPT_NAME`, `SERVER_PROTOCOL`, etc.
- **Add both pipe fds to the poll loop** — never block on CGI I/O.
- Reap child with `waitpid(pid, &status, WNOHANG)`.
- Un-chunk request bodies before passing to CGI (CGI expects EOF for end-of-body).
- If CGI output omits `Content-Length`, EOF marks end of body.

## Step 7 — Polish
- POST file uploads (multipart/form-data or raw body to upload dir).
- DELETE method.
- Directory listing (autoindex).
- Idle-connection timeouts (kick slow clients).
- Stress test with `siege`, `ab`, or a custom Python script — must not crash, must not leak fds.


# Request PARSE, Response BUILD
1. The server reads raw text from the socket.
2. A Request object takes that raw text and parses it into structured data (Method, URI, Headers, Body).
3. The Response class takes the fully parsed Request object (and optionally your server/route Config) in its constructor.
4. The Response class uses that information to check if the route is valid, verify allowed methods, read the requested file (or run CGI), and formulate the final HTTP response string.



# ressources

https://www.ibm.com/docs/en/i/7.2.0?topic=designs-example-nonblocking-io-select

https://www.youtube.com/watch?v=D26sUZ6DHNQ
https://www.ibm.com/docs/en/i/7.4.0?topic=programming-how-sockets-work

in depth:
https://beej.us/guide/bgnet/html/#audience
https://broman.dev/download/The%20Linux%20Programming%20Interface.pdf


### todo

A reasonable class breakdown:

Server — listening sockets + the poll loop
Connection — per-client state: fd, recv/send buffers, parsing state machine
Request / Response — parsed data + response builder
Config / Location — parsed config and route matching
CGI — child process + pipe management



# Ai use:

stupid function writing (like std::cout << addr operator) or color convertor
advices on architecture
refactor (make fd a public attribute)


