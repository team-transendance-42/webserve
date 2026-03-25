https://github.com/team-transendance-42/webserve

Petya:
* HTTP server ✓
* socket setup ✓
* req parsing ✓
* method handling(get, post, delete)
* response formating ✓
* routing (file handling logic ??)
* do we use pramga use once or ifndef def
-> server/client timeout handling !!! nb !!!

Noah:
* config parsing
* error handling
* cgi/dynamic content
* file handling logic ??
* logging /utilities
* install Linter: for coding style

kill working webserv:

ss -ltnp | grep ':8080'
kill 12345

or better:
kill "$(ss -ltnp | awk '/:8080/ {gsub(/.*pid=|,.*/,"",$NF); print $NF; exit}')"

delete:
204 No Content (or 200 with body) on success
404 if file missing
403 if permission denied / path escape attempt
405 if method not allowed by location
409 if target is a directory