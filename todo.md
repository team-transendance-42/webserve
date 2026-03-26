https://github.com/team-transendance-42/webserve
https://euria.infomaniak.com/

Petya:
* HTTP server ✓
* socket setup ✓
* req parsing ✓
* method handling(get, post, delete)
* response formating ✓
* routing (file handling logic ??)
* do we use pramga use once or ifndef def
-> server/client timeout handling !!! nb !!!
-> delete upload
-> handle mulformed requests (400)
-> test 500 too

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
--------------------------------------
or better:
kill "$(ss -ltnp | awk '/:8080/ {gsub(/.*pid=|,.*/,"",$NF); print $NF; exit}')"

echo -e "GET / HTTP/1.1\nBad-Header\n\n" | nc localhost 8080 to test 400 Bad Request

 curl -v -X DELETE http://localhost:8080/delete_create_file/bla
 returns 204 No Content on success
 returns 404 if file missing
 returns 403 if permission denied / path escape attempt
 returns 405 if method not allowed by location
 returns 409 if target is a directory // not handled yet, but we can check if it's a directory and return 409 instead of 403 for path escape attempts
 ------------------------------------------

 curl -v -X POST http://localhost:8080/delete_create_file \
  -H "X-Filename: bla" \
  -H "Content-Type: text/plain" \
  --data 'bla bla'
// no need for -X POST: auto doen with --data, but we can keep it for clarity
  curl -v http://localhost:8080/delete_create_file \
  -H "X-Filename: bla" \
  -H "Content-Type: text/plain" \
  --data 'bla bla'
-----------------------------------

valgrind --leak-check=full --track-fds=yes ./webserv