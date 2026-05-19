OSI: conceptual framework for how networks communicate with each others.

Sockets are an abstraction provided by the OS to enable communication between different processes either on the same machine or over a network. They act as endpoints in a two-way communication channel. So that when two machines or two apps need to communicate to each others other the internet or a local network, each side of that communication will create a socket.

Les sockets sont des canaux de communication qui permettent à des processus non liés d'échanger des données localement et sur des réseaux.

# socket fd states:

| State         | How you get there   | What you can do                                   |
|---------------|---------------------|---------------------------------------------------|
| **Unbound**   | `socket()`          | `bind`, `connect`, `close`                        |
| **Bound**     | `bind()` on unbound | `listen` (server) or `connect` (rare for clients) |
| **Listening** | `listen()` on bound | `accept`, `close` — never `recv`/`send`           |
| **Connected** | `accept()` (server) or `connect()` (client) | `recv`, `send`, `shutdown`, `close` |
| **Shut down (one side)** | `shutdown(fd, SHUT_RD/WR)` | Other direction still works     |
| **Closed**    | `close()`           | fd is gone                                        |


**int socket(int __domain, int __type, int __protocol)**
    creates a socket fd (for communication). A socker fd is a fd plus a 5-tuple of state the kernel maintains: (protocol, local IP, local port, remote IP, remote port). Reachable from other processes, other machines, the internet.
    domain: IPv4
    type: stream (TCP)
    protocol: 0 (for some type/domain combinaison, protocol allows us to select between several protocols)

**int bind(int __fd, const sockaddr *__addr, socklen_t __len)**
    bind the socket fd to an address (IPv4 + port)
    `sockaddr *__addr`: sockaddr is a struct to define the address+port,
        we use the `sockaddr_in` struct (which is more specific, for internet socket address) and upcast it as `sockaddr`.
        `addr.sin_port = htons(8080);` hton = host to network, networks are always big endians, host to network convert the bytes (usually its small endians on CPUs but not always)
        `addr.sin_addr.s_addr = htonl(INADDR_ANY);` INADDR_ANY = 0

**int listen(int __fd, int __n)**
    marks the socket as passive, "this socket's role is to receive incoming connexions, not to initiate them".
    allocates the accept queue.
    -> creates a **listening** socket

**int accept(int __fd, sockaddr *__restrict__ __addr, socklen_t *__restrict__ __addr_len)**
    takes a socket fd and wait for something to connect to it
    when a connexion arrives, open a new (client/read) socket to communicate with it.
    ADDR is set to the address and ADD_LEN set to the length
    returns a new **connected** socket


What each fd actually does
Listening socket (from socket() + bind() + listen()):

Its only job is to wait for incoming connection requests.
You never recv/send data on it. Doing so would error out.
The only thing you do with it is accept(), which pulls the next pending connection off its queue.
Connection socket (returned by accept()):

A full-duplex channel between server and client.
You can recv and send on the same fd.
Each client gets its own. Close it when the conversation is done.
So: