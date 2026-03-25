Petya:
* HTTP server ✓
* socket setup ✓
* req parsing ✓
* method handling(get, post, delete)
* response formating ✓
* routing (file handling logic ??)
* do we use pramga use once or ifndef def

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