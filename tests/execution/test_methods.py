#!/usr/bin/env python3
"""
test_methods.py — verifies HTTP method enforcement across all locations.

Assumes webserv is already running on 127.0.0.1:8080 with tests/conf/default.conf.

Run from repo root:
    python3 tests/execution/test_methods.py
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


# ── tests ─────────────────────────────────────────────────────────────────────

def test_delete_on_get_only():
    for path in ["/", "/zombie_kittens", "/game_start", "/play",
                 "/files", "/files-auto", "/api/data_json"]:
        _check(f"DELETE {path} → 405", _req("DELETE", path), 405)


def test_post_on_get_only():
    for path in ["/zombie_kittens", "/game_start", "/play",
                 "/files", "/files-auto", "/api/data_json"]:
        _check(f"POST {path} → 405", _req("POST", path), 405)
    _check("POST /secret → 403", _req("POST", "/secret"), 403)


def test_allowed_methods():
    _check("POST / → 200",
           _req("POST", "/", body=b"x", headers={"Content-Type": "text/plain"}), 200)
    _check("POST /zk_apply_form → 200",
           _req("POST", "/zk_apply_form", body=b"x", headers={"Content-Type": "text/plain"}), 200)
    _check("GET /zk_apply_form → 200", _req("GET", "/zk_apply_form"), 200)


TESTS = [test_delete_on_get_only, test_post_on_get_only, test_allowed_methods]


if __name__ == "__main__":
    require_server(HOST, PORT)
    print(f"Testing HTTP methods on {HOST}:{PORT}\n")
    for t in TESTS:
        t()
    finish()
