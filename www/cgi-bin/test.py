#!/usr/bin/env python3

import os
COERCECLOCALE=0
print("Content-Type: text/html\r\n")   # headers + blank line
print("<h1>Hello from CGI</h1>")        # body

# dump every environment variable the server passed us
print("<pre>")
for key, value in os.environ.items():
    print(f"{key}={value}")
print("</pre>")
