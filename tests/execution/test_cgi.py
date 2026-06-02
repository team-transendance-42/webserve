#!/usr/bin/env python3
"""
test_cgi.py — verifies CGI execution: success path, env vars, POST body.

Assumes webserv is already running on 127.0.0.1:8080 with tests/conf/default.conf.
The /cgi-bin location in that config roots at ./www/one/cgi-bin and executes .py
files with /usr/bin/python3.

Run from repo root:
    python3 tests/execution/test_cgi.py
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
    data = r.read()
    return r, data.decode(errors="replace")


# ── tests ─────────────────────────────────────────────────────────────────────

def test_cgi_get_success():
    """GET to a CGI script returns 200 and the script's own output."""
    r, body = _req("GET", "/cgi-bin/pythoncgitest.py")
    _check("GET /cgi-bin/pythoncgitest.py → 200", r.status, 200)
    _check("body contains script marker", "CGI script is working correctly" in body, True)


def test_cgi_request_method_get():
    r, body = _req("GET", "/cgi-bin/env_dump.py")
    _check("GET env_dump.py → 200", r.status, 200)
    _check("REQUEST_METHOD=GET", "REQUEST_METHOD=GET" in body, True)


def test_cgi_request_method_post():
    r, body = _req("POST", "/cgi-bin/env_dump.py",
                   body=b"x",
                   headers={"Content-Type": "text/plain"})
    _check("POST env_dump.py → 200", r.status, 200)
    _check("REQUEST_METHOD=POST", "REQUEST_METHOD=POST" in body, True)


def test_cgi_query_string():
    r, body = _req("GET", "/cgi-bin/env_dump.py?color=red&size=42")
    _check("GET env_dump.py?... → 200", r.status, 200)
    _check("QUERY_STRING=color=red&size=42", "QUERY_STRING=color=red&size=42" in body, True)


def test_cgi_query_string_empty_on_plain_get():
    r, body = _req("GET", "/cgi-bin/env_dump.py")
    _check("QUERY_STRING empty when no query", "QUERY_STRING=" in body, True)
    lines = {l.split("=", 1)[0]: l.split("=", 1)[1] if "=" in l else ""
             for l in body.splitlines()}
    qs = lines.get("QUERY_STRING", "MISSING")
    _check("QUERY_STRING value is empty", qs, "")


def test_cgi_post_body():
    payload = b"hello from test"
    r, body = _req("POST", "/cgi-bin/env_dump.py",
                   body=payload,
                   headers={"Content-Type": "text/plain"})
    _check("POST env_dump.py → 200", r.status, 200)
    _check("body= echoes POST payload", f"body={payload.decode()}" in body, True)


def test_cgi_content_type_env():
    r, body = _req("POST", "/cgi-bin/env_dump.py",
                   body=b"{}",
                   headers={"Content-Type": "application/json"})
    _check("POST with Content-Type: application/json → 200", r.status, 200)
    _check("CONTENT_TYPE=application/json", "CONTENT_TYPE=application/json" in body, True)


def test_cgi_content_length_env():
    payload = b"twelve bytes"  # 12 bytes
    r, body = _req("POST", "/cgi-bin/env_dump.py",
                   body=payload,
                   headers={"Content-Type": "text/plain"})
    _check("POST env_dump.py → 200", r.status, 200)
    _check(f"CONTENT_LENGTH={len(payload)}",
           f"CONTENT_LENGTH={len(payload)}" in body, True)


TESTS = [
    test_cgi_get_success,
    test_cgi_request_method_get,
    test_cgi_request_method_post,
    test_cgi_query_string,
    test_cgi_query_string_empty_on_plain_get,
    test_cgi_post_body,
    test_cgi_content_type_env,
    test_cgi_content_length_env,
]


if __name__ == "__main__":
    require_server(HOST, PORT)
    print(f"Testing CGI on {HOST}:{PORT}\n")
    for t in TESTS:
        t()
    finish()
