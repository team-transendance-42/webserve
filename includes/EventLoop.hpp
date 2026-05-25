#pragma once

#include <csignal>
#include <map>
#include "ConnectionManager.hpp"
#include "EpollLoop.hpp"
#include "Listener.hpp"

/*
 * EventLoop owns the single epoll instance for the whole process.
 * The subject requires exactly one poll()/equivalent for all I/O between
 * clients and the server, listen sockets included — this class enforces that.
 *
 * Responsibilities:
 *   - one EpollLoop (one epoll_create1) shared across all listeners and clients
 *   - listener registry (listen_fd → Listener*) used to dispatch EPOLLIN on accept
 *   - unified client registry (client_fd → Client*)
 *   - one ConnectionManager
 *   - idle-client sweep (the 408 path)
 *
 * Usage:
 *   EventLoop loop;
 *   loop.init();
 *   for (auto *l : listeners) loop.addListener(l);
 *   while (running) loop.tick();
 */
class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop &) = delete;
    EventLoop &operator=(const EventLoop &) = delete;

    void init();
    void addListener(Listener *listener);
    void tick();
    void run(volatile sig_atomic_t &running);

private:
    void _acceptFrom(Listener *listener);
    void _closeIdleClients();

    enum {
        POLL_TIMEOUT_MS = 100,
        MAX_EVENTS      = 64,
        READ_BUF        = 4096,
        CLIENT_TIMEOUT  = 6
    };

    EpollLoop                  _epoll;
    std::map<int, Listener *>  _listenFds;
    std::map<int, Client *>    _clients;
    std::map<int, Listener *>  _clientToListener;
    ConnectionManager          _conn;
};
