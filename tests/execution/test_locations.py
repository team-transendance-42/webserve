#!/usr/bin/env python3
"""
test_locations.py — verifies routing, redirects, and body-size limits.

Assumes webserv is already running on 127.0.0.1 with tests/conf/default.conf
(servers "one" :8080, "api" :8081, "static" :8082).

Run from repo root:
    python3 tests/execution/test_locations.py
"""

import http.client
import sys
from helpers import _check, require_server, finish

HOST = "127.0.0.1"
PORT_ONE = 8080
PORT_API = 8081
PORT_STATIC = 8082


def _req(method, path, port=PORT_ONE, body=None, headers=None):
    c = http.client.HTTPConnection(HOST, port, timeout=10)
    c.request(method, path, body=body, headers=headers or {})
    r = c.getresponse()
    r.read()
    return r.status


def _req_full(method, path, port=PORT_ONE, body=None, headers=None):
    c = http.client.HTTPConnection(HOST, port, timeout=10)
    c.request(method, path, body=body, headers=headers or {})
    r = c.getresponse()
    data = r.read()
    return r, data


# ── tests ─────────────────────────────────────────────────────────────────────

LOCATION_CASES = [
    ("GET",    "/",               PORT_ONE, 200, "root index"),
    ("GET",    "/zombie_kittens", PORT_ONE, 200, "specific page"),
    ("GET",    "/game_start",     PORT_ONE, 200, "specific page"),
    ("GET",    "/play",           PORT_ONE, 301, "redirect"),
    ("GET",    "/secret",         PORT_ONE, 403, "denyAll"),
    ("GET",    "/not_allowed",    PORT_ONE, 200, "page served"),
    ("GET",    "/files",          PORT_ONE, 301, "autoindex off, bare path → trailing slash"),
    ("GET",    "/files-auto",     PORT_ONE, 301, "autoindex on, bare path → trailing slash"),
    ("GET",    "/api/data_json",  PORT_ONE, 200, "json endpoint"),
    ("GET",    "/does_not_exist", PORT_ONE, 404, "missing resource"),
    ("POST",   "/zombie_kittens", PORT_ONE, 405, "method not allowed"),
    ("DELETE", "/api/data_json",  PORT_ONE, 405, "method not allowed"),
    ("POST",   "/play",           PORT_ONE, 405, "redirect disallows POST"),

    # server "api" — :8081 (location / and /health only — no /api prefix)
    ("GET",    "/",        PORT_API, 200, "api root: data.json"),
    ("GET",    "/health",  PORT_API, 200, "health-check probe"),
    ("POST",   "/",        PORT_API, 200, "api root accepts POST"),
    ("POST",   "/health",  PORT_API, 405, "health-check is read-only"),
    ("GET",    "/unknown", PORT_API, 404, "missing resource"),

    # server "static" — :8082 (location / and /downloads only — no /static prefix)
    ("GET",    "/",          PORT_STATIC, 200, "static root index"),
    ("POST",   "/",          PORT_STATIC, 405, "method not allowed"),
    ("GET",    "/unknown",   PORT_STATIC, 404, "missing resource"),
    ("GET",    "/downloads", PORT_STATIC, 301, "autoindex on, bare path → trailing slash"),
]


def test_locations():
    for method, path, port, want, note in LOCATION_CASES:
        _check(f"{method} {path} :{port} → {want}  ({note})", _req(method, path, port), want)


def test_root_routes_independently_per_port():
    # "/" is defined in all three servers, but each must route to its own
    # vhost root/index — same location string, independent result per port.
    cases = [
        (PORT_ONE,    "one",    "text/html"),
        (PORT_API,    "api",    "application/json"),
        (PORT_STATIC, "static", "text/html"),
    ]
    bodies = {}
    for port, name, content_type_prefix in cases:
        r, body = _req_full("GET", "/", port)
        ct = r.getheader("Content-Type") or ""
        bodies[port] = body
        _check(f"GET / :{port} ({name}) → 200", r.status, 200)
        _check(f"GET / :{port} ({name}) Content-Type starts with '{content_type_prefix}'",
               ct.startswith(content_type_prefix), True)
    _check("vhosts 'one' and 'api' serve different bodies for the same path '/'",
           bodies[PORT_ONE] != bodies[PORT_API], True)


def test_413_oversized_body():
    # 1.1 MB body against 1 MB clientMaxBodySize on "one" (:8080)
    _check("POST /zk_apply_form 1.1MB :8080 → 413",
           _req("POST", "/zk_apply_form", PORT_ONE,
                body=b"a" * 1_100_000,
                headers={"Content-Type": "text/plain"}),
           413)
    # 600 KB body against 512 KB clientMaxBodySize on "api" (:8081)
    _check("POST / 600KB :8081 → 413",
           _req("POST", "/", PORT_API,
                body=b"a" * 614_400,
                headers={"Content-Type": "text/plain"}),
           413)


def test_redirect_location_header():
    r, _ = _req_full("GET", "/play", PORT_ONE)
    location = r.getheader("Location") or ""
    _check("GET /play :8080 → 301", r.status, 301)
    _check("GET /play Location: /game_start", location, "/game_start")


def test_autoindex_redirect_location():
    r, _ = _req_full("GET", "/files-auto", PORT_ONE)
    location = r.getheader("Location") or ""
    _check("GET /files-auto :8080 → 301", r.status, 301)
    _check("GET /files-auto Location: /files-auto/", location, "/files-auto/")


def test_files_no_autoindex_redirect_then_403():
    r, _ = _req_full("GET", "/files", PORT_ONE)
    location = r.getheader("Location") or ""
    _check("GET /files :8080 → 301", r.status, 301)
    _check("GET /files Location: /files/", location, "/files/")
    r2, _ = _req_full("GET", "/files/", PORT_ONE)
    _check("GET /files/ → 403 (autoindex off)", r2.status, 403)


def test_autoindex_content():
    # /files-auto root is "./" (repo root) — list known top-level entries.
    r, body = _req_full("GET", "/files-auto/", PORT_ONE)
    text = body.decode(errors="replace")
    _check("GET /files-auto/ :8080 → 200", r.status, 200)
    for name in ["www", "srcs", "Makefile"]:
        _check(f"autoindex lists '{name}'", name in text, True)


def test_downloads_autoindex_redirect():
    r, _ = _req_full("GET", "/downloads", PORT_STATIC)
    location = r.getheader("Location") or ""
    _check("GET /downloads :8082 → 301", r.status, 301)
    _check("GET /downloads Location: /downloads/", location, "/downloads/")


TESTS = [
    test_locations,
    test_root_routes_independently_per_port,
    test_413_oversized_body,
    test_redirect_location_header,
    test_files_no_autoindex_redirect_then_403,
    test_autoindex_redirect_location,
    test_autoindex_content,
    test_downloads_autoindex_redirect,
]


if __name__ == "__main__":
    require_server(HOST, PORT_ONE)
    require_server(HOST, PORT_API)
    require_server(HOST, PORT_STATIC)
    print(f"Testing locations on {HOST} (:{PORT_ONE} one, :{PORT_API} api, :{PORT_STATIC} static)\n")
    for t in TESTS:
        t()
    finish()
