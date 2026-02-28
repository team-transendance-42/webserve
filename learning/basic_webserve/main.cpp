/*
** main.cpp
** --------
** Entry point for pettop webserv.
**
** Compile:
**   c++ -std=c++17 -Wall -Wextra main.cpp srcs/Server.cpp srcs/HttpRequest.cpp srcs/HttpResponse.cpp srcs/ConfigParser.cpp -o webserv
**
** Run:
**   ./webserv config.conf
**   ./webserv               (falls back to default.conf)
*/

#include "includes/ConfigParser.hpp"
#include "includes/HttpRequest.hpp"
#include "includes/HttpResponse.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <csignal>

// poll + sockets
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>

// ─────────────────────────────────────────────
// Global flag — set by SIGINT / SIGTERM
// ─────────────────────────────────────────────

static volatile sig_atomic_t g_running = 1;

static void onSignal(int) { g_running = 0; }

// ─────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────

static void setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) { perror("fcntl F_GETFL"); return; }
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int createListenSocket(const ServerConfig& cfg)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(static_cast<uint16_t>(cfg.port));
    addr.sin_addr.s_addr = inet_addr(cfg.host.c_str());
    if (addr.sin_addr.s_addr == INADDR_NONE)
        addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 128) < 0)
    {
        perror("listen");
        close(fd);
        return -1;
    }

    setNonBlocking(fd);
    return fd;
}

// ─────────────────────────────────────────────
// poll() loop state
// ─────────────────────────────────────────────

struct Client
{
    int         fd;
    std::string buf;   // accumulates raw bytes from read()
};

// ─────────────────────────────────────────────
// Handle one readable client fd
// Returns true if client should be removed
// ─────────────────────────────────────────────

static bool handleClient(Client& c, const ServerConfig& cfg)
{
    char buf[8192];
    ssize_t n = read(c.fd, buf, sizeof(buf) - 1);

    if (n <= 0)
        return true; // connection closed or error → remove

    buf[n] = '\0';
    c.buf += buf;

    // Feed bytes into HttpRequest — wait until complete
    HttpRequest req;
    req.feed(c.buf);
    if (!req.isComplete())
        return false; // need more data

    // Log the request
    std::cout << "[" << cfg.host << ":" << cfg.port << "] "
              << req.method() << " " << req.uri() << "\n";

    // Build and send the response
    HttpResponse res(req, cfg);
    std::string  raw = res.build();

    std::cout << "  -> " << res.status() << "\n";

    ssize_t sent = write(c.fd, raw.c_str(), raw.size());
    if (sent < 0)
        std::cerr << "  write() error: " << strerror(errno) << "\n";

    return true; // close after one response (HTTP/1.0 style, no keep-alive yet)
}

// ─────────────────────────────────────────────
// Run the server for one ServerConfig
// (in production you'd loop over multiple configs)
// ─────────────────────────────────────────────

static void runServer(const ServerConfig& cfg)
{
    int listenFd = createListenSocket(cfg);
    if (listenFd < 0)
        throw std::runtime_error("Failed to create listen socket");

    std::cout << "Listening on " << cfg.host << ":" << cfg.port
              << "  root=" << cfg.root << "\n";

    std::vector<pollfd>  fds;
    std::vector<Client>  clients;

    // Register listen socket
    pollfd pf{};
    pf.fd     = listenFd;
    pf.events = POLLIN;
    fds.push_back(pf);
    clients.push_back({listenFd, ""}); // placeholder — never used for read

    while (g_running)
    {
        int ready = poll(fds.data(), static_cast<nfds_t>(fds.size()), 3000);

        if (ready < 0)
        {
            if (errno == EINTR) break;
            throw std::runtime_error(std::string("poll() failed: ") + strerror(errno));
        }
        if (ready == 0)
            continue; // timeout — re-check g_running

        // Walk backwards so erase() by index is safe
        for (size_t i = fds.size(); i-- > 0;)
        {
            if (fds[i].revents == 0)
                continue;

            // ── listen fd: accept new connection ──────────────────────────
            if (fds[i].fd == listenFd)
            {
                sockaddr_in ca{}; socklen_t len = sizeof(ca);
                int clientFd = accept(listenFd,
                                      reinterpret_cast<sockaddr*>(&ca), &len);
                if (clientFd < 0) continue;
                setNonBlocking(clientFd);

                pollfd cpf{};
                cpf.fd     = clientFd;
                cpf.events = POLLIN;
                fds.push_back(cpf);
                clients.push_back({clientFd, ""});

                std::cout << "  [+] client fd=" << clientFd
                          << " from " << inet_ntoa(ca.sin_addr) << "\n";
                continue;
            }

            // ── client fd: read, parse, respond ───────────────────────────
            bool done = handleClient(clients[i], cfg);
            if (done)
            {
                std::cout << "  [-] client fd=" << fds[i].fd << "\n";
                close(fds[i].fd);
                fds.erase(fds.begin()     + static_cast<long>(i));
                clients.erase(clients.begin() + static_cast<long>(i));
            }
        }
    }

    // Cleanup: close all open fds
    for (auto& pfd : fds)
        close(pfd.fd);

    std::cout << "\nServer stopped.\n";
}

// ─────────────────────────────────────────────
// main
// ─────────────────────────────────────────────

int main(int argc, char* argv[])
{
    // Signal handling — graceful shutdown on Ctrl+C
    struct sigaction sa{};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // Config file from argv or default
    std::string configFile = (argc > 1) ? argv[1] : "default.conf";

    std::cout << "webserv — loading " << configFile << "\n";

    ServerConfig cfg;
    try
    {
        ConfigParser parser;
        cfg = parser.parse(configFile);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Config error: " << e.what() << "\n";
        return 1;
    }

    std::cout << "Config loaded:\n"
              << "  host  = " << cfg.host  << "\n"
              << "  port  = " << cfg.port  << "\n"
              << "  root  = " << cfg.root  << "\n"
              << "  index = " << cfg.index << "\n"
              << "  error_404 = " << cfg.error_404 << "\n"
              << "  max_body  = " << cfg.client_max_body_size << "\n"
              << "  methods   = ";
    for (auto& m : cfg.methods) std::cout << m << " ";
    std::cout << "\n\n";

    try
    {
        runServer(cfg);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }

    return 0;
}