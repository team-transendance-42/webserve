#!/usr/bin/env python3
"""
test_keepalive.py — verifies HTTP keep-alive and pipelining.

Assumes webserv is already running on 127.0.0.1:8080 with tests/conf/default.conf.

Run from repo root:
    python3 tests/execution/test_keepalive.py
"""

import http.client
import socket
import sys
from helpers import _check, require_server, finish

HOST = "127.0.0.1"
PORT = 8080


# ── tests ─────────────────────────────────────────────────────────────────────

def test_keepalive_reuse():
    """Three sequential requests on the same HTTPConnection (HTTP/1.1 keep-alive)."""
    c = http.client.HTTPConnection(HOST, PORT, timeout=10)
    results = []
    for path in ["/", "/api/data_json", "/"]:
        c.request("GET", path)
        r = c.getresponse()
        results.append(r.status)
        r.read()
    _check("keep-alive req 1 (GET /)          → 200", results[0], 200)
    _check("keep-alive req 2 (GET /api/data_json) → 200", results[1], 200)
    _check("keep-alive req 3 (GET /)          → 200", results[2], 200)


def test_pipelining():
    """Send two GET requests before reading either response (HTTP/1.1 pipelining)."""
    raw = (
        b"GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n"
        b"GET /api/data_json HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n"
    )
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10)
    s.connect((HOST, PORT))
    s.sendall(raw)

    data = b""
    try:
        while data.count(b"HTTP/1.1") < 2 or not data.endswith((b"\r\n\r\n", b"\n\n")):
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk
            # Stop once we have two complete response headers
            if data.count(b"HTTP/1.1") >= 2 and b"\r\n\r\n" in data.split(b"HTTP/1.1", 2)[-1]:
                break
    except socket.timeout:
        pass
    finally:
        s.close()

    responses = [p for p in data.split(b"HTTP/1.1") if p.startswith(b" ")]
    codes = []
    for resp in responses:
        parts = resp.split(b" ", 2)
        if len(parts) >= 2 and parts[1].isdigit():
            codes.append(int(parts[1]))

    _check("pipelining: response count >= 2", len(codes) >= 2, True)
    if len(codes) >= 1:
        _check("pipelining: response 1 → 200", codes[0], 200)
    if len(codes) >= 2:
        _check("pipelining: response 2 → 200", codes[1], 200)


TESTS = [test_keepalive_reuse, test_pipelining]


if __name__ == "__main__":
    require_server(HOST, PORT)
    print(f"Testing keep-alive and pipelining on {HOST}:{PORT}\n")
    for t in TESTS:
        t()
    finish()
