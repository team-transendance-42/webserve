#Idle TCP Connection (no data sent)
nc -v localhost 8080
#Expected: 408 timeout
#After CLIENT_TIMEOUT seconds, the server closes the connection and (optionally) sends a 408 response.
#Explained: netcat: open TCP socket(lets manually to send data), pipe stdin/stdout through it

nc opens a TCP connection, server accepts it, then waits for data
there is sent no HTTP request yet, server usually keeps socket open, Server has read timeout, so if client doesn't send anything within that time, server closes the connection and optionally sends a 408 Request Timeout response. This prevents idle connections from consuming resources indefinitely.
------------------------
!!NB!! EPOLL = Linux epoll event system
RD = read
HUP = hang up

EPOLLIN = data came in
EPOLLOUT = socket ready to send out
EPOLLRDHUP = remote side hung up

400 Bad Request
request is broken/incomplete and client closed
408 Request Timeout
client kept connection open, but sent too slowly / stopped sending
--------------------------

#Partial HTTP Request (incomplete headers): still get 408 server timeout: The data received so far is not malformed — it could still become valid
The server has no way to know if more bytes are coming or not
So it waits... and eventually times out → 408 Request Timeout
(echo -n "GET / HTTP/1.1\r\nHost: localhost\r\n"; sleep 10) | nc -v localhost 8080 // echo often sends literal characters \r and \n, not real CRLF. use better printf: 

printf "GET / HTTP/1.1\r\nHost: localhost\r\n"; sleep 10 | nc -v localhost 8080 
#Expected: 408 timeout

printf "GET / HTTP/1.1\r\nHost: localhost\r\n" | nc -v localhost 8080
------------------------

#Slow Client (delayed body)
(echo -e "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n\r\n"; sleep 10; echo "data") | nc -v localhost 8080

(printf "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n\r\n"; sleep 10; echo "data") | nc -v localhost 8080
#Expected: 408 timeout
#Server closes the connection before the body is fully sent if timeout is reached.
#Explained: headers done(Content-Length: 100\r\n\r\n), now parser waits for 100 bytes of body
-----------------------------

#Multiple Idle Connections
for i in {1..5}; do nc -v localhost 8080 & done
#Expected:
#Server closes all idle connections after timeout, not just the first one.
#Explained:  & = run in background (don't wait), so we have 5 concurrent idle connections. Server should check all clients for timeout and close them all after timeout.

=========================
         THEORY
=========================
A TCP connection consumes kernel resources: a file descriptor, socket buffers (~4-8KB each), and an epoll slot. Without a timeout, a client that connects and sends nothing holds those resources forever. Your server has a fixed fd limit (typically 1024 by default, up to ~65535). An attacker or buggy client can exhaust them all, making your server unable to accept new connections — this is a Slowloris attack.

### exploits
Slowloris: open thousands of connections, send partial headers every ~15s
→ never complete the request, never trigger your parser, hold fd forever

Slow POST: send Content-Length: 10000000, then drip 1 byte every 9 seconds
→ your parser keeps returning INCOMPLETE, connection stays open

Keep-Alive exhaustion: complete a request, then hold the keep-alive connection idle
→ you responded correctly but they never send the next request

Half-open connections: client opens TCP (SYN), never finishes handshake
→ handled by kernel backlog, not your timeout, but worth knowing
----------------------
### Problems it causes without timeout

fd leak       → accept() starts returning EMFILE, new clients get connection refused
memory leak   → each Client* on heap, writeBuf accumulating partial responses  
epoll bloat   → epoll_wait scans more fds each iteration, latency grows
zombie state  → client process died, kernel sent FIN, but you never called closeClient()
              → EPOLLRDHUP should catch this, but only if client closed cleanly
------------------------

