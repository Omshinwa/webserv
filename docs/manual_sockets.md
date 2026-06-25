This is a document that describes what I learned about sockets and polls.

## Contents
- [Sockets](#sockets)
  - [Socket type](#socket-type)
- [POLL](#poll)

# **Sockets**
Sockets are an abstraction provided by the OS to enable communication between different processes either on the same machine or over a network. They act as endpoints in a two-way communication channel. So that when two machines or two apps need to communicate to each others other the internet or a local network, each side of that communication will create a socket.

## **Socket type**
### Datagram (SOCK_DGRAM)
In Internet Protocol terminology, the basic unit of data transfer is a datagram.
It's also the D in UDP (vs TCP).
### Stream (SOCK_STREAM)
This type of socket is connection-oriented. Establish an end-to-end connection by using the bind(), listen(), accept(), and connect() APIs. SOCK_STREAM sends data without errors or duplication, and receives the data in the sending order.
### Raw (SOCK_RAW)
raw

---

Typical flow:

![Socket flow](socket_flow.png)

---

**`int socket(int __domain, int __type, int __protocol)`**  

    Creates a socket fd (for communication). A socker fd is a fd plus a 5-tuple of state the kernel maintains: (protocol, local IP, local port, remote IP, remote port). Reachable from other processes, other machines, the internet...
    We fill it with those arguments:

    * domain: IPv4  
    * type: stream (TCP)  
    * protocol: 0 (for some type/domain combinaison, protocol allows us to select between several protocols)  

**`int bind(int __fd, const sockaddr *__addr, socklen_t __len)`**  

    Bind the socket fd to an address (IPv4 + port)  
    `sockaddr *__addr`: sockaddr is a struct to define the address+port;  
        we use the `sockaddr_in` struct (which is more specific, for internet socket address) and upcast it as `sockaddr`.  
        `addr.sin_port = htons(8080);` hton = host to network, networks are always big endians, host to network convert the bytes (usually its small endians on CPUs but not always)  
        `addr.sin_addr.s_addr = htonl(INADDR_ANY);` INADDR_ANY = 0  

**`int listen(int __fd, int __n)`**

    Marks the socket as passive, "this socket's role is to receive incoming connexions, not to initiate them". Allocates the accept queue.  
    -> returns a *listening* socket.  

**`int accept(int __fd, sockaddr *__restrict__ __addr, socklen_t *__restrict__ __addr_len)`**
    
    Takes a socket fd and wait for something to connect to it , when a connexion arrives, open a new (client/read) socket to communicate with it.
    ADDR is set to the address and ADD_LEN set to the length  
    -> returns a new *connected* socket.  


**`socketpair(...)`**

    Similar to pipe()->fd[2] but for sockets -> sv[2]. For example, currently CGI process creates two pipes (one for reading and one for writing) resulting in 4 file descriptors and two member fields in_fd[2] and out_fd[2]. A socketpair would be a single member field with 2 fds that are bidirectionnal.

---

**Listening socket (from socket() + bind() + listen())**:

Its only job is to wait for incoming connection requests. You never recv/send data on it. The only thing you do with it is accept(), which pulls the next pending connection off its queue.

**Connection socket (returned by accept())**:

A full-bidirectionnal channel between server and client. You can recv and send on the same fd.


# **POLL**

Poll is what allows you to have a lots of fds, you put them in a poll, only when one has a change
does poll() reacts and allows you to keep going.

poll.revents is a bitmask, the main values to care about:

* POLLIN: Ready to read
* POLLOUT: Writing wont block

* POLLHUP: _Hang up_ Peer closed cleaned, end of conversation

FAILURES

* POLLERR, POLLNVAL

## select() vs poll() vs epoll() vs kqueue()
|   |                                                                                                            |
| ---------- | -------------------------------------------------------------------------------------------------------------------- |
| `select`   | The legacy solution. Ancient, but works everywhere.                                                                  |
| `poll`     | Highly portable across UNIX.                                                                                         |
| `epoll`    | Linux only, but it returns directly the fd that had events. Poll() only tells you if in the poll of fd if anything changed. |
| `kqueue`   | macOS equivalent, can also listen to signals, child process statuses, and asynchronous I/O timers.                   |

These event notification mechanisms drastically reduce CPU usage and latency by avoiding idle waiting or redundant polling.

## What "blocking" means:


A syscall is blocking when it puts your thread to sleep until the operation can complete. The OS suspends your process; the CPU runs other things; you wake up when the kernel has something for you.

For each socket call, "nothing to do" looks different:

* `accept()` blocks when the listen queue is empty (no client is connecting).
* `recv()` blocks when the receive buffer is empty (peer hasn't sent anything).
* `send()` blocks when the kernel's send buffer is full (peer isn't draining fast enough).

A non-blocking fd: instead of sleeping, the syscall returns -1 immediately and sets errno to `EAGAIN`/`EWOULDBLOCK`. The thread keeps running.

**`int fcntl(int fd, int op, ... /* arg */ );`**

    fcntl() does an operation on an fd. To set my fd as non blocking:

    fcntl(fd, F_SETFL, O_NONBLOCK);

    fd         : which fd to operate on
    F_SETFL    : is the 'set' operation.
    O_NONBLOCK : we set the fd as 'non blocking'.

## what is multiplexing?

Multiplexer (MUX): a circuit that selects one of several data inputs and routes it, unchanged, to a single output. 
poll() is an example of multiplexing.

The opposite is demultiplexing (DEMUX). Our Reactor demultiplexes I/O events and dispatches them to handlers.