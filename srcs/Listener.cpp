#include "../includes/Listener.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

Listener::Listener(const std::vector<ServerConfig> &configs)
    : _listen_fd(-1),
      _configs(configs),
      _process_request(configs) {}

Listener::~Listener() {
    if (_listen_fd >= 0) close(_listen_fd);
}

/*
 * Create TCP listen socket, bind to host:port, listen, set non-blocking.
 * Does NOT touch epoll — EventLoop::addListener registers the fd.
 */
void Listener::init() {
    _listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listen_fd < 0)
        throw std::runtime_error("socket() failed: " + std::string(strerror(errno)));

    int opt = 1;
    if (setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        throw std::runtime_error("setsockopt() failed");

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(_configs[0].port);
    if (inet_pton(AF_INET, _configs[0].host.c_str(), &addr.sin_addr) != 1)
        throw std::runtime_error("invalid host address: " + _configs[0].host);

    if (bind(_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("bind() failed on " + _configs[0].host
                                 + ":" + std::to_string(_configs[0].port)
                                 + " — " + strerror(errno));

    if (listen(_listen_fd, BACKLOG) < 0)
        throw std::runtime_error("listen() failed: " + std::string(strerror(errno)));

    _setNonBlocking(_listen_fd);

    std::string names;
    for (size_t i = 0; i < _configs.size(); ++i) {
        if (!_configs[i].server_names.empty()) {
            if (!names.empty()) names += ", ";
            names += _configs[i].server_names[0];
        }
    }
    if (names.empty()) names = "(unnamed)";
    std::cout << "[Listener] [" << names << "] listening on "
              << _configs[0].host << ":" << _configs[0].port << "\n";
}

/*
 * Non-blocking accept. Returns the new client fd, or -1 when nothing more
 * is pending. EventLoop drives this in a loop until -1 (drains the backlog
 * on one EPOLLIN). errno is intentionally NOT inspected after accept — the
 * subject forbids it after a read/write operation. We treat any -1 as
 * "done for now"; spurious errors get logged once via the return value.
 */
int Listener::acceptOne() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int fd = accept(_listen_fd, (struct sockaddr *)&addr, &len);
    if (fd < 0) return -1;
    _setNonBlocking(fd);
    return fd;
}

void Listener::_setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        throw std::runtime_error("fcntl F_GETFL failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("fcntl F_SETFL failed");
}
