/*
** testHttpResponse.cpp
** --------------------
** Unit tests for HttpResponse.
** Creates real temp files under /tmp/wr_test_root/ so _serveFile()
** has actual content to read — no network, no fork.
**
** Compile:
**   c++ -std=c++17 -Wall -Wextra tests/testHttpResponse.cpp srcs/HttpResponse.cpp srcs/HttpRequest.cpp srcs/ConfigParser.cpp -o testHttpResponse
** Run:
**   ./testHttpResponse
*/

#include "../includes/HttpResponse.hpp"
#include "../includes/HttpRequest.hpp"
#include "../includes/ConfigParser.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <cassert>

namespace fs = std::filesystem;

// ─── helpers ─────────────────────────────────────────────────────────────────

static bool allPassed = true;

#define PASS(name) do { std::cout << "[PASS] " << (name) << "\n"; } while(0)
#define FAIL(name) do { std::cout << "[FAIL] " << (name) << "\n"; allPassed = false; } while(0)

static const std::string ROOT = "/tmp/wr_test_root";

// Write a file relative to ROOT
static void writeFile(const std::string& relPath, const std::string& content)
{
    fs::path p = fs::path(ROOT) / relPath;
    fs::create_directories(p.parent_path());
    std::ofstream f(p);
    f << content;
}

// Build a ServerConfig pointing at ROOT
static ServerConfig makeConfig()
{
    ServerConfig cfg;
    cfg.host                 = "127.0.0.1";
    cfg.port                 = 8080;
    cfg.root                 = ROOT;
    cfg.index                = "index.html";
    cfg.error_404            = "404.html";
    cfg.client_max_body_size = 1024;
    cfg.methods              = {"GET", "POST", "DELETE"};
    return cfg;
}

// Build a complete GET request
static HttpRequest makeGet(const std::string& path)
{
    return HttpRequest(
        "GET " + path + " HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n"
    );
}

// Build a complete POST request
static HttpRequest makePost(const std::string& path, const std::string& body = "")
{
    return HttpRequest(
        "POST " + path + " HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n" + body
    );
}

// ─── setup ───────────────────────────────────────────────────────────────────

static void setup()
{
    fs::remove_all(ROOT);
    fs::create_directories(ROOT);

    writeFile("index.html",      "<html><body>Home</body></html>");
    writeFile("about.html",      "<html><body>About</body></html>");
    writeFile("style.css",       "body { margin: 0; }");
    writeFile("data.json",       "{\"ok\":true}");
    writeFile("img/logo.png",    "\x89PNG\r\n"); // fake PNG header
    writeFile("404.html",        "<html><body>Custom 404</body></html>");
    writeFile("sub/page.html",   "<html><body>Sub</body></html>");
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 1 — Status codes
// ═════════════════════════════════════════════════════════════════════════════

static void test_200_existing_file()
{
    auto cfg = makeConfig();
    HttpResponse res(makeGet("/about.html"), cfg);
    res.build();
    if (res.status() == 200)
        PASS("200 for existing file");
    else
        FAIL("200 for existing file");
}

static void test_200_index_on_root()
{
    // GET / → should resolve to index.html
    auto cfg = makeConfig();
    HttpResponse res(makeGet("/"), cfg);
    res.build();
    if (res.status() == 200 && res.body().find("Home") != std::string::npos)
        PASS("200 for / resolves to index.html");
    else
        FAIL("200 for / resolves to index.html");
}

static void test_404_missing_file()
{
    auto cfg = makeConfig();
    HttpResponse res(makeGet("/does_not_exist.html"), cfg);
    res.build();
    if (res.status() == 404)
        PASS("404 for missing file");
    else
        FAIL("404 for missing file");
}

static void test_404_uses_custom_error_page()
{
    auto cfg = makeConfig();
    HttpResponse res(makeGet("/ghost.html"), cfg);
    res.build();
    // body should contain our custom 404.html content
    if (res.status() == 404 && res.body().find("Custom 404") != std::string::npos)
        PASS("404 serves custom error page");
    else
        FAIL("404 serves custom error page");
}

static void test_404_fallback_when_no_error_page()
{
    auto cfg    = makeConfig();
    cfg.error_404 = ""; // no custom page
    HttpResponse res(makeGet("/gone.html"), cfg);
    res.build();
    if (res.status() == 404 && !res.body().empty())
        PASS("404 fallback built-in HTML when no error page configured");
    else
        FAIL("404 fallback built-in HTML when no error page configured");
}

static void test_405_method_not_allowed()
{
    auto cfg = makeConfig();
    cfg.methods = {"GET"}; // only GET allowed

    HttpResponse res(makePost("/index.html"), cfg);
    res.build();
    if (res.status() == 405)
        PASS("405 for disallowed method");
    else
        FAIL("405 for disallowed method");
}

static void test_403_directory_traversal()
{
    auto cfg = makeConfig();
    // Try to escape root
    HttpResponse res(makeGet("/../etc/passwd"), cfg);
    res.build();
    if (res.status() == 403)
        PASS("403 for directory traversal attempt");
    else
        FAIL("403 for directory traversal attempt");
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 2 — Content-Type
// ═════════════════════════════════════════════════════════════════════════════

static void test_content_type_html()
{
    auto cfg = makeConfig();
    HttpResponse res(makeGet("/about.html"), cfg);
    res.build();
    if (res.header("Content-Type") == "text/html")
        PASS("Content-Type: text/html for .html");
    else
        FAIL("Content-Type: text/html for .html");
}

static void test_content_type_css()
{
    auto cfg = makeConfig();
    HttpResponse res(makeGet("/style.css"), cfg);
    res.build();
    if (res.header("Content-Type") == "text/css")
        PASS("Content-Type: text/css for .css");
    else
        FAIL("Content-Type: text/css for .css");
}

static void test_content_type_json()
{
    auto cfg = makeConfig();
    HttpResponse res(makeGet("/data.json"), cfg);
    res.build();
    if (res.header("Content-Type") == "application/json")
        PASS("Content-Type: application/json for .json");
    else
        FAIL("Content-Type: application/json for .json");
}

static void test_content_type_png()
{
    auto cfg = makeConfig();
    HttpResponse res(makeGet("/img/logo.png"), cfg);
    res.build();
    if (res.header("Content-Type") == "image/png")
        PASS("Content-Type: image/png for .png");
    else
        FAIL("Content-Type: image/png for .png");
}

static void test_content_type_unknown_extension()
{
    writeFile("data.xyz", "binary stuff");
    auto cfg = makeConfig();
    HttpResponse res(makeGet("/data.xyz"), cfg);
    res.build();
    if (res.header("Content-Type") == "application/octet-stream")
        PASS("Content-Type: application/octet-stream for unknown extension");
    else
        FAIL("Content-Type: application/octet-stream for unknown extension");
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 3 — Body and headers
// ═════════════════════════════════════════════════════════════════════════════

static void test_body_matches_file_content()
{
    auto cfg = makeConfig();
    HttpResponse res(makeGet("/about.html"), cfg);
    res.build();
    if (res.body() == "<html><body>About</body></html>")
        PASS("body matches file content exactly");
    else
        FAIL("body matches file content exactly");
}

static void test_content_length_matches_body()
{
    auto cfg = makeConfig();
    HttpResponse res(makeGet("/about.html"), cfg);
    res.build();
    size_t reported = std::stoul(res.header("Content-Length"));
    if (reported == res.body().size())
        PASS("Content-Length matches actual body size");
    else
        FAIL("Content-Length matches actual body size");
}

static void test_raw_starts_with_status_line()
{
    auto cfg = makeConfig();
    HttpResponse res(makeGet("/about.html"), cfg);
    std::string raw = res.build();
    if (raw.rfind("HTTP/1.1 200 OK\r\n", 0) == 0)
        PASS("raw response starts with status line");
    else
        FAIL("raw response starts with status line");
}

static void test_raw_contains_separator()
{
    auto cfg = makeConfig();
    HttpResponse res(makeGet("/about.html"), cfg);
    std::string raw = res.build();
    if (raw.find("\r\n\r\n") != std::string::npos)
        PASS("raw response contains \\r\\n\\r\\n header separator");
    else
        FAIL("raw response contains \\r\\n\\r\\n header separator");
}

static void test_connection_close_header()
{
    auto cfg = makeConfig();
    HttpResponse res(makeGet("/about.html"), cfg);
    std::string raw = res.build();
    if (raw.find("Connection: close") != std::string::npos)
        PASS("Connection: close present in response");
    else
        FAIL("Connection: close present in response");
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 4 — Edge cases
// ═════════════════════════════════════════════════════════════════════════════

static void test_subdirectory_file()
{
    auto cfg = makeConfig();
    HttpResponse res(makeGet("/sub/page.html"), cfg);
    res.build();
    if (res.status() == 200 && res.body().find("Sub") != std::string::npos)
        PASS("file in subdirectory served correctly");
    else
        FAIL("file in subdirectory served correctly");
}

static void test_post_allowed_method()
{
    auto cfg = makeConfig(); // methods includes POST
    HttpResponse res(makePost("/index.html", "data=123"), cfg);
    res.build();
    // POST is allowed — file exists — expect 200
    if (res.status() == 200)
        PASS("POST to existing path returns 200 when method allowed");
    else
        FAIL("POST to existing path returns 200 when method allowed");
}

static void test_delete_method()
{
    auto cfg = makeConfig();
    HttpRequest req("DELETE /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n");
    HttpResponse res(req, cfg);
    res.build();
    // DELETE allowed in config → file exists → 200
    if (res.status() == 200)
        PASS("DELETE to existing path returns 200 when method allowed");
    else
        FAIL("DELETE to existing path returns 200 when method allowed");
}

static void test_double_slash_path()
{
    // //index.html → should still resolve
    auto cfg = makeConfig();
    HttpResponse res(makeGet("//index.html"), cfg);
    res.build();
    // acceptable: 200 (resolved) or 404, but must NOT crash
    bool noCrash = (res.status() == 200 || res.status() == 404);
    if (noCrash)
        PASS("double slash path handled without crash");
    else
        FAIL("double slash path handled without crash");
}

// ─────────────────────────────────────────────────────────────────────────────

int main()
{
    setup();

    std::cout << "=== HttpResponse Tests ===\n\n";

    std::cout << "-- Status codes --\n";
    test_200_existing_file();
    test_200_index_on_root();
    test_404_missing_file();
    test_404_uses_custom_error_page();
    test_404_fallback_when_no_error_page();
    test_405_method_not_allowed();
    test_403_directory_traversal();

    std::cout << "\n-- Content-Type --\n";
    test_content_type_html();
    test_content_type_css();
    test_content_type_json();
    test_content_type_png();
    test_content_type_unknown_extension();

    std::cout << "\n-- Body and headers --\n";
    test_body_matches_file_content();
    test_content_length_matches_body();
    test_raw_starts_with_status_line();
    test_raw_contains_separator();
    test_connection_close_header();

    std::cout << "\n-- Edge cases --\n";
    test_subdirectory_file();
    test_post_allowed_method();
    test_delete_method();
    test_double_slash_path();

    std::cout << "\n" << (allPassed ? "All tests PASSED." : "Some tests FAILED.") << "\n";

    // cleanup
    fs::remove_all(ROOT);
    return allPassed ? 0 : 1;
}