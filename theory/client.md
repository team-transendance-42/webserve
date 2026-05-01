A client is a browser (or curl, or anything) that connects to your server. The moment it connects, your OS gives you a new file descriptor — that's the "client fd". Everything you know about that client lives in that fd + your buffers.

Browser                          Your Server
───────                          ───────────
                                 _listen_fd = socket on port 8080
                                 poll() watching _listen_fd...

[connects] ──────────────────►  accept() fires
                                 → OS gives you clientFd = 5
                                 → you add fd 5 to _fds[]

[sends HTTP request] ─────────►  poll() says fd 5 is readable
                                 → recv(5, buf) → raw bytes land in _requests[5]
                                 → parse when you see \r\n\r\n

                                 → build response (read file, etc)

[receives HTTP response] ◄──────  send(5, response)

[closes or sends another req]    → if Connection: close → close(fd 5)
                                 → if Connection: keep-alive → stay in poll()

-----------------------------------------------------------------------

init()
  │
  ├─ socket()        → creates _listen_fd
  ├─ setsockopt()    → SO_REUSEADDR (so restart doesn't say "address in use")
  ├─ bind()          → attaches fd to host:port from config
  ├─ listen()        → OS starts queuing incoming connections (BACKLOG=128)
  ├─ fcntl()         → sets _listen_fd non-blocking
  └─ push _listen_fd into _fds[0]

run()
  │
  └─ while (_running):
        │
        poll(_fds)   → sleeps until something happens (max POLL_TIMEOUT ms)
        │
        ├─ if _fds[0] has POLLIN  → new connection → _acceptClient()
        │
        └─ for each _fds[i] (i>0):
              │
              ├─ POLLIN  → data arrived  → _handleClient(fd, i)
              └─ POLLHUP → disconnected  → _removeClient(i)

-----------------------------------------------------------------

