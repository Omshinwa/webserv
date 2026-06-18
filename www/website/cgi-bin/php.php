#!/usr/bin/env php-cgi
<?php
// read the request body (for POST), bounded by CONTENT_LENGTH
$length = (int) ($_SERVER["CONTENT_LENGTH"] ?? 0);
$body = $length > 0 ? fread(STDIN, $length) : "";

// CGI header block, terminated by a blank line
header("Content-type: text/html");

// body
echo "<h1>PHP CGI</h1>";
echo "<h2>Environment</h2>";
echo "<pre>";
$env = getenv();
ksort($env);
foreach ($env as $key => $value) {
    echo htmlspecialchars("$key=$value") . "\n";
}
echo "</pre>";

if ($body !== "") {
    echo "<h2>Request body</h2>";
    echo "<pre>" . htmlspecialchars($body) . "</pre>";
}
