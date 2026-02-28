/*
** testServer.cpp
** ---------------
** Tests for Server — NO threads (Codam webserv rules).
**
** Strategy:
**   - Tests that only need init() run in the main process.
**   - Tests that need run() fork() a child for the server,
**     the parent acts as the client, then kills the child.
**
** Compile:
**   c++ -std=c++17 -Wall -Wextra tests/TestServer.cpp srcs/Server.cpp srcs/ConfigParser.cpp -o testServer
** Run:
**   ./testServer
*/

#include "../includes/Server.hpp"
#include "../includes/ConfigParser.hpp"
#include <iostream>
#include <cassert>
#include <cstring>      // strerror, strlen
#include <sys/socket.h>
#include <sys/wait.h>   // waitpid()
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>     // fork(), kill(), sleep()
#include <signal.h>     // SIGTERM
#include <cerrno>

// ─── result tracking ─────────────────────────────────────────────────────────

static bool allPassed = true;

#define PASS(name) { std::cout << "[PASS] " << (name) << "\n"; }
#define FAIL(name) { std::cout << "[FAIL] " << (name) << "\n"; allPassed = false; }

// ─── helper: make a config on a given port ───────────────────────────────────

static ServerConfig makeTestConfig(int port)
{
    ServerConfig cfg;
    cfg.host                 = "127.0.0.1";
    cfg.port                 = port;
    cfg.root                 = "./www";
    cfg.index                = "index.html";
    cfg.error_404            = "404.html";
    cfg.client_max_body_size = 1024;
    cfg.methods              = {"GET", "POST"};
    return cfg;
}

// ─── helper: connect a raw TCP socket to 127.0.0.1:port ─────────────────────
// Returns fd on success, -1 on failure.

static int connectTo(int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        close(sock);
        return -1;
    }
    return sock;
}

// ─── helper: fork a server child, return child pid ───────────────────────────
// Child calls srv.init() then srv.run() and never returns.
// Parent waits a moment for the socket to be ready, then continues.

static pid_t forkServer(int port)
{
    pid_t pid = fork();
    if (pid < 0)
        throw std::runtime_error("fork() failed");

    if (pid == 0)
    {
        // ── child: be the server ──
        Server srv(makeTestConfig(port));
        srv.init();
        srv.run();
        _exit(0); // never reached unless stop() is called
    }

    // ── parent: give the child a moment to bind + listen ──
    usleep(100'000); // 100 ms
    return pid;
}

// ─── helper: kill server child and reap it ───────────────────────────────────

static void killServer(pid_t pid)
{
    kill(pid, SIGTERM);
    int status;
    waitpid(pid, &status, 0);
}

// ═════════════════════════════════════════════════════════════════════════════
// Test 1 — init() binds successfully (no fork needed)
// ═════════════════════════════════════════════════════════════════════════════

static void test_init_binds()
{
    try
    {
        Server srv(makeTestConfig(9091));
        srv.init();
        if (srv.getFd() >= 0) {
            PASS("init() binds to port");
        }
    
        else {
            FAIL("init() binds to port");
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "  exception: " << e.what() << "\n";
        FAIL("init() binds to port");
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Test 2 — binding twice to the same port throws (no fork needed)
// ═════════════════════════════════════════════════════════════════════════════

static void test_double_bind_throws()
{
    try
    {
        Server s1(makeTestConfig(9092));
        s1.init();

        Server s2(makeTestConfig(9092));
        s2.init(); // must throw

        FAIL("double-bind throws");
    }
    catch (const std::exception&)
    {
        PASS("double-bind throws");
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Test 3 — server accepts a TCP connection (client just connects, no HTTP)
// ═════════════════════════════════════════════════════════════════════════════

static void test_accepts_connection()
{
    pid_t serverPid = forkServer(9093);

    int  sock = connectTo(9093);
    bool ok   = (sock >= 0);
    if (ok) close(sock);

    killServer(serverPid);

    if (ok)
    {
        PASS("server accepts TCP connection");
    }
    else {
        FAIL("server accepts TCP connection");
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Test 4 — server replies with HTTP/1.1 200 to a GET request
// ═════════════════════════════════════════════════════════════════════════════

static void test_http_200()
{
    pid_t serverPid = forkServer(9094);

    bool ok   = false;
    int  sock = connectTo(9094);
    if (sock >= 0)
    {
        const char* req = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
        write(sock, req, strlen(req));

        char    buf[4096] = {};
        ssize_t n         = read(sock, buf, sizeof(buf) - 1);
        if (n > 0)
        {
            std::string resp(buf, static_cast<size_t>(n));
            ok = (resp.find("HTTP/1.1 200") != std::string::npos);
        }
        close(sock);
    }

    killServer(serverPid);

    if (ok) { PASS("GET / returns HTTP/1.1 200");}
    else { FAIL("GET / returns HTTP/1.1 200"); }
}

// ═════════════════════════════════════════════════════════════════════════════
// Test 5 — response contains Content-Length header
// ═════════════════════════════════════════════════════════════════════════════

static void test_content_length_present()
{
    pid_t serverPid = forkServer(9095);

    bool ok   = false;
    int  sock = connectTo(9095);
    if (sock >= 0)
    {
        const char* req = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
        write(sock, req, strlen(req));

        char    buf[4096] = {};
        ssize_t n         = read(sock, buf, sizeof(buf) - 1);
        if (n > 0)
        {
            std::string resp(buf, static_cast<size_t>(n));
            ok = (resp.find("Content-Length:") != std::string::npos);
        }
        close(sock);
    }

    killServer(serverPid);

    if (ok) { PASS("response contains Content-Length"); }
    else { FAIL("response contains Content-Length"); }
}

// ═════════════════════════════════════════════════════════════════════════════
// Test 6 — server handles multiple sequential clients
// ═════════════════════════════════════════════════════════════════════════════

static void test_multiple_clients()
{
    pid_t serverPid = forkServer(9096);

    int       successCount = 0;
    const int N            = 3;

    for (int i = 0; i < N; ++i)
    {
        int sock = connectTo(9096);
        if (sock < 0) continue;

        const char* req = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
        write(sock, req, strlen(req));

        char    buf[4096] = {};
        ssize_t n         = read(sock, buf, sizeof(buf) - 1);
        if (n > 0)
        {
            std::string resp(buf, static_cast<size_t>(n));
            if (resp.find("HTTP/1.1 200") != std::string::npos)
                ++successCount;
        }
        close(sock);
        usleep(20'000); // 20 ms between clients — let poll() cycle
    }

    killServer(serverPid);

    if (successCount == N) {
        PASS("handles multiple sequential clients");
    }
    else {
        FAIL("handles multiple sequential clients");
    }
}

// ─────────────────────────────────────────────────────────────────────────────

int main()
{
    std::cout << "=== Server Tests (no threads) ===\n\n";

    test_init_binds();
    test_double_bind_throws();
    test_accepts_connection();
    test_http_200();
    test_content_length_present();
    test_multiple_clients();

    std::cout << "\n" << (allPassed ? "All tests PASSED." : "Some tests FAILED.") << "\n";
    return allPassed ? 0 : 1;
}