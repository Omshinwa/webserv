#!/usr/bin/env python3

import os
import sys
import json
import urllib.parse
import urllib.request


def translate(text, src="fr", dst="en"):
    """Translate via Google's free (unofficial) web endpoint. No API key needed."""
    params = urllib.parse.urlencode({
        "client": "gtx",
        "sl": src,       # source language
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


def fail(status, message):
    """Report a client-side problem with a real HTTP status (see note above)."""
    sys.stdout.write("Status: {}\r\n".format(status))
    sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n\r\n")
    sys.stdout.write(message + "\n")
    sys.exit(0)


if os.environ.get("REQUEST_METHOD", "") != "POST":
    fail("405 Method Not Allowed",
         "capitalize.py expects a POST request with the file as the body.")


# Read exactly CONTENT_LENGTH bytes from stdin (binary-safe).
length = int(os.environ.get("CONTENT_LENGTH", 0) or 0)
if length <= 0:
    fail("400 Bad Request", "Empty body: nothing to process.")
data = sys.stdin.buffer.read(length)

# The body is JSON: {"name": "...", "message": "..."}. Parse it, translate the
# message fr -> en, then format the line and encode for output.
text = data.decode("utf-8", errors="replace")
try:
    payload = json.loads(text)                                    # loads, not load
    translated = translate(payload["message"], src="fr", dst="en")
    result = f"{payload['name']} says `{translated}`".encode("utf-8")
except Exception as e:
    fail("502 Bad Gateway", "Processing failed: {}".format(e))

# CGI header block (CRLF, terminated by a blank line), then the translated body.
sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n")
sys.stdout.write("\r\n")
sys.stdout.flush()
sys.stdout.buffer.write(result)
