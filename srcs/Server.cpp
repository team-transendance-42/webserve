#include "../includes/Server.hpp"

// ── ctor / dtor ───────────────────────────────────────────────────────────────

Server::Server(const ServerConfig &config)
        : _config(config),
            _router(_config),
            _listenFd(-1),
        _epoll(),
            _running(true),
            _requestProcessor(_config, _router),
            _connectionManager(
                    _clients,
                [this](int fd, uint32_t events) { _epoll.mod(fd, events); },
                [this](int fd) { _epoll.del(fd); },
                [this](Client &client) { _requestProcessor.handle(client); }) {
		// todo: missing init _clients
	}

Server::~Server() {
    typedef std::map<int, Client *>::iterator It;
    for (It it = _clients.begin(); it != _clients.end(); ++it) {
        close(it->first);
        delete it->second;// pointer to heap obj created by new in _acceptClient, we need to free the memory
    }
    if (_listenFd >= 0) close(_listenFd);
}

// ── init ──────────────────────────────────────────────────────────────────────

void Server::init() {
    // 1. epoll instance
    _epoll.init();

    // 2. TCP socket
    _listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listenFd < 0)
        throw std::runtime_error("socket() failed: "
            + std::string(strerror(errno)));

    // 3. SO_REUSEADDR — no "address already in use" on restart
    int opt = 1;
    if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        throw std::runtime_error("setsockopt() failed");

    // 4. bind
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr)); // todo: do we need it? memory....
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(_config.port);
    addr.sin_addr.s_addr = inet_addr(_config.host.c_str());

    if (bind(_listenFd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("bind() failed on "
            + _config.host + ":" + std::to_string(_config.port)
            + " — " + strerror(errno));

    // 5. listen
    if (listen(_listenFd, BACKLOG) < 0)
        throw std::runtime_error("listen() failed: "
            + std::string(strerror(errno)));

    // 6. non-blocking
    _setNonBlocking(_listenFd);

    // 7. register with epoll
    _epoll.add(_listenFd, EPOLLIN);

    std::cout << "[Server] '" << _config.server_names[0]
              << "' listening on " << _config.host
              << ":" << _config.port << "\n";
}

// ── tick ───────────────────────────────────────────────────────────────────────

/*
    EPOLLERR: An error occurred on the file descriptor (e.g., socket error).
	EPOLLHUP: The file descriptor was "hung up" (connection closed by peer).
	EPOLLRDHUP: The peer closed its read end (remote shutdown).
	The & operator in this context is a bitwise AND. It checks if any of the specified event flags (EPOLLERR, EPOLLHUP, EPOLLRDHUP) are set in ev.

	If ev contains any of those flags, the result is non-zero, so the condition is true. This is a common way to test for specific bits in a bitmask.
 */

 /**
 * One event-loop step: wait for ready fds, accept new clients,
 * and route client events to read/write/close handlers.
*/

void Server::tick() {
    struct epoll_event events[MAX_EVENTS];

    // ONE epoll_wait call — returns after POLL_TIMEOUT ms if nothing happens
    // this lets main() check g_running regularly
	// numReadyEvents = num of fds ready for i/o: how many events are avail in the events arr
    int numReadyEvents = _epoll.wait(events, MAX_EVENTS, POLL_TIMEOUT);

    if (numReadyEvents < 0) {
        if (errno == EINTR) return;  // signal interrupted — main will check g_running
        throw std::runtime_error("epoll_wait() failed: "
            + std::string(strerror(errno)));
    }
    if (numReadyEvents == 0) return;  // timeout — nothing ready, return to main

    for (int i = 0; i < numReadyEvents; i++) {
        int      fd = events[i].data.fd;
        uint32_t ev = events[i].events;

        if (fd == _listenFd) {
            _acceptClient();
            continue;
        }

        auto it = _clients.find(fd);
        if (it == _clients.end()) continue;
        Client &client = *it->second;

        if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            _connectionManager.closeClient(fd);
            continue;
        }
        if (ev & EPOLLIN)  _connectionManager.readClient(client, READ_BUF);
        if (ev & EPOLLOUT) _connectionManager.writeClient(client);
    }
}

void Server::stop() {
    _running = false;
    std::cout << "[Server] stopping '" << _config.server_names[0] << "'\n";
}

// ── _acceptClient ─────────────────────────────────────────────────────────────

void Server::_acceptClient() {
    while (true) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);

        int clientFd = accept(_listenFd, (struct sockaddr *)&addr, &len);
        if (clientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            std::cerr << "[accept] failed: " << strerror(errno) << "\n";
            break;
        }

        _setNonBlocking(clientFd);
        _epoll.add(clientFd, EPOLLIN | EPOLLRDHUP);
        _clients[clientFd] = new Client(clientFd);

        std::cout << "[Server] new client fd=" << clientFd << "\n";
    }
}

// ── utils ─────────────────────────────────────────────────────────────────────

// todo: catch somewhere the throw
void Server::_setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        throw std::runtime_error("fcntl F_GETFL failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("fcntl F_SETFL failed");
}

