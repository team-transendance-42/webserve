#!/usr/bin/env python3
"""
test_locations.py — verifies routing, redirects, and body-size limits.

Assumes webserv is already running on 127.0.0.1:8080 with tests/conf/default.conf.

Run from repo root:
    python3 tests/execution/test_locations.py
"""

import http.client
import sys
from helpers import _check, require_server, finish

HOST = "127.0.0.1"
PORT = 8080


def _req(method, path, body=None, headers=None):
    c = http.client.HTTPConnection(HOST, PORT, timeout=10)
    c.request(method, path, body=body, headers=headers or {})
    r = c.getresponse()
    r.read()
    return r.status


def _req_full(method, path, body=None, headers=None):
    c = http.client.HTTPConnection(HOST, PORT, timeout=10)
    c.request(method, path, body=body, headers=headers or {})
    r = c.getresponse()
    data = r.read()
    return r, data


# ── tests ─────────────────────────────────────────────────────────────────────

LOCATION_CASES = [
    ("GET",    "/",               200, "root index"),
    ("GET",    "/zombie_kittens", 200, "specific page"),
    ("GET",    "/game_start",     200, "specific page"),
    ("GET",    "/play",           301, "redirect"),
    ("GET",    "/secret",         403, "denyAll"),
    ("GET",    "/not_allowed",    200, "page served"),
    ("GET",    "/files",          301, "autoindex off, bare path → trailing slash"),
    ("GET",    "/files-auto",     301, "autoindex on, bare path → trailing slash"),
    ("GET",    "/api/data_json",  200, "json endpoint"),
    ("GET",    "/does_not_exist", 404, "missing resource"),
    ("POST",   "/zombie_kittens", 405, "method not allowed"),
    ("DELETE", "/api/data_json",  405, "method not allowed"),
    ("POST",   "/play",           405, "redirect disallows POST"),
]


def test_locations():
    for method, path, want, note in LOCATION_CASES:
        _check(f"{method} {path} → {want}  ({note})", _req(method, path), want)


def test_413_oversized_body():
    # 1.1 MB body against 1 MB clientMaxBodySize
    _check("POST /zk_apply_form 1.1MB → 413",
           _req("POST", "/zk_apply_form",
                body=b"a" * 1_100_000,
                headers={"Content-Type": "text/plain"}),
           413)


def test_redirect_location_header():
    r, _ = _req_full("GET", "/play")
    location = r.getheader("Location") or ""
    _check("GET /play → 301", r.status, 301)
    _check("GET /play Location: /game_start", location, "/game_start")


def test_autoindex_redirect_location():
    r, _ = _req_full("GET", "/files-auto")
    location = r.getheader("Location") or ""
    _check("GET /files-auto → 301", r.status, 301)
    _check("GET /files-auto Location: /files-auto/", location, "/files-auto/")


def test_files_no_autoindex_redirect_then_403():
    r, _ = _req_full("GET", "/files")
    location = r.getheader("Location") or ""
    _check("GET /files → 301", r.status, 301)
    _check("GET /files Location: /files/", location, "/files/")
    r2, _ = _req_full("GET", "/files/")
    _check("GET /files/ → 403 (autoindex off)", r2.status, 403)


def test_autoindex_content():
    r, body = _req_full("GET", "/files-auto/")
    text = body.decode(errors="replace")
    _check("GET /files-auto/ → 200", r.status, 200)
    for name in ["222_1", "dull", "zombie"]:
        _check(f"autoindex lists '{name}'", name in text, True)


TESTS = [
    test_locations,
    test_413_oversized_body,
    test_redirect_location_header,
    test_files_no_autoindex_redirect_then_403,
    test_autoindex_redirect_location,
    test_autoindex_content,
]


if __name__ == "__main__":
    require_server(HOST, PORT)
    print(f"Testing locations on {HOST}:{PORT}\n")
    for t in TESTS:
        t()
    finish()
