#!/usr/bin/env python3
"""
test_vhost.py — verifies name-based virtual hosting (Host header routing).

Both virtual servers (alpha, beta) share port 18080.  The server picks the
right config based on the Host: header value.

Starts its own webserv instance so it can run standalone.

Run from repo root:
    python3 tests/execution/test_vhost.py
"""

import http.client
import os
import socket
import subprocess
import sys
import time
from helpers import finish

PORT   = 18080
HOST   = "127.0.0.1"
CONFIG = "tests/conf/vhost_test.conf"

_passed = 0
_failed = 0


def _check(label, got, want):
    global _passed, _failed
    if got == want:
        _passed += 1
        print(f"  PASS  {label}")
    else:
        _failed += 1
        print(f"  FAIL  {label}: got {got!r}, want {want!r}")


def _start_server():
    if not os.path.exists("./webserv"):
        print("ERROR: ./webserv not found — run make first")
        sys.exit(1)
    proc = subprocess.Popen(
        ["./webserv", CONFIG],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    for _ in range(30):
        try:
            socket.create_connection((HOST, PORT), timeout=0.1).close()
            return proc
        except OSError:
            time.sleep(0.1)
    proc.kill()
    raise RuntimeError(f"webserv did not start on port {PORT}")


def _stop_server(proc):
    proc.terminate()
    try:
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        proc.kill()


def _req(vhost_name, path):
    """GET request with an explicit Host: header to select the virtual server."""
    c = http.client.HTTPConnection(HOST, PORT, timeout=10)
    c.request("GET", path, headers={"Host": vhost_name})
    r = c.getresponse()
    body = r.read()
    ct = r.getheader("Content-Type") or ""
    return r.status, ct, body


# ── tests ─────────────────────────────────────────────────────────────────────

def test_alpha_vhost(proc):
    status, ct, body = _req("alpha", "/")
    _check("Host: alpha → 200", status, 200)
    _check("Host: alpha Content-Type: text/html", ct.startswith("text/html"), True)
    doctype = b"<!DOCTYPE html>" in body or b"<!doctype html>" in body
    _check("Host: alpha body is HTML", doctype, True)


def test_beta_vhost(proc):
    status, ct, body = _req("beta", "/")
    _check("Host: beta → 200", status, 200)
    _check("Host: beta Content-Type: application/json", ct.startswith("application/json"), True)
    _check("Host: beta body is JSON", body.strip().startswith(b"{"), True)


def test_unknown_host_fallback(proc):
    # No match → falls back to first config (alpha), which serves HTML
    status, ct, _ = _req("unknown-host", "/")
    _check("Host: unknown-host → 200 (fallback to first vhost)", status, 200)
    _check("fallback vhost Content-Type: text/html", ct.startswith("text/html"), True)


if __name__ == "__main__":
    print(f"Starting webserv ({CONFIG}) on {HOST}:{PORT} …")
    server = _start_server()
    print("Server ready.\n")

    try:
        test_alpha_vhost(server)
        test_beta_vhost(server)
        test_unknown_host_fallback(server)
    finally:
        _stop_server(server)

    total = _passed + _failed
    print(f"\n{'=' * 42}")
    print(f"  {total} tests: {_passed} passed, {_failed} failed")
    print(f"{'=' * 42}")
    sys.exit(0 if _failed == 0 else 1)
