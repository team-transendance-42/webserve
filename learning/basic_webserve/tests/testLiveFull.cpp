/*
** testLiveFull.cpp
** ----------------
** Wires HttpRequest + HttpResponse into a real Server and fires
** actual HTTP requests at localhost:8080.
**
** Creates a small www/ tree under /tmp/live_www/ so responses
** serve real files.
**
** Compile:
**   c++ -std=c++17 -Wall -Wextra tests/testLiveFull.cpp srcs/Server.cpp srcs/HttpRequest.cpp  srcs/HttpResponse.cpp srcs/ConfigParser.cpp -o testLiveFull
**
** Run auto tests:
**   ./testLiveFull
**
** Keep server alive (curl / browser):
**   ./testLiveFull --keep-alive
*/

#include "../includes/HttpRequest.hpp"
#include "../includes/HttpResponse.hpp"
#include "../includes/Server.hpp"
#include "../includes/ConfigParser.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

namespace fs = std::filesystem;

// ─── colours ─────────────────────────────────────────────────────────────────
#define GRN  "\033[32m"
#define RED  "\033[31m"
#define YEL  "\033[33m"
#define CYN  "\033[36m"
#define MAG  "\033[35m"
#define BOLD "\033[1m"
#define RST  "\033[0m"

static constexpr int    PORT    = 8080;
static constexpr size_t BUFSZ   = 16384;
static const std::string WWW    = "/tmp/live_www";

static bool allPassed = true;
#define PASS(n) do { std::cout << GRN "[PASS] " RST << (n) << "\n"; } while(0)
#define FAIL(n) do { std::cout << RED "[FAIL] " RST << (n) << "\n"; allPassed = false; } while(0)

// ─────────────────────────────────────────────────────────────────────────────
// NOTE: Server.cpp currently has a hardcoded response body.
// For this live test we patch the behaviour by subclassing Server
// via a patched _handleClient — but that requires access to private methods.
//
// Instead, we compile a PATCHED copy of Server logic directly here,
// so this file is self-contained and you can see exactly how
// HttpRequest + HttpResponse plug in.
//
// After validating here, copy the _handleClient() body below back
// into Server.cpp to replace the hardcoded response.
// ─────────────────────────────────────────────────────────────────────────────

// ═════════════════════════════════════════════════════════════════════════════
// MiniServer  — minimal poll() server with HttpRequest+HttpResponse wired in
// ═════════════════════════════════════════════════════════════════════════════

#include <poll.h>
#include <fcntl.h>
#include <cerrno>

class MiniServer
{
public:
    explicit MiniServer(const ServerConfig& cfg)
        : _cfg(cfg), _listenFd(-1), _running(false) {}

    ~MiniServer()
    {
        for (auto& p : _fds)
            if (p.fd >= 0) close(p.fd);
    }

    void init()
    {
        _listenFd = socket(AF_INET, SOCK_STREAM, 0);
        if (_listenFd < 0) throw std::runtime_error("socket() failed");

        int opt = 1;
        setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(static_cast<uint16_t>(_cfg.port));
        addr.sin_addr.s_addr = inet_addr(_cfg.host.c_str());
        if (bind(_listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
            throw std::runtime_error(std::string("bind() failed: ") + strerror(errno));

        listen(_listenFd, 128);
        _setNonBlocking(_listenFd);

        pollfd pf{}; pf.fd = _listenFd; pf.events = POLLIN;
        _fds.push_back(pf);
        _bufs.push_back("");
    }

    void run()
    {
        _running = true;
        while (_running)
        {
            int ready = poll(_fds.data(), static_cast<nfds_t>(_fds.size()), 3000);
            if (ready < 0) { if (errno == EINTR) break; throw std::runtime_error("poll()"); }
            if (ready == 0) continue;

            for (size_t i = _fds.size(); i-- > 0;)
            {
                if (_fds[i].revents == 0) continue;
                if (_fds[i].fd == _listenFd) _accept();
                else                         _handle(i);
            }
        }
    }

    void stop() { _running = false; }

private:
    // ── accept ────────────────────────────────────────────────────────────────
    void _accept()
    {
        sockaddr_in ca{}; socklen_t len = sizeof(ca);
        int fd = accept(_listenFd, reinterpret_cast<sockaddr*>(&ca), &len);
        if (fd < 0) return;
        _setNonBlocking(fd);
        pollfd pf{}; pf.fd = fd; pf.events = POLLIN;
        _fds.push_back(pf);
        _bufs.push_back("");
        std::cout << MAG "[server] accept fd=" << fd
                  << "  " << inet_ntoa(ca.sin_addr) << RST "\n";
    }

    // ── handle client — THIS IS THE KEY INTEGRATION POINT ────────────────────
    void _handle(size_t i)
    {
        char buf[BUFSZ];
        ssize_t n = read(_fds[i].fd, buf, sizeof(buf) - 1);

        if (n <= 0) { _remove(i); return; }
        buf[n] = '\0';
        _bufs[i] += buf;

        // Feed into HttpRequest — only respond once headers+body complete
        HttpRequest req;
        req.feed(_bufs[i]);
        if (!req.isComplete()) return;

        // Build response using config + parsed request
        HttpResponse res(req, _cfg);
        std::string raw = res.build();

        // Log to stdout so we can see what happened
        std::cout << CYN "  " << req.method() << " " << req.uri()
                  << "  →  " BOLD << res.status() << RST "\n";

        write(_fds[i].fd, raw.c_str(), raw.size());
        _remove(i);
    }

    void _remove(size_t i)
    {
        close(_fds[i].fd);
        _fds.erase(_fds.begin() + static_cast<long>(i));
        _bufs.erase(_bufs.begin() + static_cast<long>(i));
    }

    static void _setNonBlocking(int fd)
    {
        int fl = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    }

    ServerConfig             _cfg;
    int                      _listenFd;
    bool                     _running;
    std::vector<pollfd>      _fds;
    std::vector<std::string> _bufs;
};

// ─────────────────────────────────────────────────────────────────────────────
// www/ tree setup
// ─────────────────────────────────────────────────────────────────────────────

static void writeFile(const std::string& rel, const std::string& content)
{
    fs::path p = fs::path(WWW) / rel;
    fs::create_directories(p.parent_path());
    std::ofstream f(p);
    f << content;
}

static void setupWww()
{
    fs::remove_all(WWW);
    writeFile("index.html",    "<html><body><h1>Welcome to pettop webserv</h1></body></html>");
    writeFile("about.html",    "<html><body><h1>About</h1><p>C++17 webserver.</p></body></html>");
    writeFile("style.css",     "body { font-family: monospace; background: #111; color: #eee; }");
    writeFile("api/data.json", R"({"server":"webserv","version":"0.1","status":"ok"})");
    writeFile("img/pixel.png", "\x89PNG\r\n\x1a\n"); // minimal PNG header
    writeFile("404.html",      "<html><body><h1>404 — Not Found</h1></body></html>");
    writeFile("sub/index.html","<html><body><h1>Sub index</h1></body></html>");
}

static ServerConfig makeConfig()
{
    ServerConfig cfg;
    cfg.host                 = "127.0.0.1";
    cfg.port                 = PORT;
    cfg.root                 = WWW;
    cfg.index                = "index.html";
    cfg.error_404            = "404.html";
    cfg.client_max_body_size = 1024 * 1024;
    cfg.methods              = {"GET", "POST", "DELETE"};
    return cfg;
}

// ─────────────────────────────────────────────────────────────────────────────
// Fork helpers
// ─────────────────────────────────────────────────────────────────────────────

static pid_t forkServer()
{
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid == 0)
    {
        MiniServer srv(makeConfig());
        srv.init();
        srv.run();
        _exit(0);
    }
    usleep(120'000);
    return pid;
}

static void killServer(pid_t pid)
{
    kill(pid, SIGTERM);
    int st; waitpid(pid, &st, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// TCP client helpers
// ─────────────────────────────────────────────────────────────────────────────

static std::string sendRaw(const std::string& raw)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    { close(sock); return ""; }

    write(sock, raw.c_str(), raw.size());

    std::string resp;
    char buf[BUFSZ];
    ssize_t n;
    while ((n = read(sock, buf, sizeof(buf) - 1)) > 0)
    { buf[n] = '\0'; resp += buf; }
    close(sock);
    return resp;
}

static int statusOf(const std::string& resp)
{
    // "HTTP/1.1 200 OK\r\n..." → 200
    if (resp.size() < 12) return 0;
    try { return std::stoi(resp.substr(9, 3)); }
    catch (...) { return 0; }
}

static std::string bodyOf(const std::string& resp)
{
    size_t sep = resp.find("\r\n\r\n");
    return sep != std::string::npos ? resp.substr(sep + 4) : "";
}

// ─────────────────────────────────────────────────────────────────────────────
// Pretty print one test result
// ─────────────────────────────────────────────────────────────────────────────

static void printResult(const std::string& label,
                        const std::string& request,
                        const std::string& response)
{
    int  code = statusOf(response);
    bool ok   = (code >= 100 && code < 600);

    std::cout << "\n" BOLD "── " << label << " ──" RST "\n";
    // first line of request
    std::cout << YEL "  REQ  " RST
              << request.substr(0, request.find("\r\n")) << "\n";
    // status line
    if (code < 400) {
        std::cout << GRN "  RES  " RST;
    } else {
        std::cout << RED "  RES  " RST;
    }
    std::cout << RST << response.substr(0, response.find("\r\n")) << "\n";
    // body snippet
    std::string b = bodyOf(response);
    if (!b.empty())
        std::cout << "  BODY " << b.substr(0, 80)
                  << (b.size() > 80 ? "…" : "") << "\n";
    (void)ok;
}

// ═════════════════════════════════════════════════════════════════════════════
// Automated assertions
// ═════════════════════════════════════════════════════════════════════════════

struct Case { std::string label; std::string raw; int wantStatus; std::string wantBody; };

static void runCases(const std::vector<Case>& cases)
{
    for (auto& c : cases)
    {
        std::string resp = sendRaw(c.raw);
        printResult(c.label, c.raw, resp);

        int code = statusOf(resp);
        bool statusOk = (code == c.wantStatus);
        bool bodyOk   = c.wantBody.empty() ||
                        bodyOf(resp).find(c.wantBody) != std::string::npos;

        if (statusOk && bodyOk)
            PASS(c.label);
        else
        {
            if (!statusOk)
                std::cout << RED "    status: got " << code
                          << ", want " << c.wantStatus << RST "\n";
            if (!bodyOk)
                std::cout << RED "    body missing: " << c.wantBody << RST "\n";
            FAIL(c.label);
        }
        usleep(30'000);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    bool keepAlive = (argc > 1 && std::string(argv[1]) == "--keep-alive");

    setupWww();
    std::cout << BOLD "\n=== Live Full Test  localhost:" << PORT << " ===" RST "\n";
    std::cout << "www root: " << WWW << "\n\n";

    pid_t pid = forkServer();
    std::cout << MAG "[server] pid=" << pid << RST "\n";

    // ── test cases ────────────────────────────────────────────────────────────
    std::string postBody = "user=pettop&score=42";
    std::string postReq  =
        "POST /index.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: " + std::to_string(postBody.size()) + "\r\n"
        "\r\n" + postBody;

    runCases({
        // 200s
        { "GET /",
          "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n",
          200, "Welcome" },

        { "GET /about.html",
          "GET /about.html HTTP/1.1\r\nHost: localhost\r\n\r\n",
          200, "About" },

        { "GET /style.css",
          "GET /style.css HTTP/1.1\r\nHost: localhost\r\n\r\n",
          200, "monospace" },

        { "GET /api/data.json",
          "GET /api/data.json HTTP/1.1\r\nHost: localhost\r\n\r\n",
          200, "webserv" },

        { "GET /sub/  (directory index)",
          "GET /sub/ HTTP/1.1\r\nHost: localhost\r\n\r\n",
          200, "Sub index" },

        // 404
        { "GET /missing.html → 404",
          "GET /missing.html HTTP/1.1\r\nHost: localhost\r\n\r\n",
          404, "Not Found" },

        // 405
        { "PUT / → 405 (not in allowed methods)",
          "PUT / HTTP/1.1\r\nHost: localhost\r\n\r\n",
          405, "" },

        // 403 traversal
        { "GET /../etc/passwd → 403",
          "GET /../etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n",
          403, "" },

        // POST
        { "POST /index.html with body → 200",
          postReq,
          200, "" },
    });

    if (keepAlive)
    {
        std::cout << "\n" BOLD YEL
                  "[server] still alive on :" << PORT << "\n"
                  "  curl http://localhost:" << PORT << "/\n"
                  "  curl http://localhost:" << PORT << "/about.html\n"
                  "  curl http://localhost:" << PORT << "/api/data.json\n"
                  "  curl http://localhost:" << PORT << "/missing.html\n"
                  "  Press Enter to stop..." RST "\n";
        std::cin.get();
    }

    killServer(pid);
    fs::remove_all(WWW);

    std::cout << "\n" << (allPassed ? GRN BOLD "All tests PASSED." : RED BOLD "Some tests FAILED.")
              << RST "\n\n";
    return allPassed ? 0 : 1;
}