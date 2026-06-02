#!/usr/bin/env python3
"""
test_static_content.py — verifies response bodies and Content-Type headers.

Assumes webserv is already running on 127.0.0.1:8080 with tests/conf/default.conf.

Run from repo root:
    python3 tests/execution/test_static_content.py
"""

import http.client
import sys
from helpers import _check, require_server, finish

HOST = "127.0.0.1"
PORT = 8080


def _req(method, path, headers=None):
    c = http.client.HTTPConnection(HOST, PORT, timeout=10)
    c.request(method, path, headers=headers or {})
    r = c.getresponse()
    body = r.read()
    return r, body


# ── tests ─────────────────────────────────────────────────────────────────────

def test_html_content_type():
    r, body = _req("GET", "/")
    ct = r.getheader("Content-Type") or ""
    _check("GET / → 200", r.status, 200)
    _check("GET / Content-Type: text/html", ct.startswith("text/html"), True)
    doctype = b"<!DOCTYPE html>" in body or b"<!doctype html>" in body
    _check("GET / body contains DOCTYPE", doctype, True)


def test_json_content_type():
    r, body = _req("GET", "/api/data_json")
    ct = r.getheader("Content-Type") or ""
    _check("GET /api/data_json → 200", r.status, 200)
    _check("GET /api/data_json Content-Type: application/json", ct.startswith("application/json"), True)
    _check("GET /api/data_json body is JSON object", body.strip().startswith(b"{"), True)


def test_css_content_type():
    r, body = _req("GET", "/common.css")
    ct = r.getheader("Content-Type") or ""
    _check("GET /common.css → 200", r.status, 200)
    _check("GET /common.css Content-Type: text/css", ct.startswith("text/css"), True)
    _check("GET /common.css body non-empty", len(body) > 0, True)


def test_content_length_header():
    r, body = _req("GET", "/api/data_json")
    cl = r.getheader("Content-Length")
    _check("GET /api/data_json Content-Length present", cl is not None, True)
    if cl is not None:
        _check("GET /api/data_json Content-Length matches body", int(cl), len(body))


TESTS = [
    test_html_content_type,
    test_json_content_type,
    test_css_content_type,
    test_content_length_header,
]


if __name__ == "__main__":
    require_server(HOST, PORT)
    print(f"Testing static content on {HOST}:{PORT}\n")
    for t in TESTS:
        t()
    finish()
