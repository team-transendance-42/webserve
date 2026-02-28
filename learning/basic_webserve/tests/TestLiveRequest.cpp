/*
** testLive.cpp
** ------------
** Fires real HTTP requests at localhost and prints the parsed result.
** The server runs in a fork()ed child; the parent is the test client.
**
** Compile:
**   c++ -std=c++17 -Wall -Wextra tests/TestLiveRequest.cpp srcs/HttpRequest.cpp srcs/Server.cpp srcs/ConfigParser.cpp -o testLive
** Run:
**   ./testLive
**
** Or keep the server alive and use curl / browser in parallel:
**   ./testLive --keep-alive
*/

#include "../includes/HttpRequest.hpp"
#include "../includes/Server.hpp"
#include "../includes/ConfigParser.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

// ─── colour codes ─────────────────────────────────────────────────────────────

#define GRN  "\033[32m"
#define RED  "\033[31m"
#define YEL  "\033[33m"
#define CYN  "\033[36m"
#define BOLD "\033[1m"
#define RST  "\033[0m"

static constexpr int    PORT    = 8080;
static constexpr size_t BUFSIZE = 8192;

// ─────────────────────────────────────────────────────────────────────────────
// Server child In programming, spawn means to create a new process or thread. spawnServer() starts a new server process using fork().
// ─────────────────────────────────────────────────────────────────────────────

static pid_t spawnServer()
{
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid == 0)
    {
        ServerConfig cfg;
        cfg.host                 = "127.0.0.1";
        cfg.port                 = PORT;
        cfg.root                 = "./www";
        cfg.index                = "index.html";
        cfg.error_404            = "404.html";
        cfg.client_max_body_size = 1024 * 1024;
        cfg.methods              = {"GET", "POST", "DELETE"};

        Server srv(cfg);
        srv.init();
        srv.run();
        _exit(0);
    }
    usleep(120'000); // let the child bind + listen
    return pid;
}

static void stopServer(pid_t pid)
{
    kill(pid, SIGTERM);
    int st;
    waitpid(pid, &st, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Raw TCP send → receive
// ─────────────────────────────────────────────────────────────────────────────

static std::string sendRaw(const std::string& raw)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "[socket() failed]";

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        close(sock);
        return "[connect() failed]";
    }

    write(sock, raw.c_str(), raw.size());

    std::string resp;
    char buf[BUFSIZE];
    ssize_t n;
    while ((n = read(sock, buf, sizeof(buf) - 1)) > 0)
    {
        buf[n] = '\0';
        resp += buf;
    }
    close(sock);
    return resp;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pretty printers
// ─────────────────────────────────────────────────────────────────────────────

static void printSeparator(const std::string& title)
{
    std::cout << "\n" << BOLD << CYN
              << "━━━  " << title << "  ━━━"
              << RST << "\n";
}

static void printRequest(const std::string& raw)
{
    std::cout << YEL << "▶ RAW REQUEST:\n" << RST;
    // print escaping \r\n visibly
    for (size_t i = 0; i < raw.size(); ++i)
    {
        if (raw[i] == '\r')      std::cout << "\\r";
        else if (raw[i] == '\n') std::cout << "\\n\n";
        else                     std::cout << raw[i];
    }
    std::cout << "\n";
}

static void printParsed(const std::string& raw)
{
    std::cout << GRN << "▶ PARSED HttpRequest:\n" << RST;
    try
    {
        HttpRequest req(raw);
        if (!req.isComplete())
        {
            std::cout << RED << "  [incomplete — no \\r\\n\\r\\n found]\n" << RST;
            return;
        }
        std::cout << "  method:  " << BOLD << req.method()  << RST << "\n"
                  << "  uri:     " << req.uri()     << "\n"
                  << "  path:    " << req.path()    << "\n"
                  << "  query:   " << (req.query().empty() ? "(none)" : req.query()) << "\n"
                  << "  version: " << req.version() << "\n"
                  << "  host:    " << req.header("host") << "\n";

        if (req.hasBody())
            std::cout << "  body:    " << BOLD << req.body() << RST << "\n"
                      << "  bodylen: " << req.contentLength() << "\n";
        else
            std::cout << "  body:    (none)\n";
    }
    catch (const HttpRequest::ParseError& e)
    {
        std::cout << RED << "  ParseError: " << e.what() << RST << "\n";
    }
}

static void printResponse(const std::string& resp)
{
    std::cout << CYN << "▶ SERVER RESPONSE:\n" << RST;
    if (resp.empty()) { std::cout << RED << "  (no response)\n" << RST; return; }

    // Split status line + headers from body
    size_t sep = resp.find("\r\n\r\n");
    std::string headers = (sep != std::string::npos) ? resp.substr(0, sep) : resp;
    std::string body    = (sep != std::string::npos) ? resp.substr(sep + 4) : "";

    std::cout << BOLD << "  " << headers.substr(0, headers.find("\r\n")) << RST << "\n";

    // print remaining headers
    size_t pos = headers.find("\r\n");
    if (pos != std::string::npos)
    {
        std::istringstream ss(headers.substr(pos + 2));
        std::string line;
        while (std::getline(ss, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) std::cout << "  " << line << "\n";
        }
    }

    if (!body.empty())
        std::cout << "\n  body: " << body.substr(0, 120)
                  << (body.size() > 120 ? "…" : "") << "\n";
}

static void runTest(const std::string& label, const std::string& raw)
{
    printSeparator(label);
    printRequest(raw);
    printParsed(raw);
    std::string resp = sendRaw(raw);
    printResponse(resp);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test cases
// ─────────────────────────────────────────────────────────────────────────────

static void runAllTests()
{
    // 1. Basic GET /
    runTest("GET /",
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n");

    // 2. GET with query string
    runTest("GET with query string",
        "GET /search?q=webserv&page=1 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Accept: text/html\r\n"
        "\r\n");

    // 3. GET deep path
    runTest("GET deep path",
        "GET /a/b/c/page.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n");

    // 4. POST with form body
    {
        std::string body = "username=codam&password=42network";
        std::string raw  =
            "POST /login HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n"
            "\r\n" + body;
        runTest("POST /login with body", raw);
    }

    // 5. DELETE
    runTest("DELETE /resource/42",
        "DELETE /resource/42 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n");

    // 6. Custom headers
    runTest("GET with custom headers",
        "GET /api/data HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Authorization: Bearer abc123\r\n"
        "X-Request-ID: deadbeef\r\n"
        "\r\n");

    // 7. Malformed — no \r\n\r\n (parser should not mark complete)
    printSeparator("EDGE: incomplete request (no \\r\\n\\r\\n)");
    {
        std::string raw = "GET / HTTP/1.1\r\nHost: localhost\r\n";
        printRequest(raw);
        printParsed(raw);  // shows [incomplete], no server send
        std::cout << CYN << "▶ SERVER RESPONSE:\n" << RST
                  << "  (not sent — request is incomplete)\n";
    }

    // 8. Malformed request line — ParseError expected
    printSeparator("EDGE: malformed request line");
    {
        std::string raw = "NOTAVERB\r\n\r\n";
        printRequest(raw);
        printParsed(raw);  // shows ParseError
        std::cout << CYN << "▶ SERVER RESPONSE:\n" << RST
                  << "  (not sent — parse error)\n";
    }

    // 9. Large custom header
    {
        std::string bigVal(2000, 'X');
        std::string raw =
            "GET /big HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "X-Huge: " + bigVal + "\r\n"
            "\r\n";
        runTest("GET with large header (2000 byte value)", raw);
    }

    // 10. POST with JSON body
    {
        std::string body = R"({"user":"alice","score":42})";
        std::string raw  =
            "POST /api/score HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n"
            "\r\n" + body;
        runTest("POST /api/score with JSON body", raw);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    bool keepAlive = (argc > 1 && std::string(argv[1]) == "--keep-alive");

    std::cout << BOLD << "\n=== Live HTTP Test — localhost:" << PORT << " ===\n" << RST;

    pid_t serverPid = spawnServer();
    std::cout << "[server] child pid=" << serverPid << "\n";

    runAllTests();

    if (keepAlive)
    {
        std::cout << "\n" << BOLD << YEL
                  << "[server] still running on :" << PORT
                  << " — open browser or run curl\n"
                  << "  curl http://localhost:" << PORT << "/\n"
                  << "  Press Enter to stop...\n" << RST;
        std::cin.get();
    }

    stopServer(serverPid);
    std::cout << BOLD << "\n[server] stopped.\n" << RST;
    return 0;
}