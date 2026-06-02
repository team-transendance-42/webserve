#!/usr/bin/env python3
"""
test_head.py — verifies HEAD method behaviour.

Assumes webserv is already running on 127.0.0.1:8080 with tests/conf/default.conf.

Run from repo root:
    python3 tests/execution/test_head.py
"""

import http.client
import sys
from helpers import _check, require_server, finish

HOST = "127.0.0.1"
PORT = 8080


def _get(path):
    c = http.client.HTTPConnection(HOST, PORT, timeout=5)
    c.request("GET", path)
    r = c.getresponse()
    body = r.read()
    return r.status, r, body


def _head(path):
    c = http.client.HTTPConnection(HOST, PORT, timeout=5)
    c.request("HEAD", path)
    r = c.getresponse()
    body = r.read()
    return r.status, r, body


# ── tests ─────────────────────────────────────────────────────────────────────

def test_head_200():
    code, r, body = _head("/")
    _check("HEAD / → 200",       code,      200)
    _check("HEAD / → no body",   len(body), 0)


def test_head_content_length_matches_get():
    # RFC 9110 §9.3.2: HEAD headers must be identical to what GET would return
    _, get_r, _ = _get("/")
    _, head_r, _ = _head("/")
    _check(
        "HEAD Content-Length == GET Content-Length",
        head_r.getheader("Content-Length"),
        get_r.getheader("Content-Length"),
    )


def test_head_has_content_type():
    _, r, _ = _head("/")
    _check("HEAD has Content-Type", bool(r.getheader("Content-Type")), True)


def test_head_has_date():
    _, r, _ = _head("/")
    _check("HEAD has Date header", bool(r.getheader("Date")), True)


def test_head_404():
    code, r, body = _head("/no_such_path_xyz")
    _check("HEAD nonexistent → 404",  code,      404)
    _check("HEAD 404 → no body",      len(body), 0)


def test_head_403():
    code, r, body = _head("/secret")
    _check("HEAD /secret (deny all) → 403", code,      403)
    _check("HEAD 403 → no body",            len(body), 0)


def test_head_redirect():
    # HEAD on a redirect: 301 + Location header, no body
    code, r, body = _head("/play")
    _check("HEAD /play → 301",              code,                    301)
    _check("HEAD 301 has Location header",  bool(r.getheader("Location")), True)
    _check("HEAD 301 → no body",            len(body),               0)


def test_head_implicit_on_get_location():
    # HEAD is not listed in allowedMethod for /uploads (GET DELETE are) — must still work
    code, r, body = _head("/uploads/")
    _check("HEAD on GET location (implicit) → 200", code,      200)
    _check("HEAD implicit → no body",               len(body), 0)


def test_head_405_on_post_only():
    # /zk_apply_form allows GET POST — so HEAD is fine; /uploads allows GET DELETE
    # Use /api/data_json which is GET-only — HEAD must be allowed
    code, r, body = _head("/api/data_json")
    _check("HEAD on GET-only location → 200", code,      200)
    _check("HEAD GET-only → no body",         len(body), 0)


TESTS = [
    test_head_200,
    test_head_content_length_matches_get,
    test_head_has_content_type,
    test_head_has_date,
    test_head_404,
    test_head_403,
    test_head_redirect,
    test_head_implicit_on_get_location,
    test_head_405_on_post_only,
]

if __name__ == "__main__":
    require_server(HOST, PORT)
    print(f"Running HEAD tests against {HOST}:{PORT} …\n")
    for t in TESTS:
        t()
    finish()
