https://github.com/team-transendance-42/webserve

## NB!!!
Many sockets does not mean many tabs exactly.
It means many active TCP connections.
One tab can open multiple connections.
Two URLs can reuse same connection (keep-alive).
So fd count tracks connections, not pages.
----------------------------------------

## NB!!

----------------------------------------

binding ports below 1024 requires root privileges on Linux — you'd need sudo

can use these ports: 8080, 8081, 8082, 8443 — common convention for HTTP dev servers.

## Virtual hosting works on the same port

Two servers sharing port `8080` is fine — the server picks which one to use via the `Host:` header the browser sends:
```
Request arrives on port 8080
        │
        ▼
Host: one        →  serve SERVER 1
Host: two        →  serve SERVER 2
Host: unknown    →  serve first server block (default)
```

To test locally, add to `/etc/hosts`:
```
127.0.0.1   one
127.0.0.1   two
127.0.0.1   three
127.0.0.1   four
------------------------------------------------

Our webserv uses a single-threaded, event-driven reactor approach with non-blocking sockets + epoll:

Open one listening TCP socket in init
Create epoll instance
Create socket
setsockopt(SO_REUSEADDR)
bind(host, port)
listen(backlog)
set listening fd to non-blocking
register listen fd in epoll

Main loop is epoll-driven
Main repeatedly calls tick
tick calls epoll_wait once per iteration
Handles only ready fds, not all fds

Accept all pending clients in a loop
On listen-fd readable event, accept in while loop until EAGAIN/EWOULDBLOCK
Each new client socket is set non-blocking
Each client fd is added to epoll with read/hangup interest
Client state stored in map fd -> Client

Per-client read/write state switching
EPOLLIN: read + parse request
When response ready, modify interest to EPOLLOUT
EPOLLOUT: send response
If keep-alive, switch back to EPOLLIN; else close

Cleanup path
On RDHUP/HUP/ERR or recv 0/error: epoll delete + close fd + erase client map entry
----------------------------------

In short: not thread-per-connection and not blocking I/O. It is one process, one event loop, many sockets multiplexed by epoll. one thread handles all ready sockets
--------------------------------

Multiplexing means combining management of many I/O channels through one event loop.

Without multiplexing:

You might use one thread per client, or
A blocking loop that gets stuck on one slow client.

With epoll multiplexing:

You register all relevant fds once.
Kernel tracks readiness for you.
epoll_wait returns only ready fds.
Single loop handles thousands of connections efficiently.
----------------------------------
NB!!!
Many sockets does not mean many tabs exactly.
It means many active TCP connections.
One tab can open multiple connections.
Two URLs can reuse same connection (keep-alive).
So fd count tracks connections, not pages.
----------------------------------------

Complexity intuition

Thread-per-connection scales poorly due to thread cost.
Event loop + epoll scales much better for many idle/active mixed clients.
This is why nginx-like servers use this model.
------------------------------------------

In one sentence:
epoll multiplexing is a way for one event loop to efficiently handle many client connections by reacting only to sockets that are ready for I/O right now.
------------------------------------------

