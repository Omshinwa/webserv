# Request PARSE, Response BUILD
1. The server reads raw text from the socket.
2. A Request object takes that raw text and parses it into structured data (Method, URI, Headers, Body).
3. The Response class takes the fully parsed Request object (and optionally your server/route Config) in its constructor.
4. The Response class uses that information to check if the route is valid, verify allowed methods, read the requested file (or run CGI), and formulate the final HTTP response string.


High-level component flow:

```
Log, Utils
   └─> Config
          └─> RequestParser, ResponseBuilder
                 └─> Connection
                        └─> Server
                               └─> Reactor ──> CgiHandler / CgiProcess
                                      └─> main
```