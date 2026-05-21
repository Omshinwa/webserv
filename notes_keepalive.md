# HTTP/1.1 keep-alive — do I need an IDLE state?

**No, you don't need a new state.** After the response is fully sent in HTTP/1.1, just flip `_state` back to `READING`, reset the send/recv buffers, and the poll loop already handles toggling the fd's events from `POLLOUT` to `POLLIN` (you have that logic in `Server.cpp:156-157`).

So the minimum change is:

```cpp
if (n > 0) {
    _send_offset += n;
    if (_send_offset >= _send_buf.size()) {
        // response fully sent
        _send_buf.clear();
        _send_offset = 0;
        _recv_buf.clear();   // ready for the next request
        _state = READING;
    }
}
```

Then in `Server.cpp:150-157`, drop the `HTTP_VER == "1.0"` close-after-send branch (or keep it gated on the request's `Connection: close` header once you parse those).

---

## When `IDLE` would be useful

You'd add it if you want to **distinguish two situations**:

1. Reading bytes of an actively-in-progress request (`READING`)
2. Waiting for the next request to arrive between requests (`IDLE`)

Why distinguish? **Idle timeouts.** HTTP/1.1 keep-alive without a timeout is a DoS vector — a client can hold connections open forever. Typical servers close after 5-15s of idle time *between* requests, but allow much longer while actively receiving a request. To enforce different timeouts you need to know which situation the connection is in.

If you're not implementing idle timeouts yet, skip `IDLE` and just reuse `READING`. You can always add it later.

**A cleaner enum could also look like this** (if you want to be more explicit about the full lifecycle):

```cpp
enum State {
    IDLE,      // keep-alive: waiting for the next request to start
    READING,   // actively receiving request bytes
    WRITING,   // sending response
    CLOSING    // schedule for removal
};
```

But again — start simple. Reuse `READING` until something specifically needs `IDLE`.
