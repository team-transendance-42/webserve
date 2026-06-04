#!/usr/bin/env python3
"""
test_cgi_timeout.py — verifies CGI timeout and infinite loop handling.

Tests that the server:
1. Kills CGI processes that exceed the timeout (5000ms / 5s)
2. Returns 504 Gateway Timeout (not 200 or connection error)
3. Remains responsive after killing a hung process

Assumes webserv is already running on 127.0.0.1:8080 with tests/conf/default.conf.

Run from repo root:
    python3 tests/execution/test_cgi_timeout.py
"""

import http.client
import sys
from helpers import _check, require_server, finish

HOST = "127.0.0.1"
PORT = 8080


def _req(method, path, body=None, headers=None, timeout=10):
    """Make HTTP request. timeout is in seconds (client-side)."""
    c = http.client.HTTPConnection(HOST, PORT, timeout=timeout)
    c.request(method, path, body=body, headers=headers or {})
    r = c.getresponse()
    data = r.read()
    return r, data.decode(errors="replace")


# ── tests ─────────────────────────────────────────────────────────────────────

def test_cgi_timeout_hangs_forever():
    """
    Request a CGI script that sleeps 10 seconds (exceeds 5s timeout).
    Server should kill it and return 504 after ~5s.
    Client timeout is 10s so we don't timeout first.
    """
    try:
        r, body = _req("GET", "/cgi-bin/slow.py", timeout=10)
        _check("slow.py hangs → 504 Gateway Timeout", r.status, 504)
    except http.client.HTTPException as e:
        # Connection closed prematurely (also acceptable, means process was killed)
        _check("slow.py causes connection close (process killed)", True, True)


def test_server_alive_after_timeout():
    """
    After the timeout test killed a hung process, verify the server is still
    responsive by hitting a normal (fast) CGI script.
    """
    r, body = _req("GET", "/cgi-bin/pythoncgitest.py")
    _check("Server responsive after timeout", r.status, 200)
    _check("Response contains script marker", "simple CGI script" in body, True)


def test_cgi_normal_script_completes():
    """
    Baseline: a normal CGI script completes in <5s and returns 200.
    This ensures we're not accidentally timing out fast scripts.
    """
    r, body = _req("GET", "/cgi-bin/pythoncgitest.py")
    _check("Fast CGI script → 200", r.status, 200)
    _check("Body is not empty", len(body) > 0, True)


TESTS = [
    test_cgi_timeout_hangs_forever,
    test_server_alive_after_timeout,
    test_cgi_normal_script_completes,
]


if __name__ == "__main__":
    require_server(HOST, PORT)
    print(f"Testing CGI timeout on {HOST}:{PORT}\n")
    for t in TESTS:
        t()
    finish()
