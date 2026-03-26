#include "../includes/Server.hpp"
#include "../includes/HttpResponse.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>

/**
 * lambda expression: [this](int fd, uint32_t events) { _epoll.mod(fd, events); }
 * [ ] = capture list that specifies what variables/pointers the lambda can access
this = captures the pointer to the current object (the Server instance)
Effect: Inside the lambda body, we can access all member variables and methods of Server like _epoll, _clients, etc.
Without [this], calling _epoll.mod() would fail—the lambda wouldn't know what _epoll is

Each browser tab or curl command is a separate client (separate TCP connection, separate fd).
When a client disconnects, we remove its fd from epoll and close it.
Server fd: only for new connections.
Client fd: for reading requests and writing responses.
epoll manages all active fds and notifies you when they’re ready for I/O.
 */
Server::Server(const ServerConfig &config)
    : _config(config),
        _listenFd(-1),
        _epoll(),
        _running(true),
        _processRequest(_config),
        _connectionManager(
                _clients,
            [this](int fd, uint32_t events) { _epoll.mod(fd, events); },
            [this](int fd) { _epoll.del(fd); },
            _processRequest) {} // map *clients is auto initialized as empty, we will add client objects to it in _acceptClient when new clients connect

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
    _epoll.init(); // create epoll instance, store fd internally

    _listenFd = socket(AF_INET, SOCK_STREAM, 0); // 2. TCP socket
    if (_listenFd < 0)
        throw std::runtime_error("socket() failed: "
            + std::string(strerror(errno)));

    int opt = 1;
    if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) // 3. SO_REUSEADDR — a socket option,  lets quick restart the server on the same port, even if the previous socket is in TIME_WAIT state.
        throw std::runtime_error("setsockopt() failed");

    // 4. bind
    struct sockaddr_in addr; // hold the address info for the socket.
    std::memset(&addr, 0, sizeof(addr)); // Sets all bytes of addr to zero (clears memory). This prevents garbage values and ensures all fields are initialized.
    addr.sin_family      = AF_INET; //Sets the address family to IPv4
    addr.sin_port        = htons(_config.port); // htons converts the port number from host byte order to network byte order (big-endian). This is necessary for correct communication over the network, as different machines may have different byte orders.
    if (_config.host == "localhost")
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    else
        addr.sin_addr.s_addr = inet_addr(_config.host.c_str());

    if (bind(_listenFd, (struct sockaddr *)&addr, sizeof(addr)) < 0) //s ssigns address/port to server socket (_listenFd).  “I want to receive connections on this IP and port"
        throw std::runtime_error("bind() failed on "
            + _config.host + ":" + std::to_string(_config.port)
            + " — " + strerror(errno));

    if (listen(_listenFd, BACKLOG) < 0) // // 5. listen, ready to accept connections. BACKLOG = max pending connections in queue before new ones are refused.
        throw std::runtime_error("listen() failed: "
            + std::string(strerror(errno)));

    _setNonBlocking(_listenFd);

    _epoll.add(_listenFd, EPOLLIN); // 7. register server with epoll to wait for incoming connection events (EPOLLIN on _listenFd means new client is trying to connect)

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
    struct epoll_event events[maxEvents];

    // ONE epoll_wait call — returns after POLL_TIMEOUT ms if nothing happens
    // this lets main() check g_running regularly
	// numReadyEvents = num of fds ready for i/o: how many events are avail in the events arr
    int numReadyEvents = _epoll.wait(events, maxEvents, POLL_TIMEOUT);

    for (auto it = _clients.begin(); it != _clients.end();) {
        Client* client = it->second;
        if (std::time(0) - client->lastTimestamp > SERVER_TIMEOUT) {
            std::cout << "[Server] closing idle client fd=" << client->fd << "\n";
            // set res 408 Request Timeout before closing, for HTTP compliance. (optional, since client will just see connection close without response anyway)
            // HttpResponse resp = HttpResponse::make_408();
            // std::string msg = resp.serialize();
            // send(client->fd, msg.c_str(), msg.size(), 0);
            _connectionManager.closeClient(client->fd);
            it = begin(_clients); // reset iterator to beginning after erasing current client, since erasing invalidates the iterator. We will check all clients again for idle timeout in the next tick.
        } else
            ++it; // only iterate if not erasing Client, otherwise after erasing, the iterator becomes invalid and we cannot increment it. If we erase, we do not increment because the next Client will now be at the same iterator position after erasing the current one.
    }

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

    // Simulate server hang: do not respond, just sleep forever to trigger client timeout
    // while (true) {
    //     sleep(1000); 
    // }
    // (This code will never reach the response handling)
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
        if (clientFd == 0) {
            std::cerr << "[accept] returned fd=0 (invalid client), skipping.\n";
            continue;
        }
        _setNonBlocking(clientFd);
        _epoll.add(clientFd, EPOLLIN | EPOLLRDHUP);
        _clients[clientFd] = new Client(clientFd);

        std::cout << "[Server] new client fd=" << clientFd << "\n";
    }
}

// ── utils ─────────────────────────────────────────────────────────────────────

void Server::_setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        throw std::runtime_error("fcntl F_GETFL failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("fcntl F_SETFL failed");
}

