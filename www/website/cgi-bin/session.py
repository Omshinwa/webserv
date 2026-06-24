#!/usr/bin/env python3
"""Cookie / session demo.

The cookie holds only an opaque session id; the real data lives server-side in
a file under cgi-bin/sessions/<id>. This is the classic "cookie = id, state =
server" pattern.

  POST (body = name)          -> "log in": create a session, Set-Cookie the id.
  GET                         -> returning visitor? read the session, bump the
                                 visit count, greet them; else say "logged out".
  GET ?action=logout          -> delete the session + expire the cookie.
"""

import os
import sys
import urllib.parse
import uuid

# We are chdir'd into cgi-bin/, so sessions live next to this script.
SESS_DIR = os.path.join(os.getcwd(), "sessions")


def respond(headers, body):
    """
    write the response and exit the program
    """
    for h in headers:
        sys.stdout.write(h + "\r\n")
    sys.stdout.write("\r\n")
    sys.stdout.write(body)
    sys.exit(0)


def get_cookie(name):
    # HTTP_COOKIE looks like "sid=abc123; theme=dark"
    for part in os.environ.get("HTTP_COOKIE", "").split(";"):
        key, _, value = part.strip().partition("=")
        if key == name:
            return value
    return None


def session_path(sid):
    # Only allow hex ids (uuid4().hex) so a crafted cookie can't escape the dir.
    if not sid or not all(c in "0123456789abcdef" for c in sid):
        return None
    return os.path.join(SESS_DIR, sid)


def read_session(sid):
    path = session_path(sid)
    if not path or not os.path.isfile(path):
        return None
    lines = open(path).read().splitlines()
    return {"name": lines[0], "count": int(lines[1])} if len(lines) >= 2 else None


def write_session(sid, name, count):
    os.makedirs(SESS_DIR, exist_ok=True)
    with open(os.path.join(SESS_DIR, sid), "w") as f:
        f.write(f"{name}\n{count}")


method = os.environ.get("REQUEST_METHOD", "GET")
query = os.environ.get("QUERY_STRING", "")
sid = get_cookie("sid")

# --- logout: drop the server session and tell the browser to delete the cookie
if "action=logout" in query:
    path = session_path(sid)
    if path and os.path.isfile(path):
        os.remove(path)
    respond(["Status: 200 OK",
             "Content-Type: text/plain; charset=utf-8",
             "Set-Cookie: sid=; Path=/; Max-Age=0",
             "Set-Cookie: user=; Path=/; Max-Age=0"],
            "Logged out. The session cookie was expired.\n")

# --- login: a POST with a name creates a brand-new session
if method == "POST":
    length = int(os.environ.get("CONTENT_LENGTH", 0) or 0)
    name = (sys.stdin.read(length).strip() if length > 0 else "") or "anonymous"
    sid = uuid.uuid4().hex
    # Start at 0:
    write_session(sid, name, 0)
    respond(["Status: 200 OK",
             "Content-Type: text/plain; charset=utf-8",
             # HttpOnly id for auth (JS can't read it) + a readable name for the UI.
             f"Set-Cookie: sid={sid}; Path=/; HttpOnly",
             f"Set-Cookie: user={urllib.parse.quote(name)}; Path=/"],
            f"Logged in as '{name}'. A session cookie was set on your browser.\n")

# --- GET: do we recognise this visitor's cookie?
sess = read_session(sid)
if sess:
    sess["count"] += 1
    write_session(sid, sess["name"], sess["count"])
    respond(["Status: 200 OK", "Content-Type: text/plain; charset=utf-8"],
            f"Hello {sess['name']}! You have visited this page {sess['count']} time(s).\n")

respond(["Status: 200 OK", "Content-Type: text/plain; charset=utf-8"],
        "No active session. POST your name to /cgi-bin/session.py to log in.\n")
