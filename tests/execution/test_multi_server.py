#!/usr/bin/env python3
"""
test_multi_server.py — verifies the api (8081) and static (8082) virtual servers.

Assumes webserv is already running with tests/conf/default.conf.

Run from repo root:
    python3 tests/execution/test_multi_server.py
"""

import http.client
import sys
from helpers import _check, require_server, finish

HOST = "127.0.0.1"


def _req(port, method, path, body=None, headers=None, follow_redirects=False):
    c = http.client.HTTPConnection(HOST, port, timeout=10)
    c.request(method, path, body=body, headers=headers or {})
    r = c.getresponse()
    r.read()
    if follow_redirects and r.status in (301, 302):
        location = r.getheader("Location", "")
        if location:
            return _req(port, "GET", location, follow_redirects=True)
    return r.status


# ── tests ─────────────────────────────────────────────────────────────────────

def test_api_server():
    print("=== PORT 8081 (api server) ===")
    _check("GET  /api/v1     → 200", _req(8081, "GET",  "/api/v1"),      200)
    _check("GET  /api/health → 200", _req(8081, "GET",  "/api/health"),  200)
    _check("POST /api/v1     → 200", _req(8081, "POST", "/api/v1"),      200)
    _check("POST /api/health → 405", _req(8081, "POST", "/api/health"),  405)
    _check("GET  /unknown    → 404", _req(8081, "GET",  "/unknown"),      404)
    # 600 KB body against 512 KB clientMaxBodySize → err code 413
    _check("POST /api/v1 600KB → 413",
           _req(8081, "POST", "/api/v1",
                body=b"a" * 614_400,
                headers={"Content-Type": "text/plain"}),
           413)


def test_static_server():
    print("\n=== PORT 8082 (static server) ===")
    _check("GET  /static    → 200", _req(8082, "GET",  "/static"),                          200)
    _check("GET  /downloads → 200", _req(8082, "GET",  "/downloads", follow_redirects=True), 200)
    _check("POST /static    → 405", _req(8082, "POST", "/static"),                          405)
    _check("GET  /unknown   → 404", _req(8082, "GET",  "/unknown"),                         404)


TESTS = [test_api_server, test_static_server]


if __name__ == "__main__":
    require_server(HOST, 8081)
    require_server(HOST, 8082)
    print(f"Testing virtual servers on {HOST}\n")
    for t in TESTS:
        t()
    finish()
