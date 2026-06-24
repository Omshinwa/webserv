#!/usr/bin/env python3
"""Merged upload-transform CGI (uppercase.py + translate.py).

POST the file contents as the request body. The save name and the operation(s)
to run are passed in the query string, e.g.:

    POST /cgi-bin/transform.py?name=notes.txt&uppercase=1
    POST /cgi-bin/transform.py?name=notes.txt&translate=1
    POST /cgi-bin/transform.py?name=notes.txt&uppercase=1&translate=1

The body is transformed and saved into the uploads/ directory next to the
website root. When both operations are requested the text is translated first,
then uppercased. With no operation it is saved unchanged.
"""

import os
import sys
import json
import urllib.parse
import urllib.request


def reply(status, message):
    """Emit a CGI response (header block terminated by a blank line) and exit."""
    sys.stdout.write(f"Status: {status}\r\n")
    sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n\r\n")
    sys.stdout.write(message)
    sys.exit(0)


def translate(text, src="auto", dst="en"):
    """Translate via Google's free (unofficial) web endpoint. No API key needed."""
    params = urllib.parse.urlencode({
        "client": "gtx",
        "sl": src,       # source language ("auto" = detect)
        "tl": dst,       # target language
        "dt": "t",       # return translated text
        "q": text,
    })
    url = "https://translate.googleapis.com/translate_a/single?" + params
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=5) as resp:
        payload = json.load(resp)
    # payload[0] is a list of [translated_segment, original_segment, ...] chunks.
    return "".join(chunk[0] for chunk in payload[0] if chunk[0])


# 1. Only POST makes sense here.
if os.environ.get("REQUEST_METHOD", "") != "POST":
    reply("405 Method Not Allowed",
          "Send a POST request with the file contents as the body.\n")

# 2. Read exactly CONTENT_LENGTH bytes from stdin (binary-safe).
length = int(os.environ.get("CONTENT_LENGTH", 0) or 0)
if length <= 0:
    reply("400 Bad Request", "Empty body: nothing to process.\n")
data = sys.stdin.buffer.read(length)

# 3. Query string: save name + which operation(s) to run. Keep only the basename
#    so a crafted name can't escape the uploads dir with "../" or an absolute path.
params = urllib.parse.parse_qs(os.environ.get("QUERY_STRING", ""))
name = os.path.basename(params.get("name", ["upload.txt"])[0]).strip() or "upload.txt"
do_translate = "translate" in params
do_upper = "uppercase" in params

# 4. Decode + transform (translate first, then uppercase if both requested).
try:
    text = data.decode("utf-8")
except UnicodeDecodeError:
    reply("400 Bad Request", "Body is not valid UTF-8 text.\n")

applied = []
try:
    if do_translate:
        text = translate(text, dst="en")
        applied.append("translated -> en")
    if do_upper:
        text = text.upper()
        applied.append("uppercased")
except Exception as e:
    reply("502 Bad Gateway", f"Processing failed: {e}\n")
if not applied:
    applied.append("saved unchanged")

# 5. Save into uploads/ (one level up from cgi-bin/).
save_dir = os.path.abspath(os.path.join(os.getcwd(), "..", "uploads"))
os.makedirs(save_dir, exist_ok=True)
path = os.path.join(save_dir, name)
try:
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)
except OSError as e:
    reply("500 Internal Server Error", f"Could not write file: {e}\n")

# 6. Report what happened, with a short preview of the result.
preview = text if len(text) <= 500 else text[:500] + "\n...(truncated)"
reply("201 Created",
      f"Saved {len(text.encode('utf-8'))} bytes ({', '.join(applied)}) to uploads/{name}\n\n"
      f"--- preview ---\n{preview}\n")
