#include "../includes/Server.hpp"
#include <cstring>   // strerror
#include <cerrno>    // errno
#include <stdexcept>
#include <sstream>

// ─────────────────────────────────────────────
// Construction / Destruction
// ─────────────────────────────────────────────

Server::Server(const ServerConfig& config)
    : _config(config), _listenFd(-1), _running(false)
{}

Server::~Server()
{
    // Close listen socket + all client sockets
    for (auto& pfd : _fds)
        if (pfd.fd >= 0)
            close(pfd.fd);
}

// ─────────────────────────────────────────────
// init()  — socket → options → bind → listen
// ─────────────────────────────────────────────

void Server::init()
{
    // 1. Create TCP socket
    _listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listenFd < 0)
        throw std::runtime_error(std::string("socket() failed: ") + strerror(errno));

    // 2. Allow immediate reuse of the port after restart (avoid TIME_WAIT pain)
    int opt = 1;
    if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        throw std::runtime_error(std::string("setsockopt() failed: ") + strerror(errno));

    // 3. Bind to host:port
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(static_cast<uint16_t>(_config.port));
    addr.sin_addr.s_addr = inet_addr(_config.host.c_str());
    if (addr.sin_addr.s_addr == INADDR_NONE)
        addr.sin_addr.s_addr = INADDR_ANY; // fallback if host is empty / "0.0.0.0"

    if (bind(_listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error(std::string("bind() failed: ") + strerror(errno));

    // 4. Listen
    if (listen(_listenFd, BACKLOG) < 0)
        throw std::runtime_error(std::string("listen() failed: ") + strerror(errno));

    // 5. Non-blocking so poll() never gets stuck on accept()
    _setNonBlocking(_listenFd);

    // 6. Register listen fd in poll set
    pollfd pfd{};
    pfd.fd     = _listenFd;
    pfd.events = POLLIN;
    _fds.push_back(pfd);
    _requests.push_back(""); // placeholder, never used for listen fd

    std::cout << "[Server] Listening on "
              << _config.host << ":" << _config.port << std::endl;
}

// ─────────────────────────────────────────────
// run()  — main poll() loop
// ─────────────────────────────────────────────

void Server::run()
{
    _running = true;

    while (_running)
    {
        // poll() blocks until an event fires or timeout expires
        int ready = poll(_fds.data(), static_cast<nfds_t>(_fds.size()), POLL_TIMEOUT);

        if (ready < 0)
        {
            if (errno == EINTR) // interrupted by signal — clean exit
                break;
            throw std::runtime_error(std::string("poll() failed: ") + strerror(errno));
        }

        if (ready == 0)
            continue; // timeout — loop again (lets _running be re-checked)

        // Walk fds from the end so removal by index is safe
        for (size_t i = _fds.size(); i-- > 0; )
        {
            if (_fds[i].revents == 0)
                continue;

            if (_fds[i].fd == _listenFd)
                _acceptClient();
            else
                _handleClient(_fds[i].fd, i);
        }
    }

    std::cout << "[Server] Shutting down." << std::endl;
}

void Server::stop()
{
    _running = false;
}

// ─────────────────────────────────────────────
// _acceptClient()  — called when listen fd is readable
// ─────────────────────────────────────────────

void Server::_acceptClient()
{
    sockaddr_in clientAddr{};
    socklen_t   len = sizeof(clientAddr);

    int clientFd = accept(_listenFd,
                          reinterpret_cast<sockaddr*>(&clientAddr), &len);
    if (clientFd < 0)
    {
        // EAGAIN/EWOULDBLOCK is normal in non-blocking mode — no client ready
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            std::cerr << "[Server] accept() error: " << strerror(errno) << std::endl;
        return;
    }

    _setNonBlocking(clientFd);

    pollfd pfd{};
    pfd.fd     = clientFd;
    pfd.events = POLLIN;
    _fds.push_back(pfd);
    _requests.push_back("");

    std::cout << "[Server] New client fd=" << clientFd
              << "  addr=" << inet_ntoa(clientAddr.sin_addr) << std::endl;
}

// ─────────────────────────────────────────────
// _handleClient()  — read request, send minimal HTTP response
// ─────────────────────────────────────────────

void Server::_handleClient(int clientFd, size_t pollIdx)
{
    char buf[READ_BUF];
    ssize_t n = read(clientFd, buf, sizeof(buf) - 1);

    if (n <= 0)
    {
        // n == 0 → client closed connection
        // n <  0 → read error
        if (n < 0 && errno != EAGAIN)
            std::cerr << "[Server] read() error on fd=" << clientFd
                      << ": " << strerror(errno) << std::endl;
        _removeClient(pollIdx);
        return;
    }

    buf[n] = '\0';
    _requests[pollIdx] += buf;

    // Minimal HTTP/1.1: wait for the full header block (\r\n\r\n)
    if (_requests[pollIdx].find("\r\n\r\n") == std::string::npos)
        return; // not done reading headers yet

    // ── Build a trivial response ──────────────────────────────────────
    std::string body =
        "<html><body><h1>42 Webserv</h1>"
        "<p>Server is alive.</p></body></html>";

    std::string response = _buildResponse(body);
    ssize_t sent = write(clientFd, response.c_str(), response.size());
    if (sent < 0)
        std::cerr << "[Server] write() error: " << strerror(errno) << std::endl;

    // HTTP/1.0-style: close after one response (no keep-alive yet)
    _removeClient(pollIdx);
}

// ─────────────────────────────────────────────
// _removeClient()
// ─────────────────────────────────────────────

void Server::_removeClient(size_t pollIdx)
{
    std::cout << "[Server] Closing client fd=" << _fds[pollIdx].fd << std::endl;
    close(_fds[pollIdx].fd);
    _fds.erase(_fds.begin() + static_cast<long>(pollIdx));
    _requests.erase(_requests.begin() + static_cast<long>(pollIdx));
}

// ─────────────────────────────────────────────
// _buildResponse()  — bare-minimum HTTP/1.1 response
// ─────────────────────────────────────────────

std::string Server::_buildResponse(const std::string& body, int status)
{
    std::string statusText = (status == 200) ? "OK" : "Error";
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << " " << statusText << "\r\n"
        << "Content-Type: text/html\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

// ─────────────────────────────────────────────
// _setNonBlocking()  — O_NONBLOCK via fcntl
// ─────────────────────────────────────────────

void Server::_setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        throw std::runtime_error(std::string("fcntl(F_GETFL) failed: ") + strerror(errno));
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error(std::string("fcntl(F_SETFL) failed: ") + strerror(errno));
}