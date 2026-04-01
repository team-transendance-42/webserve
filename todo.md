https://github.com/team-transendance-42/webserve
https://euria.infomaniak.com/

Petya:
* HTTP server ✓
* socket setup ✓
* req parsing ✓
* method handling(get, post, delete) ✓
* response formating ✓
* routing (file handling logic) ✓
-> implement strict http 1.1
-> chunked
-> implement multiple servers
-> test 500 too

Noah:
* config parsing
* error handling
* cgi/dynamic content
* logging /utilities
* install Linter: for coding style

-------------------------------------
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
------------------------------------

echo -e "GET / HTTP/1.1\nBad-Header\n\n" | nc localhost 8080

nc localhost 8080
# (do not type anything, just wait)

curl -v http://localhost:8080/cgi-bin/doesnotexist

curl -v -X POST http://localhost:8080/delete_create_file --data 'test'
# Remove required headers or send invalid ones to test 400 Bad Request

-------------------------------------


(echo -n "GET / HTTP/1.1\r\nHost: localhost\r\n"; sleep 10) | nc localhost 8080
//Send only part of a request, then wait. The server should close the connection after 6 seconds of idle time and not wait indefinitely for the rest of the request.

--------------------------------------
curl -v http://localhost:8080/trigger500
//Test 500 by simulating a server-side bug (if you have a debug endpoint):

---------------------------------------
!!NB!! how nc works—it does not exit until you close it or send EOF.
nc localhost 8080 opens a TCP connection and waits for data or closure.
If you type, it sends data to the server.
If you do nothing, it just waits.

------------------------
nc = netcat(utility for eading/writing data across network connections using TCP/UDP)
For automatic exit, try:

nc -v -q 1 localhost 8080
The -q 1 option tells nc to quit 1 second after EOF on stdin or after the connection closes. This works on most modern nc versions (like OpenBSD netcat). If your nc does not support -q, this behavior is just a limitation of your nc version.

For HTTP testing and automatic exit, use curl instead:
curl -v http://localhost:8080

---------------------
todo:
1. A field for return (redirect) at the server level, if you want server-wide redirects.
A field for listen (to support multiple ports per server, if needed).
If you want to support multiple servers, you’ll need a container (e.g., std::vector<ServerConfig>) in your main config manager.
----------------------

2. Hardcoded defaults in struct fields (e.g., port = 0) are fine for simple projects and make the code less error-prone.
For production or config-file-driven servers, it’s better to initialize all values explicitly when parsing the config file.
If you plan to parse config files, consider removing most hardcoded values and initializing them in your config parser.
------------------------
 
3. Make sure you always send the Allow header with 405 Method Not Allowed (RFC and nginx do this).
For 400 Bad Request, ensure you catch all malformed request cases (missing Host for HTTP/1.1, bad headers, etc.).
For 413, 414, 408, 501, 502, 504, etc., add static helpers and error pages if you want full nginx parity.
Consider supporting per-location error_page overrides if you want to match nginx exactly.
Always set the correct Content-Type and Content-Length headers in error responses.
----------------------

In HttpRequest (or immediately after parsing), add:
Check for required Host header in HTTP/1.1 (you just fixed this).
Reject duplicate headers where not allowed (e.g., multiple Content-Length).
Validate header field names and values for allowed characters (RFC compliance).
Enforce maximum number of headers (to prevent header DoS).
Check for invalid/malformed Content-Length (non-numeric, negative).
Validate HTTP version (only allow 1.0 and 1.1).
Reject requests with only CR or only LF as line endings.
Optionally, check for forbidden headers (e.g., Transfer-Encoding if not supported).

FindNFund
Leaning Technologies
Beyond Sports
QPS
Floyd & Hamilton
Van Lanschot Kempen