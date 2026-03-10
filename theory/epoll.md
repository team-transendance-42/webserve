poll()                          epoll()
──────                          ───────
passes ALL fds to kernel        registers fds ONCE with kernel
on every call                   kernel tracks them internally

O(n) scan every wakeup          O(1) — only returns READY fds

1000 clients = kernel scans     1000 clients = kernel returns
1000 fds every poll() call      only the 3 that have data

breaks at ~1000 fds             handles 100,000+ fds fine

// 1. create the epoll instance — returns a fd
int epfd = epoll_create1(0);

// 2. register/modify/remove fds from the watch list
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);   // add
epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event);   // modify
epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);     // remove

// 3. wait for events — like poll() but only returns ready fds
int n = epoll_wait(epfd, events, MAX_EVENTS, timeout_ms);
--------------------------------------------------

struct epoll_event {
    uint32_t events;   // what to watch for
    epoll_data_t data; // YOUR data — attach anything
};

// data is a union:
union epoll_data_t {
    int       fd;      // most common — store the fd
    uint32_t  u32;
    uint64_t  u64;
    void     *ptr;     // can store a pointer to your Client struct
};
```

### Event flags you need
```
EPOLLIN        → fd has data to read (or new connection on listen fd)
EPOLLOUT       → fd is ready to write (send buffer has space)
EPOLLERR       → error on fd
EPOLLHUP       → hangup — client disconnected
EPOLLET        → Edge Triggered mode (advanced — explained below)
EPOLLRDHUP     → peer shut down writing (clean disconnect detection)
```

---

## Level Triggered vs Edge Triggered

This is the most important epoll concept:
```
LEVEL TRIGGERED (default):
  epoll_wait keeps returning the fd
  as long as data is still available
  → safe, easy, slight overhead
  → read in a loop until you're done, or it fires again next call

EDGE TRIGGERED (EPOLLET):
  epoll_wait fires ONCE when new data arrives
  does NOT fire again until NEW data comes in
  → if you don't read ALL data in one shot → fd goes silent forever
  → MUST read in a loop until EAGAIN
  → harder but more efficient