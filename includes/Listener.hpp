#pragma once

#include <vector>
#include "ProcessRequest.hpp"
#include "config/Config.hpp"

/*
 * Listener owns one TCP listening socket and the configs (virtual hosts)
 * served on that (host, port). It does NOT own an epoll instance and does
 * NOT run an event loop — EventLoop drives all I/O.
 *
 * Lifecycle:
 *   Listener l(configs);
 *   l.init();                              // socket → setsockopt → bind → listen → non-block
 *   loop.addListener(&l);                  // EventLoop registers listen_fd with the shared epoll
 *   // ... event loop runs, calls l.acceptOne() when listen_fd fires EPOLLIN
 */
class Listener {
public:
    explicit Listener(const std::vector<ServerConfig> &configs);
    Listener(const Listener &) = delete;
    Listener &operator=(const Listener &) = delete;
    ~Listener();

    void init();           // socket, bind, listen, non-block
    int  acceptOne();      // returns new client fd, or -1 on EAGAIN/EWOULDBLOCK

    int                              listenFd()       const { return _listen_fd; }
    const std::vector<ServerConfig> &configs()        const { return _configs; }
    const ProcessRequest            &processRequest() const { return _process_request; }

private:
    static void _setNonBlocking(int fd);

    enum { BACKLOG = 128 };

    int                       _listen_fd;
    std::vector<ServerConfig> _configs;
    ProcessRequest            _process_request;
};
