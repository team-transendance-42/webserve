#include "../includes/EventLoop.hpp"

#include <cerrno>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/epoll.h>
#include <unistd.h>

#include "../includes/ErrorResponseBuilder.hpp"
#include "../includes/HttpResponse.hpp"
#include "../includes/Listener.hpp"

/*
 * Constructor: wires ConnectionManager with epoll callbacks pointing at the
 * single shared EpollLoop. The unified _clients map is passed by reference so
 * accept (here) and close (inside ConnectionManager) mutate the same registry.
 */
EventLoop::EventLoop()
    : _epoll(),
      _conn(_clients,
            _clientToListener,
            [this](int fd, uint32_t events) { _epoll.mod(fd, events); },
            [this](int fd) { _epoll.del(fd); }) {}

EventLoop::~EventLoop() {
    for (std::map<int, Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        close(it->first);
        delete it->second;
    }
}

void EventLoop::init() {
    _epoll.init();
}

/*
 * Register a Listener's listen socket with the shared epoll. EPOLLIN fires
 * whenever a new client is queued; tick() dispatches it back to _acceptFrom.
 */
void EventLoop::addListener(Listener *listener) {
    int lfd = listener->listenFd();
    if (lfd < 0)
        throw std::runtime_error("EventLoop::addListener: listener not initialised");
    _listenFds[lfd] = listener;
    _epoll.add(lfd, EPOLLIN);
}

void EventLoop::run(volatile sig_atomic_t &running) {
    while (running) tick();
}

/*
 * One epoll_wait iteration. Three dispatch buckets:
 *   1. fd is a listen socket → drain accept() until EAGAIN.
 *   2. fd is a known client → readClient / writeClient / closeClient by event mask.
 *   3. fd is unknown → silently skip (closed mid-batch).
 *
 * Idle clients are swept once per tick before event dispatch so a 408 lands
 * on the same iteration the timeout expires.
 */
void EventLoop::tick() {
    struct epoll_event events[MAX_EVENTS];
    int n = _epoll.wait(events, MAX_EVENTS, POLL_TIMEOUT_MS);

    if (n < 0) {
        if (errno == EINTR) return;
        throw std::runtime_error("epoll_wait() failed: " + std::string(strerror(errno)));
    }

    _closeIdleClients();

    if (n == 0) return;

    for (int i = 0; i < n; ++i) {
        int      fd = events[i].data.fd;
        uint32_t ev = events[i].events;

        std::map<int, Listener *>::iterator lit = _listenFds.find(fd);
        if (lit != _listenFds.end()) {
            _acceptFrom(lit->second);
            continue;
        }

        std::map<int, Client *>::iterator cit = _clients.find(fd);
        if (cit == _clients.end()) continue;
        Client &client = *cit->second;

        if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            _conn.closeClient(fd);
            continue;
        }
        if (ev & EPOLLIN)  _conn.readClient(client, READ_BUF);
        if (ev & EPOLLOUT) _conn.writeClient(client);
    }
}

/*
 * Drain the listen socket's pending backlog. Each accepted fd is registered
 * in the shared epoll and bound to the originating Listener so per-client
 * routing (config lookup, ProcessRequest) follows the connection.
 */
void EventLoop::_acceptFrom(Listener *listener) {
    while (true) {
        int cfd = listener->acceptOne();
        if (cfd < 0) break;
        _epoll.add(cfd, EPOLLIN | EPOLLRDHUP);
        _clients[cfd] = new Client(cfd);
        _clientToListener[cfd] = listener;
        std::cout << "[EventLoop] new client fd=" << cfd
                  << " on listen_fd=" << listener->listenFd() << "\n";
    }
}

/*
 * Walks every active client across every Listener. A client whose idle window
 * has elapsed (and isn't already mid-write) is handed a 408 built from its
 * owning Listener's first ServerConfig, then flipped to EPOLLOUT so the next
 * writeClient drains it and closes the socket (keep_alive=false).
 */
void EventLoop::_closeIdleClients() {
    time_t now = std::time(nullptr);
    for (std::map<int, Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        Client *client = it->second;
        if (now - client->lastTimestamp > CLIENT_TIMEOUT && client->writeBuf.empty()) {
            const ServerConfig &cfg = _clientToListener.at(client->fd)->configs()[0];
            std::cout << "[EventLoop] timeout: closing client fd=" << client->fd << "\n";
            client->writeBuf   = ErrorResponseBuilder::buildErrorResponse(408, cfg).serialize();
            client->keep_alive = false;
            // RFC 9110 §15.5.9: 408 must carry Connection: close so the client knows we're closing
            size_t pos = client->writeBuf.find("\r\n");
            if (pos != std::string::npos)
                client->writeBuf.insert(pos + 2, "Connection: close\r\n");
            _epoll.mod(client->fd, EPOLLOUT | EPOLLRDHUP);
        }
    }
}
