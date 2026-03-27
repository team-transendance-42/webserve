#Idle TCP Connection (no data sent)
nc -v localhost 8080
#Expected:
#After SERVER_TIMEOUT seconds, the server closes the connection and (optionally) sends a 408 response.


#Partial HTTP Request (incomplete headers)
(echo -n "GET / HTTP/1.1\r\nHost: localhost\r\n"; sleep 10) | nc -v localhost 8080
#Expected:
#Server closes the connection after timeout, even though the request is incomplete.

#Slow Client (delayed body)
(echo -e "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n\r\n"; sleep 10; echo "data") | nc -v localhost 8080
#Expected:
#Server closes the connection before the body is fully sent if timeout is reached.

#Keep-Alive Connection (no further requests)
(echo -e "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n"; sleep 10) | nc -v localhost 8080
#Expected:
#Server responds, keeps connection open, then closes it after timeout if no new request is sent.

#Multiple Idle Connections
for i in {1..5}; do nc -v localhost 8080 & done
#Expected:
#Server closes all idle connections after timeout, not just the first one.