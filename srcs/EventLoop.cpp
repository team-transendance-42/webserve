#include "../includes/EventLoop.hpp"

#include <cerrno>
#include <cstring>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
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
      _conn(_clients, _clientToListener, [this](int fd, uint32_t events) {
                if (!_epoll.mod(fd, events)) {
                    int err = errno;
                    std::cerr << "[EventLoop] epoll MOD fd=" << fd
                              << " (" << strerror(err) << ") — connection will time out\n";
                }
            }, [this](int fd) { _epoll.del(fd); }) {}

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
    if (!_epoll.add(lfd, EPOLLIN)) {
        int err = errno;
        throw std::runtime_error("EventLoop::addListener: epoll ADD failed for listen fd="
                                 + std::to_string(lfd) + " (" + strerror(err) + ")");
    }
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
        throw std::runtime_error("epoll_wait() failed: " + std::string(strerror(errno))); //EBADF/EINVAL here means the epoll fd is broken — unrecoverable, throw is correct. 
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
        if (ev & EPOLLIN) {
            _conn.readClient(client, READ_BUF);
            /* readClient may have closed and deleted the client (disconnect, parse error).
               Re-check before touching client again — use-after-free otherwise. */
            if (_clients.find(fd) == _clients.end()) continue;
        }
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
        if (cfd < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                std::cerr << "[EventLoop] accept failed on listen_fd="
                          << listener->listenFd() << " (" << strerror(errno) << ")\n";
            break;
        }
        if (!_epoll.add(cfd, EPOLLIN | EPOLLRDHUP)) {
            int err = errno;
            std::cerr << "[EventLoop] epoll ADD failed for new client fd=" << cfd
                      << " (" << strerror(err) << ") — dropping connection\n";
            close(cfd);
            continue;
        }
        _clients[cfd] = new Client(cfd);
        _clientToListener[cfd] = listener;
        std::cout << "[EventLoop] new client fd=" << cfd
                  << " on listen_fd=" << listener->listenFd() << "\n";
    }
}

/* Sends 408 to clients idle past CLIENT_TIMEOUT and arms them for EPOLLOUT.
   On epoll MOD failure: collect the fd and close after the loop —
   closing inside the loop would invalidate the _clients iterator. */
void EventLoop::_closeIdleClients() {
    time_t now = std::time(nullptr);
    std::vector<int> toForceClose;
    for (std::map<int, Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        Client *client = it->second;
        if (now - client->lastTimestamp > CLIENT_TIMEOUT && client->writeBuf.empty()) {
            std::map<int, Listener *>::iterator listenerIt = _clientToListener.find(client->fd);
            if (listenerIt == _clientToListener.end()) {
                /* maps out of sync — fd in _clients but not in _clientToListener.
                   at() would throw here; collect for cleanup instead. */
                toForceClose.push_back(client->fd);
                continue;
            }
            const ServerConfig &cfg = listenerIt->second->configs()[0];
            std::cout << "[EventLoop] timeout: closing client fd=" << client->fd << "\n";
            client->writeBuf   = ErrorResponseBuilder::buildErrorResponse(408, cfg).serialize();
            client->keep_alive = false;
            if (!_epoll.mod(client->fd, EPOLLOUT | EPOLLRDHUP)) {
                int err = errno;
                std::cerr << "[EventLoop] epoll MOD failed for timeout fd=" << client->fd
                          << " (" << strerror(err) << ") — force closing\n";
                toForceClose.push_back(client->fd);
            }
        }
    }
    for (size_t i = 0; i < toForceClose.size(); ++i)
        _conn.closeClient(toForceClose[i]);
}
