poll()                          epoll()
──────                          ───────
passes ALL fds to kernel        registers fds ONCE with kernel
on every call                   kernel tracks them internally

O(n) scan every wakeup          O(1) — only returns READY fds

1000 clients = kernel scans     1000 clients = kernel returns
1000 fds every poll() call      only the 3 that have data

breaks at ~1000 fds             handles 100,000+ fds fine

man epoll
---------------------------------------------------------
Imagine you're a security guard watching a building with 1000 doors.
Checking every door every second = exhausting and slow.
Instead, you install alarm sensors on doors. Now you just wait — and only act when an alarm rings.
epoll is that alarm system. Your 3 functions = managing those sensors.

_epollAdd — Install a sensor on a door

New client connects → _epollAdd(clientFd, EPOLLIN) — watch for their request
New listening socket → _epollAdd(listenFd, EPOLLIN) — watch for new connections

_epollMod — Change what the sensor watches for

```

A connection has **two phases**:
```
Phase 1: Client → Server    (you need to READ)
         watch EPOLLIN

         client sends "GET / HTTP/1.1..."
         you read it, build the response

Phase 2: Server → Client    (you need to WRITE)  
         watch EPOLLOUT

         you send "HTTP/1.1 200 OK..."
         then switch BACK to EPOLLIN for next request

// Client just connected — watch for their request
_epollAdd(clientFd, EPOLLIN);

// You finished reading, response is ready — switch to write mode
_epollMod(clientFd, EPOLLOUT);

// You finished sending — switch back to read mode (keep-alive)
_epollMod(clientFd, EPOLLIN);

// ❌ Bad — EPOLLOUT fires constantly when buffer has space
//    your loop spins doing nothing, wasting 100% CPU
ev.events = EPOLLIN | EPOLLOUT;  // don't do this

// ✅ Good — only watch EPOLLOUT when you actually have data to send
_epollMod(fd, EPOLLOUT);         // then switch back when done

_epollDel — Remove the sensor
/ Client disconnected (read returned 0)
_epollDel(clientFd);
close(clientFd);       // ← always close AFTER del
----------------------------


client connects
      │
      ▼
_epollAdd(clientFd, EPOLLIN)        ← "watch for their request"
      │
      ▼
epoll_wait fires EPOLLIN
      │
      ▼
read() drain loop until EAGAIN      ← non-blocking reads
      │
      ▼
parse HTTP request, build response
      │
      ▼
_epollMod(clientFd, EPOLLOUT)       ← "tell me when I can write"
      │
      ▼
epoll_wait fires EPOLLOUT
      │
      ▼
write() response
      │
      ▼
_epollMod(clientFd, EPOLLIN)        ← keep-alive: back to reading
      │        OR
      ▼
_epollDel(clientFd)                 ← connection: close
close(clientFd)
































