
epoll is an event notifier for many sockets.
You register file descriptors once, then ask the kernel: “which ones are ready now?”
This avoids scanning every fd each loop, so it scales much better than poll/select for many clients.
Your server uses non-blocking sockets plus level-triggered epoll (default), so you must read/write until EAGAIN.
----------------------------------------------------------------

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

Our epoll Flow (Exact Code Path)

Create epoll instance and listening socket in init:
Server.cpp:27, Server.cpp:64, Server.cpp:432

Main loop calls one tick repeatedly:
main.cpp:48, main.cpp:49, Server.cpp:82

Wait for ready events:
Server.cpp:88

If event belongs to listen fd, accept all pending clients:
Server.cpp:101, Server.cpp:152, Server.cpp:165

If event belongs to a client, run state machine:
Server.cpp:174, Server.cpp:203, Server.cpp:212, Server.cpp:228

On disconnect/error, remove from epoll and close:
Server.cpp:236, Server.cpp:237, Server.cpp:144
--------------------------------------------------------------

Minimal Mental Model

Listening socket only accepts.
Client socket alternates:
read request -> build response -> write response -> back to read (keep-alive) or close.
epoll is just the scheduler that tells you which fd is ready for the next step.
----------------------------------------------------------------

tests:
Start server and open one browser tab; follow logs for one full read/write cycle.
Open multiple tabs quickly; confirm only ready sockets are processed each tick.
Test keep-alive with repeated requests; verify same fd goes read -> write -> read.
Force disconnect mid-request; verify close path removes fd cleanly.





































