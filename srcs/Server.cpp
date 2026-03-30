#include "../includes/Server.hpp"
#include "../includes/HttpResponse.hpp"
#include "../includes/ErrorResponseBuilder.hpp"
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
    :  _config(config),
        _listen_fd(-1),
        _epoll(), // has default constructor that initializes internal fd
        _running(true),
        _process_request(_config),
        _connection_manager(
                _clients,
            [this](int fd, uint32_t events) { _epoll.mod(fd, events); },
            [this](int fd) { _epoll.del(fd); },
            _process_request) {} // map *clients is auto initialized as empty, we will add client objects to it in _acceptClient when new clients connect

Server::~Server() {
    typedef std::map<int, Client *>::iterator It;
    for (It it = _clients.begin(); it != _clients.end(); ++it) {
        close(it->first);
        delete it->second;// pointer to heap obj created by new in _acceptClient, we need to free the memory
    }
    if (_listen_fd >= 0) close(_listen_fd);
}

// ── init ──────────────────────────────────────────────────────────────────────

void Server::init() {
    _epoll.init(); // create epoll instance, store fd internally

    _listen_fd = socket(AF_INET, SOCK_STREAM, 0); // 2. TCP socket
    if (_listen_fd < 0)
        throw std::runtime_error("socket() failed: "
            + std::string(strerror(errno)));

    int opt = 1;
    if (setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) // 3. SO_REUSEADDR — a socket option,  lets quick restart the server on the same port, even if the previous socket is in TIME_WAIT state.
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

    if (bind(_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) //s ssigns address/port to server socket (_listen_fd).  “I want to receive connections on this IP and port"
        throw std::runtime_error("bind() failed on "
            + _config.host + ":" + std::to_string(_config.port)
            + " — " + strerror(errno));

    if (listen(_listen_fd, BACKLOG) < 0) // // 5. listen, ready to accept connections. BACKLOG = max pending connections in queue before new ones are refused.
        throw std::runtime_error("listen() failed: "
            + std::string(strerror(errno)));

    _setNonBlocking(_listen_fd);

    _epoll.add(_listen_fd, EPOLLIN); // 7. register server with epoll to wait for incoming connection events (EPOLLIN on _listen_fd means new client is trying to connect)

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

 void Server::handleServerTimeout() {
    for (auto it = _clients.begin(); it != _clients.end();) {
        Client* client = it->second;
        if (std::time(0) - client->lastTimestamp > SERVER_TIMEOUT) {
            int fd = client->fd;
            std::cout << "[Server] timeout: closing client fd=" << fd << "\n";
            std::string resp = ErrorResponseBuilder::buildErrorResponse(408, _config).serialize();
            send(fd, resp.c_str(), resp.size(), 0);
            it = ++it; // advance BEFORE closeClient removes the entry
            _connection_manager.closeClient(fd); // handles delete + erase + epoll.del + close
        } else
            ++it;
    }
 }
 /**
 * One event-loop step: wait for ready fds, accept new clients,
 * and route client events to read/write/close handlers.
*/

void Server::tick() {
    struct epoll_event events[maxEvents];

    int numReadyEvents = _epoll.wait(events, maxEvents, POLL_TIMEOUT);

    if (numReadyEvents < 0) {
        if (errno == EINTR) return;  // signal interrupted — main will check g_running
        throw std::runtime_error("epoll_wait() failed: "
            + std::string(strerror(errno)));
    }
    handleServerTimeout() ; // check for idle clients and close them before processing ready events
    
    if (numReadyEvents == 0) return;  // timeout — nothing ready, return to main

    for (int i = 0; i < numReadyEvents; i++) {
        int      fd = events[i].data.fd;
        uint32_t ev = events[i].events;

        if (fd == _listen_fd) {
            _acceptClient();
            continue;
        }

        auto it = _clients.find(fd);
        if (it == _clients.end()) continue;
        Client &client = *it->second;

        if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            _connection_manager.closeClient(fd);
            continue;
        }
        if (ev & EPOLLIN)  _connection_manager.readClient(client, READ_BUF);
        if (ev & EPOLLOUT) _connection_manager.writeClient(client);
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

        int clientFd = accept(_listen_fd, (struct sockaddr *)&addr, &len);
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

