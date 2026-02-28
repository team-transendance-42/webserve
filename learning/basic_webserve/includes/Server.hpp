#pragma once

#include "ConfigParser.hpp"
#include <string>
#include <vector>
#include <poll.h>       // pollfd, poll()
#include <sys/socket.h> // socket(), bind(), listen(), accept()
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h>  // inet_addr()
#include <unistd.h>     // close(), read(), write()
#include <fcntl.h>      // fcntl() for non-blocking
#include <stdexcept>
#include <iostream>
#include <sstream>

/*
** Server
** ------
** Owns one listening socket (fd).
** Uses poll() to multiplex the listen fd + all client fds.
**
** Lifecycle:
**   Server srv(config);
**   srv.init();   // socket → setsockopt → bind → listen → fcntl non-block
**   srv.run();    // poll loop — blocks until SIGINT or error
*/

class Server
{
public:
    explicit Server(const ServerConfig& config);
    ~Server();

    // No copy — owning a raw fd
    Server(const Server&)            = delete;
    Server& operator=(const Server&) = delete;

    void init();   // call once before run()
    void run();    // blocking poll() loop
    void stop();   // sets _running = false (call from signal handler)

    // Accessors (useful for tests)
    int  getFd()     const { return _listenFd; }
    bool isRunning() const { return _running;  }

private:
    // --- helpers ---
    void        _acceptClient();
    void        _handleClient(int clientFd, size_t pollIdx);
    void        _removeClient(size_t pollIdx);
    std::string _buildResponse(const std::string& body, int status = 200);
    static void _setNonBlocking(int fd);

    // --- state ---
    ServerConfig             _config;
    int                      _listenFd;
    bool                     _running;
    std::vector<pollfd>      _fds;      // [0] = listen socket, [1..] = clients
    std::vector<std::string> _requests; // partial read buffer per client (parallel to _fds)

    static constexpr int BACKLOG      = 128;
    static constexpr int POLL_TIMEOUT = 5000; // ms
    static constexpr size_t READ_BUF  = 4096;
};