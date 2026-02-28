/*
** testHttpRequest.cpp
** -------------------
** Pure unit tests for HttpRequest — no network, no fork needed.
** Parsing is deterministic: feed a string, check the result.
**
** Compile:
**   c++ -std=c++17 -Wall -Wextra tests/TestHttpRequest.cpp srcs/HttpRequest.cpp -o testHttpRequest
** Run:
**   ./testHttpRequest
*/

#include "../includes/HttpRequest.hpp"
#include <iostream>
#include <cassert>

static bool allPassed = true;

#define PASS(name) do { std::cout << "[PASS] " << (name) << "\n"; } while(0)
#define FAIL(name) do { std::cout << "[FAIL] " << (name) << "\n"; allPassed = false; } while(0)

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Build a complete GET request string
static std::string makeGet(const std::string& path,
                           const std::string& extraHeaders = "")
{
    return "GET " + path + " HTTP/1.1\r\n"
           "Host: localhost\r\n"
           + extraHeaders +
           "\r\n";
}

// Build a complete POST request string with body
static std::string makePost(const std::string& path,
                            const std::string& body,
                            const std::string& contentType = "application/x-www-form-urlencoded")
{
    return "POST " + path + " HTTP/1.1\r\n"
           "Host: localhost\r\n"
           "Content-Type: " + contentType + "\r\n"
           "Content-Length: " + std::to_string(body.size()) + "\r\n"
           "\r\n" + body;
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 1 — Basic parsing
// ═════════════════════════════════════════════════════════════════════════════

static void test_basic_get()
{
    HttpRequest r(makeGet("/"));
    if (r.isComplete() && r.method() == "GET" && r.uri() == "/" && r.version() == "HTTP/1.1")
        PASS("basic GET parsed");
    else
        FAIL("basic GET parsed");
}

static void test_method_post()
{
    HttpRequest r(makePost("/submit", "a=1&b=2"));
    if (r.isComplete() && r.method() == "POST")
        PASS("POST method parsed");
    else
        FAIL("POST method parsed");
}

static void test_uri_path_only()
{
    HttpRequest r(makeGet("/about"));
    if (r.path() == "/about" && r.query().empty())
        PASS("URI path without query");
    else
        FAIL("URI path without query");
}

static void test_uri_with_query()
{
    HttpRequest r(makeGet("/search?q=hello&page=2"));
    if (r.path() == "/search" && r.query() == "q=hello&page=2")
        PASS("URI path + query string split");
    else
        FAIL("URI path + query string split");
}

static void test_header_lookup()
{
    HttpRequest r(makeGet("/", "X-Custom: myvalue\r\n"));
    if (r.header("x-custom") == "myvalue")
        PASS("header lookup (lowercase)");
    else
        FAIL("header lookup (lowercase)");
}

static void test_header_case_insensitive()
{
    HttpRequest r(makeGet("/", "Content-Type: text/html\r\n"));
    // look up with different casing
    if (r.header("CONTENT-TYPE") == "text/html" &&
        r.header("content-type") == "text/html")
        PASS("header lookup is case-insensitive");
    else
        FAIL("header lookup is case-insensitive");
}

static void test_missing_header_returns_empty()
{
    HttpRequest r(makeGet("/"));
    if (r.header("x-does-not-exist").empty())
        PASS("missing header returns empty string");
    else
        FAIL("missing header returns empty string");
}

static void test_host_header()
{
    HttpRequest r(makeGet("/"));
    if (r.header("host") == "localhost")
        PASS("Host header parsed");
    else
        FAIL("Host header parsed");
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 2 — Body handling
// ═════════════════════════════════════════════════════════════════════════════

static void test_post_body()
{
    std::string body = "name=Alice&city=Amsterdam";
    HttpRequest r(makePost("/form", body));
    if (r.body() == body && r.contentLength() == body.size())
        PASS("POST body correctly extracted");
    else
        FAIL("POST body correctly extracted");
}

static void test_get_has_no_body()
{
    HttpRequest r(makeGet("/"));
    if (!r.hasBody() && r.body().empty())
        PASS("GET has no body");
    else
        FAIL("GET has no body");
}

static void test_body_exact_content_length()
{
    // Content-Length says 5, body has 10 — should only read 5
    std::string raw =
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "HelloEXTRADATA";
    HttpRequest r(raw);
    if (r.isComplete() && r.body() == "Hello")
        PASS("body truncated to Content-Length");
    else
        FAIL("body truncated to Content-Length");
}

static void test_zero_content_length()
{
    std::string raw =
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    HttpRequest r(raw);
    if (r.isComplete() && r.body().empty())
        PASS("Content-Length: 0 gives empty body");
    else
        FAIL("Content-Length: 0 gives empty body");
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 3 — Incremental feed (simulates split TCP reads)
// ═════════════════════════════════════════════════════════════════════════════

static void test_feed_split_at_headers()
{
    // Feed headers in two chunks, split right in the middle of a header line
    std::string full = makeGet("/page", "Accept: text/html\r\n");
    size_t mid = full.size() / 2;

    HttpRequest r;
    r.feed(full.substr(0, mid));
    bool notYet = !r.isComplete();
    r.feed(full.substr(mid));

    if (notYet && r.isComplete() && r.method() == "GET")
        PASS("incremental feed: split mid-headers");
    else
        FAIL("incremental feed: split mid-headers");
}

static void test_feed_split_at_separator()
{
    // Split exactly at the \r\n\r\n boundary
    std::string full = makeGet("/");
    // full ends with \r\n\r\n — send everything except last 2 chars, then rest
    HttpRequest r;
    r.feed(full.substr(0, full.size() - 2));
    bool notYet = !r.isComplete();
    r.feed(full.substr(full.size() - 2));

    if (notYet && r.isComplete())
        PASS("incremental feed: split at \\r\\n\\r\\n boundary");
    else
        FAIL("incremental feed: split at \\r\\n\\r\\n boundary");
}

static void test_feed_body_arrives_late()
{
    // POST: headers arrive first, body comes in second feed
    std::string body = "payload=xyz";
    std::string headers =
        "POST /up HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n";

    HttpRequest r;
    r.feed(headers);
    bool headersNotComplete = !r.isComplete(); // body not yet
    r.feed(body);

    if (headersNotComplete && r.isComplete() && r.body() == body)
        PASS("incremental feed: body arrives in second chunk");
    else
        FAIL("incremental feed: body arrives in second chunk");
}

static void test_feed_one_byte_at_a_time()
{
    std::string full = makeGet("/slow");
    HttpRequest r;
    for (char c : full)
        r.feed(std::string(1, c));

    if (r.isComplete() && r.path() == "/slow")
        PASS("incremental feed: one byte at a time");
    else
        FAIL("incremental feed: one byte at a time");
}

// ═════════════════════════════════════════════════════════════════════════════
// SECTION 4 — Edge cases
// ═════════════════════════════════════════════════════════════════════════════

static void test_malformed_request_line_throws()
{
    try
    {
        HttpRequest r("BADLINE\r\n\r\n");
        FAIL("malformed request line throws ParseError");
    }
    catch (const HttpRequest::ParseError&) { PASS("malformed request line throws ParseError"); }
    catch (...) { FAIL("malformed request line throws ParseError"); }
}

static void test_unsupported_method_throws()
{
    try
    {
        HttpRequest r("BREW /coffee HTTP/1.1\r\nHost: localhost\r\n\r\n");
        FAIL("unsupported method throws ParseError");
    }
    catch (const HttpRequest::ParseError&) { PASS("unsupported method throws ParseError"); }
    catch (...) { FAIL("unsupported method throws ParseError"); }
}

static void test_empty_raw_not_complete()
{
    HttpRequest r("");
    if (!r.isComplete())
        PASS("empty raw string not marked complete");
    else
        FAIL("empty raw string not marked complete");
}

static void test_header_with_colon_in_value()
{
    // "Authorization: Basic dXNlcjpwYXNz" — value contains ':'
    HttpRequest r(makeGet("/", "Authorization: Basic dXNlcjpwYXNz\r\n"));
    if (r.header("authorization") == "Basic dXNlcjpwYXNz")
        PASS("header value containing ':' parsed correctly");
    else
        FAIL("header value containing ':' parsed correctly");
}

static void test_uri_root_slash()
{
    HttpRequest r(makeGet("/"));
    if (r.path() == "/" && r.query().empty())
        PASS("URI '/' parsed correctly");
    else
        FAIL("URI '/' parsed correctly");
}

static void test_uri_deep_path()
{
    HttpRequest r(makeGet("/a/b/c/d.html"));
    if (r.path() == "/a/b/c/d.html")
        PASS("deep URI path parsed correctly");
    else
        FAIL("deep URI path parsed correctly");
}

static void test_feed_after_complete_ignored()
{
    HttpRequest r(makeGet("/"));
    std::string before = r.body();
    r.feed("GARBAGE DATA AFTER COMPLETE\r\n");
    if (r.body() == before)
        PASS("feed after complete is ignored");
    else
        FAIL("feed after complete is ignored");
}

static void test_delete_method()
{
    HttpRequest r("DELETE /resource/42 HTTP/1.1\r\nHost: localhost\r\n\r\n");
    if (r.isComplete() && r.method() == "DELETE" && r.path() == "/resource/42")
        PASS("DELETE method parsed");
    else
        FAIL("DELETE method parsed");
}

// ─────────────────────────────────────────────────────────────────────────────

int main()
{
    std::cout << "=== HttpRequest Tests ===\n\n";

    std::cout << "-- Basic parsing --\n";
    test_basic_get();
    test_method_post();
    test_uri_path_only();
    test_uri_with_query();
    test_header_lookup();
    test_header_case_insensitive();
    test_missing_header_returns_empty();
    test_host_header();

    std::cout << "\n-- Body handling --\n";
    test_post_body();
    test_get_has_no_body();
    test_body_exact_content_length();
    test_zero_content_length();

    std::cout << "\n-- Incremental feed --\n";
    test_feed_split_at_headers();
    test_feed_split_at_separator();
    test_feed_body_arrives_late();
    test_feed_one_byte_at_a_time();

    std::cout << "\n-- Edge cases --\n";
    test_malformed_request_line_throws();
    test_unsupported_method_throws();
    test_empty_raw_not_complete();
    test_header_with_colon_in_value();
    test_uri_root_slash();
    test_uri_deep_path();
    test_feed_after_complete_ignored();
    test_delete_method();

    std::cout << "\n" << (allPassed ? "All tests PASSED." : "Some tests FAILED.") << "\n";
    return allPassed ? 0 : 1;
}