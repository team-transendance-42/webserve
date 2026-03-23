1. recv()  ← read bytes from socket into buffer
2. parse   ← HttpRequest::feed() assembles the HTTP request  
3. process ← _processRequest() builds HttpResponse
4. send()  ← write response bytes back to same socket fd
----------------------

Step 1 — Client connects → _acceptClient()
Each client gets its own fd. _listenFd only accepts — never reads/writes.

Step 2 — Data arrives → _readClient()
// epoll fires EPOLLIN on clientFd
// recv() pulls raw bytes off the socket
ssize_t bytes = recv(client.fd, buf, sizeof(buf) - 1, 0);

Your parser accumulates in _buf and only returns COMPLETE when it has the full request (headers + body if Content-Length says so).

Step 3 — Request complete → _processRequest()

_processRequest() reads client.request and writes into client.writeBuf: