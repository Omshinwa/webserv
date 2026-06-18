#!/bin/bash

cat << EOF
Content-type: text/html

<h1>Bash CGI</h1>
Files in the folder:
<hr>
EOF
ls

echo "<br>"
echo "<br>"
echo "Env vars:"

echo "<p>"
env
echo "</p>"