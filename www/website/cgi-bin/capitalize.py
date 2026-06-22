#!/usr/bin/env python3

# Usage:  POST /cgi-bin/capitalize.py?name=foo.txt
#         body = the raw text file (same way upload.html POSTs files)

import os
import sys


def fail(status, message):
    """Report a client-side problem with a real HTTP status (see note above)."""
    sys.stdout.write("Status: {}\r\n".format(status))
    sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n\r\n")
    sys.stdout.write(message + "\n")
    sys.exit(0)


# The server chdir()s into this script's directory before exec, so by default we
# save one level up: the server root under configs/default.conf (root www/website),
# which makes the result downloadable at http://localhost:8080/<name>.
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
OUTPUT_DIR = os.path.dirname(SCRIPT_DIR)

if os.environ.get("REQUEST_METHOD", "") != "POST":
    fail("405 Method Not Allowed",
         "capitalize.py expects a POST request with the file as the body.")

# QUERY_STRING is "name=foo.txt&..."; pull out name, default to capitalized.txt.
query = os.environ.get("QUERY_STRING", "")
name = ""
for pair in query.split("&"):
    if pair.startswith("name="):
        name = pair[len("name="):]
name = os.path.basename(name).strip()  # basename() kills any ../ traversal
if not name:
    name = "capitalized.txt"

# Read exactly CONTENT_LENGTH bytes from stdin (binary-safe).
length = int(os.environ.get("CONTENT_LENGTH", 0) or 0)
if length <= 0:
    fail("400 Bad Request", "Empty body: nothing to capitalize.")
data = sys.stdin.buffer.read(length)

# Decode as text so .upper() applies, replacing anything that isn't valid UTF-8.
text = data.decode("utf-8", errors="replace")
result = text.upper().encode("utf-8")

out_path = os.path.join(OUTPUT_DIR, name)
try:
    with open(out_path, "wb") as f:
        f.write(result)
except OSError as e:
    fail("500 Internal Server Error", "Could not save '{}': {}".format(name, e))

# CGI header block (CRLF, terminated by a blank line), then the capitalized body.
sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n")
sys.stdout.write("X-Saved-As: /{}\r\n".format(name))
sys.stdout.write("\r\n")
sys.stdout.flush()
sys.stdout.buffer.write(result)
