#!/usr/bin/env python3
"""
test_request_parsing.py — verifies the HTTP request parser rejects malformed
input and correctly handles edge cases.

Assumes webserv is already running on 127.0.0.1:8080 with tests/conf/default.conf.

Run from repo root:
    python3 tests/execution/test_request_parsing.py
"""

import http.client
import socket
import sys
from helpers import _check, require_server, finish

HOST = "127.0.0.1"
PORT = 8080


def _raw(raw_bytes, timeout=5):
    """Send exact bytes over TCP; return (status_code, reason) from status line."""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect((HOST, PORT))
    s.sendall(raw_bytes)
    data = b""
    try:
        while b"\r\n" not in data:
            chunk = s.recv(256)
            if not chunk:
                break
            data += chunk
    except socket.timeout:
        pass
    finally:
        s.close()
    first_line = data.split(b"\r\n")[0].decode(errors="replace")
    parts = first_line.split(" ", 2)
    if len(parts) < 3:
        return None, first_line
    return int(parts[1]), parts[2]


def _raw_full(raw_bytes, timeout=5):
    """Send exact bytes over TCP; return (status_code, body_str)."""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect((HOST, PORT))
    s.sendall(raw_bytes)
    data = b""
    try:
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk
    except socket.timeout:
        pass
    finally:
        s.close()
    if b"\r\n\r\n" not in data:
        return None, ""
    header_part, body = data.split(b"\r\n\r\n", 1)
    first_line = header_part.split(b"\r\n")[0].decode(errors="replace")
    parts = first_line.split(" ", 2)
    if len(parts) < 2:
        return None, ""
    return int(parts[1]), body.decode(errors="replace")


def _request(method, path, headers=None, body=None):
    c = http.client.HTTPConnection(HOST, PORT, timeout=5)
    c.request(method, path, body=body, headers=headers or {})
    r = c.getresponse()
    r.read()
    return r.status, r.reason, r


def _request_body(method, path, headers=None, body=None):
    """Like _request but returns the decoded response body for content checks."""
    c = http.client.HTTPConnection(HOST, PORT, timeout=5)
    c.request(method, path, body=body, headers=headers or {})
    r = c.getresponse()
    body = r.read().decode(errors="replace")
    return r.status, r.reason, body


def _request_body(method, path, headers=None, body=None):
    """Like _request but returns the response body instead of the response object."""
    c = http.client.HTTPConnection(HOST, PORT, timeout=5)
    c.request(method, path, body=body, headers=headers or {})
    r = c.getresponse()
    body = r.read().decode(errors="replace")
    return r.status, r.reason, body


# ── parser robustness ─────────────────────────────────────────────────────────

def test_double_host():
    # RFC 9112 §6.3: two Host headers = malformed request → 400
    code, _ = _raw(b"GET / HTTP/1.1\r\nHost: localhost\r\nHost: evil.com\r\n\r\n")
    _check("double Host header → 400", code, 400)


def test_negative_content_length():
    # -1 must not be cast to a huge size_t — server would then wait forever for body
    code, _ = _raw(b"POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: -1\r\n\r\nbody")
    _check("Content-Length: -1 → 400", code, 400)


def test_non_numeric_content_length():
    # atoi("abc") returns 0 silently — parser must validate before using the value
    code, _ = _raw(b"POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: abc\r\n\r\nbody")
    _check("Content-Length: abc → 400", code, 400)


def test_extra_spaces_in_request_line():
    # RFC 9112 §3: exactly one SP between method, target, version
    code, _ = _raw(b"GET  /  HTTP/1.1\r\nHost: localhost\r\n\r\n")
    _check("double spaces in request line → 400", code, 400)


def test_empty_request():
    # bare CRLF is not a valid request line — must not crash or hang
    code, _ = _raw(b"\r\n\r\n")
    _check("bare CRLF (empty request) → 400", code, 400)


def test_lf_only_line_endings():
    # \n without \r — RFC says MUST use \r\n; server may accept or reject,
    # but must not crash or hang — we just assert it responds
    code, _ = _raw(b"GET / HTTP/1.1\nHost: localhost\n\n")
    _check("LF-only line endings → server responds (not None)", code is not None, True)


def test_path_traversal():
    # /../ must never reach files outside root — expect 400 or 403, never 200
    code, _ = _raw(b"GET /../etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n")
    _check("path traversal /../ → not 200", code != 200, True)


def test_uri_encoded_traversal():
    # %2e%2e is URL-encoded ".." — must be decoded and resolved before root check
    code, _ = _raw(b"GET /%2e%2e/etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n")
    _check("URL-encoded traversal %2e%2e → not 200", code != 200, True)


def test_very_long_uri():
    # 9 KB URI — must not crash; RFC allows 414 or 400
    long_path = b"/" + b"a" * 9000
    code, _ = _raw(b"GET " + long_path + b" HTTP/1.1\r\nHost: localhost\r\n\r\n")
    _check("9000-byte URI → 400 or 414", code in (400, 414), True)


def test_absolute_form_uri():
    # RFC 9112 §3.2.2: servers MUST accept absolute-form URIs (used by proxies)
    code, _ = _raw(b"GET http://localhost:8080/ HTTP/1.1\r\nHost: localhost\r\n\r\n")
    _check("absolute-form URI → 200", code, 200)


def test_get_with_body_ignored():
    # GET body has no defined semantics — server must not reject the request
    code, _, _ = _request("GET", "/", headers={"Content-Length": "5"}, body=b"hello")
    _check("GET with body → 200 (body ignored)", code, 200)


def test_content_length_zero_body_ignored():
    # Content-Length: 0 means no body — extra bytes belong to the next request
    code, _ = _raw(b"GET / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\nhello")
    _check("Content-Length: 0 → 200 (extra bytes ignored)", code, 200)


# ── query string ──────────────────────────────────────────────────────────────

def test_query_string_cgi_params():
    # CGI script receives query string as QUERY_STRING; values appear in output
    code, _, body = _request_body("GET", "/cgi-bin/pythoncgitest.py?your_name=Alice&company_name=42")
    _check("CGI query string → 200",                code,             200)
    _check("CGI output contains your_name value",   "Alice" in body,  True)
    _check("CGI output contains company_name value","42" in body,     True)


def test_query_string_missing_params():
    # No query string — form.getvalue() returns None → script falls back to N/A
    code, _, body = _request_body("GET", "/cgi-bin/pythoncgitest.py")
    _check("CGI no query string → 200",   code,           200)
    _check("CGI missing params → N/A",    "N/A" in body,  True)


def test_query_string_empty_value():
    # ?key= (empty value) must not crash the parser or the CGI
    code, _, _ = _request_body("GET", "/cgi-bin/pythoncgitest.py?your_name=&company_name=")
    _check("query string empty values → 200", code, 200)


def test_absolute_form_uri_with_query():
    # Proxy sends absolute-form URI with query string — query must not be dropped
    # This is the bug fixed in _parsePath: http://host/path?q was losing ?q when
    # the authority had no trailing slash before the query.
    code, _, body = _request_body(
        "GET",
        "http://localhost:8080/cgi-bin/pythoncgitest.py?your_name=Proxy&company_name=Test"
    )
    _check("absolute-form URI with query → 200",        code,             200)
    _check("absolute-form URI: query string preserved", "Proxy" in body,  True)


TESTS = [
    test_double_host,
    test_negative_content_length,
    test_non_numeric_content_length,
    test_extra_spaces_in_request_line,
    test_empty_request,
    test_lf_only_line_endings,
    test_path_traversal,
    test_uri_encoded_traversal,
    test_very_long_uri,
    test_absolute_form_uri,
    test_get_with_body_ignored,
    test_content_length_zero_body_ignored,
    test_query_string_cgi_params,
    test_query_string_missing_params,
    test_query_string_empty_value,
    test_absolute_form_uri_with_query,
]

if __name__ == "__main__":
    require_server(HOST, PORT)
    print(f"Running request parsing edge-case tests against {HOST}:{PORT} …\n")
    for t in TESTS:
        t()
    finish()
