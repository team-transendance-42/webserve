```
ConfigParser   → parses default.conf  → ServerConfig[]
                                              │
                                              ▼
                                           Server
                                         (epoll loop)
                                              │
                              ┌───────────────┼───────────────┐
                              ▼               ▼               ▼
                           Client          Client          Client
                         (fd + bufs)     (fd + bufs)     (fd + bufs)
                              │
                    ┌─────────┴─────────┐
                    ▼                   ▼
               HttpRequest         HttpResponse
               (parse bytes)       (build reply)
                                        │
                              (Router goes here later)
                              reads files, checks methods,
                              matches location blocks
```
HttpRequest and HttpResponse are pure data classes — no sockets, no fds, no epoll. They only deal with HTTP text. That separation is what makes the code testable and clean.