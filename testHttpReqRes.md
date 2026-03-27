curl -v http://localhost:8080/

### Malformed HTTP Request Line

echo -e "BADREQUEST\r\n\r\n" | nc -v localhost 8080

Expected: 400 Bad Request
-----------------------

### Missing Host Header (HTTP/1.1)

echo -e "GET / HTTP/1.1\r\n\r\n" | nc -v localhost 8080

Expected: 400 Bad Request
-------------------------

### Very Large Headers

perl -e 'print "GET / HTTP/1.1\\r\\nHost: localhost\\r\\nX-Big: " . \"A\"x10000 . \"\\r\\n\\r\\n\"' | nc -v localhost 8080 ** todo

Expected: 400 Bad Request or 431 Request Header Fields Too Large **** todo
--------------------------

### Invalid Content-Length

echo -e "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: notanumber\r\n\r\n" | nc -v localhost 8080

Expected: 400 Bad Request
--------------------------

### Content-Length Mismatch (short body)

echo -e "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10\r\n\r\nabc" | nc -v localhost 8080

Expected: 400 Bad Request or timeout
---------------------------

### Content-Length Mismatch (long body)

echo -e "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 3\r\n\r\nabcdef" | nc -v localhost 8080

Expected: 400
--------------------------

### Multiple Requests in One Connection (pipelining)

printf "GET / HTTP/1.1\r\nHost: localhost\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080

(printf "GET / HTTP/1.1\r\nHost: localhost\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\n\r\n"; sleep 1) | timeout 2 nc localhost 8080 | tr -d '\r' | sed -E 's/HTTP\/1\.1 ([0-9]{3})/\nHTTP\/1.1 \1/g' | grep -E '^(HTTP/1\.1 [0-9]{3}|Content-Length:|Content-Type:)'

Expected: Two responses, or first response and connection close if not supported
Explained:
(starts a subshell)
After printing requests, wait 1 second before the subshell exits.
Why: keeps the pipe open a little longer
gives server time to respond
prevents too-early close from client side
) Ends the subshell. So this whole left part produces bytes to standard output.
| nc localhost 8080

Pipe the printed requests into nc = netcat.

nc:

opens TCP connection to localhost port 8080
sends the request bytes to your server
prints server response to stdout

So now server output flows to the next command.
-----------------------------

### Invalid HTTP Version

echo -e "GET / HTTP/2.0\r\nHost: localhost\r\n\r\n" | nc -v localhost 8080

Expected: 400 Bad Request
----------------------------

### Request with Only CR(Carriage Return = \r (ASCII 13) → move to start of line) or Only LF(line feed, ASCII code: 10)
CRLF = \r\n → used in HTTP, Windows

echo -e "GET / HTTP/1.1\nHost: localhost\n\n" | nc -v localhost 8080

Expected: 400 Bad Request with echo because adds extra \n 
but

printf "GET / HTTP/1.1\nHost: localhost\n\n" | nc -v localhost 8080
returns 200 : nginx is linient: can handle only \n and \n\n not only \n\r



But NGINX has become stricter for chunked transfer encoding. In NGINX 1.29.4, they explicitly changed chunked parsing to require CRLF and reject bare LF, mainly for security and request-smuggling reasons.
------------------------

### Request with Binary Data in Headers

printf "GET / HTTP/1.1\r\nHost: localhost\r\nX-Bin: \x01\x02\x03\r\n\r\n" | nc -v localhost 8080

Expected: 400 Bad Request
explained:
X-Bin: \x01\x02\x03\r\n
custom header whose value contains raw bytes 01 02 03
| nc -v localhost 8080
sends that raw request to your server
-------------------------


