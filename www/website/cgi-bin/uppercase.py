#!/usr/bin/env python3
"""Tiny CGI upload showcase.

POST the contents of a text file as the request body. The target file name is
passed in the query string, e.g.:

    POST /cgi-bin/uppercase.py?name=notes.txt

The script reads the body (bounded by CONTENT_LENGTH), UPPERCASES it, and writes
the result into the uploads/ directory next to the website root.
"""

import os
import sys
import urllib.parse


def reply(status, message):
    """Emit a CGI response (header block terminated by a blank line) and exit."""
    sys.stdout.write("Status: {}\r\n".format(status))
    sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n\r\n")
    sys.stdout.write(message)
    sys.exit(0)


# 1. Only POST makes sense here.
if os.environ.get("REQUEST_METHOD", "") != "POST":
    reply("405 Method Not Allowed",
          "Send a POST request with the file contents as the body.\n")

# 2. Read exactly CONTENT_LENGTH bytes from stdin (binary-safe).
length = int(os.environ.get("CONTENT_LENGTH", 0) or 0)
if length <= 0:
    reply("400 Bad Request", "Empty body: nothing to upload.\n")
data = sys.stdin.buffer.read(length)

# 3. Where to save: ?name=... from QUERY_STRING. Keep only the basename so a
#    client can't escape the uploads dir with "../" or an absolute path.
params = urllib.parse.parse_qs(os.environ.get("QUERY_STRING", ""))
name = os.path.basename(params.get("name", ["uppercased.txt"])[0]).strip()
if not name:
    name = "uppercased.txt"

# 4. Uppercase the text. The CGI runs from cgi-bin/, so uploads/ is one level up.
try:
    text = data.decode("utf-8")
except UnicodeDecodeError:
    reply("400 Bad Request", "Body is not valid UTF-8 text.\n")
upper = text.upper()

save_dir = os.path.abspath(os.path.join(os.getcwd(), "..", "uploads"))
os.makedirs(save_dir, exist_ok=True)
path = os.path.join(save_dir, name)
try:
    with open(path, "w", encoding="utf-8") as f:
        f.write(upper)
except OSError as e:
    reply("500 Internal Server Error", "Could not write file: {}\n".format(e))

# 5. Report what happened, with a short preview of the result.
preview = upper if len(upper) <= 500 else upper[:500] + "\n...(truncated)"
reply("201 Created",
      "Saved {} bytes (uppercased) to uploads/{}\n\n"
      "--- preview ---\n{}\n".format(len(upper.encode("utf-8")), name, preview))
