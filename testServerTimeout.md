#Idle TCP Connection (no data sent)
nc -v localhost 8080
#Expected: 408 timeout
#After SERVER_TIMEOUT seconds, the server closes the connection and (optionally) sends a 408 response.
#Explained: netcat: open TCP socket, pipe stdin/stdout through it
------------------------

#Partial HTTP Request (incomplete headers)
(echo -n "GET / HTTP/1.1\r\nHost: localhost\r\n"; sleep 10) | nc -v localhost 8080 
#Expected: 408 timeout
#Server closes the connection after timeout, even though the request is incomplete.
#Explained: GET / HTTP/1.1\r\n
Host: localhost\r\n
\r\n              ← this blank line is missing — parser never sees COMPLETE
------------------------

#Slow Client (delayed body)
(echo -e "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n\r\n"; sleep 10; echo "data") | nc -v localhost 8080
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

