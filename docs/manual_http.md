
# HTTP

## \r\n vs \n
CRLF means Carriage Return + Line Feed.

In the HTTP/1.1 protocol, all header fields except Host are optional. 
A request line containing only the path name is accepted by servers to maintain compatibility with HTTP clients before the HTTP/1.0 specification in RFC 1945.

 A general-purpose web server is required to implement at least GET and HEAD, and all other methods are considered optional by the specification.[18]: §9.1 

Method names are case sensitive.[4]: §3 [18]: §9.1  This is in contrast to HTTP header field names which are case-insensitive.[18]: §6.3  

**Don't trim the key**. Per RFC 7230 §3.2.4, whitespace between the field name and the colon is forbidden (Host : example.com ← invalid). You should reject it with 400, not silently strip it. The grammar is field-name ":" OWS field-value OWS — leading/trailing OWS only applies to the value side.

**Header names are case-insensitive (Host == host == HOST).** You typically normalize to lowercase when storing in the map so lookups are consistent:

414 URI Too Long — request line (specifically the URI) exceeds the limit
431 Request Header Fields Too Large — a header line or total headers exceed
400 Bad Request — generic fallback

# CGI

The CGI produces a *CGI response*: a small set of headers, a blank line, then the body.


Content-Type: text/html        <- CGI headers (script writes these)
                               <- blank line (\r\n)
<html>...</html>               <- body

There's no status line, no Content-Length etc.

The authoritative spec is RFC 3875 — "The Common Gateway Interface (CGI) Version 1.1": https://datatracker.ietf.org/doc/html/rfc3875

That's the document to cite. The environment variables are in §4.1 and the meta-variable rules in §4. A few other useful references:

# TESTS

## stress test

siege -b http://127.0.0.1:8080/

## check the usage by a process

pgrep -a webserv          # PID + full command line, filtered by name
pidof webserv             # just the PID(s)

watch -n 1 'ps -o pid,rss,vsz,comm -C webserv'

Security
test the traversal path hack (remonter depuis le root du projet pour chopper des fichiers externes)

    nc localhost 8080

then

GET /../../../../../../etc/passwd HTTP/1.0
Host: localhost



# test netcat
POST /directory/youpla.bla HTTP/1.0
Host: localhost
Content-Length: 20

4444444444444455555555555555555555555555555555555555555555555555