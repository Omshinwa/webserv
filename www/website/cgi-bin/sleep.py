#!/usr/bin/env python3
from time import sleep
sleep(20)

# The cgi and cgitb modules were deprecated in Python 3.11 (PEP 594)
import os
import sys

# read the request body (for POST), bounded by CONTENT_LENGTH
length = int(os.environ.get("CONTENT_LENGTH", 0) or 0)
body = sys.stdin.read(length) if length > 0 else ""

# CGI header block, terminated by a blank line
print("Content-type: text/html")
print()

# body
print("<h1>Sleepy.py Python CGI</h1>")
print("<h2>Environment</h2>")
print("<pre>")
for key in sorted(os.environ):
    print(f"{key}={os.environ[key]}")
print("</pre>")

if body:
    print("<h2>Request body</h2>")
    print(f"<pre>{body}</pre>")
