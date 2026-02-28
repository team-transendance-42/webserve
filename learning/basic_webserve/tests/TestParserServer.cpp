/*
** testParserServer.cpp
** --------------------
** Edge cases for ConfigParser + next step: raw HTTP request parsing.
**
** Compile:
**   c++ -std=c++17 -Wall -Wextra tests/testParserServer.cpp srcs/Server.cpp srcs/ConfigParser.cpp -o testParserServer
** Run:
**   ./testParserServer
*/

#include "../includes/ConfigParser.hpp"
#include "../includes/Server.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

// ─── result tracking ─────────────────────────────────────────────────────────

static bool allPassed = true;

#define PASS(name) do { std::cout << "[PASS] " << (name) << "\n"; } while(0)
#define FAIL(name) do { std::cout << "[FAIL] " << (name) << "\n"; allPassed = false; } while(0)

// ─── file helper: write a tmp config, return its path ────────────────────────

static std::string writeTmpConfig(const std::string& content)
{
    std::string path = "/tmp/test_webserv_tmp.conf";
    std::ofstream f(path);
    f << content;
    return path;
}

// ─── network helpers ─────────────────────────────────────────────────────────

static ServerConfig makeConfig(int port)
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

static pid_t forkServer(int port)
{
    pid_t pid = fork();
    if (pid < 0) throw std::runtime_error("fork() failed");
    if (pid == 0)
    {
        Server srv(makeConfig(port));
        srv.init();
        srv.run();
        _exit(0);
    }
    usleep(100'000);
    return pid;
}

static void killServer(pid_t pid)
{
    kill(pid, SIGTERM);
    int status;
    waitpid(pid, &status, 0);
}

static int connectTo(int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    { close(sock); return -1; }
    return sock;
}

// Send a raw request, read back the full response
static std::string sendRaw(int port, const std::string& raw)
{
    int sock = connectTo(port);
    if (sock < 0) return "";
    write(sock, raw.c_str(), raw.size());

    std::string resp;
    char buf[4096];
    ssize_t n;
    while ((n = read(sock, buf, sizeof(buf) - 1)) > 0)
    {
        buf[n] = '\0';
        resp += buf;
    }
    close(sock);
    return resp;
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 1 — ConfigParser edge cases
// ═════════════════════════════════════════════════════════════════════════════

// ── 1.1  missing file throws ──────────────────────────────────────────────────
static void test_config_missing_file()
{
    try
    {
        ConfigParser p;
        p.parse("/tmp/this_file_does_not_exist_42.conf");
        FAIL("missing file throws");
    }
    catch (const std::exception&) { PASS("missing file throws"); }
}

// ── 1.2  empty file → zero-value config, no crash ────────────────────────────
static void test_config_empty_file()
{
    std::string path = writeTmpConfig("");
    try
    {
        ConfigParser p;
        ServerConfig cfg = p.parse(path);
        // host is empty string, port is 0 — that's fine, parser must not crash
        if (cfg.port == 0 && cfg.host.empty())
            PASS("empty file yields zero config");
        else
            FAIL("empty file yields zero config");
    }
    catch (...) { FAIL("empty file yields zero config"); }
}

// ── 1.3  unknown keys are silently ignored ────────────────────────────────────
static void test_config_unknown_keys_ignored()
{
    std::string path = writeTmpConfig(
        "host=127.0.0.1\n"
        "port=8080\n"
        "dragon=fire\n"      // unknown
        "root=/var/www\n"
    );
    try
    {
        ConfigParser p;
        ServerConfig cfg = p.parse(path);
        if (cfg.host == "127.0.0.1" && cfg.port == 8080 && cfg.root == "/var/www")
            PASS("unknown keys ignored");
        else
            FAIL("unknown keys ignored");
    }
    catch (...) { FAIL("unknown keys ignored"); }
}

// ── 1.4  duplicate key → last value wins ─────────────────────────────────────
static void test_config_duplicate_key_last_wins()
{
    std::string path = writeTmpConfig(
        "port=1111\n"
        "port=2222\n"
    );
    ConfigParser p;
    ServerConfig cfg = p.parse(path);
    if (cfg.port == 2222)
        PASS("duplicate key: last value wins");
    else
        FAIL("duplicate key: last value wins");
}

// ── 1.5  methods with one entry (no comma) ────────────────────────────────────
static void test_config_single_method()
{
    std::string path = writeTmpConfig("methods=GET\n");
    ConfigParser p;
    ServerConfig cfg = p.parse(path);
    if (cfg.methods.size() == 1 && cfg.methods[0] == "GET")
        PASS("single method parsed");
    else
        FAIL("single method parsed");
}

// ── 1.6  methods with trailing comma ─────────────────────────────────────────
static void test_config_trailing_comma_methods()
{
    std::string path = writeTmpConfig("methods=GET,POST,\n");
    ConfigParser p;
    ServerConfig cfg = p.parse(path);
    // trailing comma produces an empty string token — parser should handle it
    // acceptable: either 2 real methods or 3 with last being empty
    bool ok = (cfg.methods.size() >= 2 &&
               cfg.methods[0] == "GET" &&
               cfg.methods[1] == "POST");
    if (ok) PASS("trailing comma in methods");
    else    FAIL("trailing comma in methods");
}

// ── 1.7  value with '=' inside (e.g. a URL as a value) ───────────────────────
static void test_config_value_contains_equals()
{
    // "root=/var/www=test" → key="root", value="/var/www=test"
    // getline(ss, value) reads the REST of the line after first '='
    std::string path = writeTmpConfig("root=/var/www=test\n");
    ConfigParser p;
    ServerConfig cfg = p.parse(path);
    if (cfg.root == "/var/www=test")
        PASS("value containing '=' parsed correctly");
    else
        FAIL("value containing '=' parsed correctly");
}

// ── 1.8  line without '=' is skipped, no crash ───────────────────────────────
static void test_config_line_without_equals()
{
    std::string path = writeTmpConfig(
        "# this is a comment\n"
        "port=9000\n"
        "this line has no equals sign\n"
        "host=localhost\n"
    );
    try
    {
        ConfigParser p;
        ServerConfig cfg = p.parse(path);
        if (cfg.port == 9000 && cfg.host == "localhost")
            PASS("line without '=' skipped");
        else
            FAIL("line without '=' skipped");
    }
    catch (...) { FAIL("line without '=' skipped"); }
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 2 — Next step: HTTP request edge cases over the wire
// These tests validate that the Server correctly handles malformed or unusual
// HTTP requests — groundwork for your HttpRequest parser class.
// ═════════════════════════════════════════════════════════════════════════════

// ── 2.1  request missing \r\n\r\n — server must not reply until it arrives ───
//   We send an incomplete request, then complete it, and expect a 200.
static void test_http_incomplete_then_complete()
{
    pid_t pid = forkServer(9201);

    bool ok = false;
    int sock = connectTo(9201);
    if (sock >= 0)
    {
        // send headers in two parts with a pause
        const char* part1 = "GET / HTTP/1.1\r\nHost: localhost\r\n";
        write(sock, part1, strlen(part1));
        usleep(50'000); // 50 ms — server should not have replied yet

        const char* part2 = "\r\n"; // completes the header block
        write(sock, part2, strlen(part2));

        char buf[4096] = {};
        ssize_t n = read(sock, buf, sizeof(buf) - 1);
        if (n > 0)
            ok = (std::string(buf, n).find("HTTP/1.1 200") != std::string::npos);
        close(sock);
    }

    killServer(pid);
    if (ok) PASS("incomplete request buffered, completes on second write");
    else    FAIL("incomplete request buffered, completes on second write");
}

// ── 2.2  empty request (immediate close) — server must not crash ──────────────
static void test_http_empty_request()
{
    pid_t pid = forkServer(9202);

    bool ok = false;
    int sock = connectTo(9202);
    if (sock >= 0)
    {
        close(sock); // close immediately without sending anything
        ok = true;   // if we get here the server didn't crash
    }
    usleep(50'000); // give server time to handle the EOF

    // verify server still accepts a new connection after the empty client
    sock = connectTo(9202);
    if (sock >= 0)
    {
        const char* req = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
        write(sock, req, strlen(req));
        char buf[4096] = {};
        ssize_t n = read(sock, buf, sizeof(buf) - 1);
        ok = (n > 0 && std::string(buf, n).find("HTTP/1.1") != std::string::npos);
        close(sock);
    }

    killServer(pid);
    if (ok) PASS("empty request (immediate close) doesn't crash server");
    else    FAIL("empty request (immediate close) doesn't crash server");
}

// ── 2.3  very large header (exceeds READ_BUF in one shot) ────────────────────
static void test_http_large_header()
{
    pid_t pid = forkServer(9203);

    bool ok = false;
    int sock = connectTo(9203);
    if (sock >= 0)
    {
        // Build a header with a very long custom field
        std::string req = "GET / HTTP/1.1\r\nHost: localhost\r\n";
        req += "X-Big: " + std::string(8000, 'A') + "\r\n";
        req += "\r\n";

        write(sock, req.c_str(), req.size());

        char buf[4096] = {};
        ssize_t n = read(sock, buf, sizeof(buf) - 1);
        // We just want the server to respond somehow without crashing
        ok = (n > 0);
        close(sock);
    }

    killServer(pid);
    if (ok) PASS("large header handled without crash");
    else    FAIL("large header handled without crash");
}

// ── 2.4  POST request with body ───────────────────────────────────────────────
//   Server doesn't parse body yet, but must not hang or crash.
static void test_http_post_with_body()
{
    pid_t pid = forkServer(9204);

    bool ok = false;
    int sock = connectTo(9204);
    if (sock >= 0)
    {
        std::string body = "name=Alice&age=30";
        std::ostringstream req;
        req << "POST /submit HTTP/1.1\r\n"
            << "Host: localhost\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Content-Type: application/x-www-form-urlencoded\r\n"
            << "\r\n"
            << body;

        std::string r = req.str();
        write(sock, r.c_str(), r.size());

        char buf[4096] = {};
        ssize_t n = read(sock, buf, sizeof(buf) - 1);
        ok = (n > 0);
        close(sock);
    }

    killServer(pid);
    if (ok) PASS("POST with body: server responds without hanging");
    else    FAIL("POST with body: server responds without hanging");
}

// ── 2.5  multiple requests on same connection (keep-alive pattern) ────────────
//   Current server closes after one response — second request must get
//   a fresh connection. Validates poll() keeps cycling after client removal.
static void test_http_second_request_new_connection()
{
    pid_t pid = forkServer(9205);

    int ok = 0;
    for (int i = 0; i < 2; ++i)
    {
        std::string resp = sendRaw(9205, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
        if (resp.find("HTTP/1.1 200") != std::string::npos)
            ++ok;
    }

    killServer(pid);
    if (ok == 2) PASS("two sequential connections both get 200");
    else         FAIL("two sequential connections both get 200");
}

// ─────────────────────────────────────────────────────────────────────────────

int main()
{
    std::cout << "=== ConfigParser edge cases ===\n\n";

    test_config_missing_file();
    test_config_empty_file();
    test_config_unknown_keys_ignored();
    test_config_duplicate_key_last_wins();
    test_config_single_method();
    test_config_trailing_comma_methods();
    test_config_value_contains_equals();
    test_config_line_without_equals();

    std::cout << "\n=== HTTP request edge cases (next step groundwork) ===\n\n";

    test_http_incomplete_then_complete();
    test_http_empty_request();
    test_http_large_header();
    test_http_post_with_body();
    test_http_second_request_new_connection();

    std::cout << "\n" << (allPassed ? "All tests PASSED." : "Some tests FAILED.") << "\n";
    return allPassed ? 0 : 1;
}