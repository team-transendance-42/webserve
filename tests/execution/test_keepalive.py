#!/usr/bin/env python3
"""
test_keepalive.py — verifies HTTP keep-alive and pipelining.

Assumes webserv is already running on 127.0.0.1:8080 with tests/conf/default.conf.

Run from repo root:
    python3 tests/execution/test_keepalive.py
"""

import http.client
import shutil
import socket
import subprocess
import sys
import time
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


def _tool(*names):
    for n in names:
        p = shutil.which(n)
        if p:
            return p
    return None


# ── curl / nc / raw-socket tests ──────────────────────────────────────────────

def test_curl_keepalive_multi():
    """curl: 3 requests reusing one TCP connection via HTTP/1.1 keep-alive (--next)."""
    if not _tool("curl"):
        print("  SKIP  curl not found"); return
    result = subprocess.run(
        [
            "curl", "-s", "--http1.1",
            "-o", "/dev/null", "-w", "%{http_code}\n",
            f"http://{HOST}:{PORT}/",
            "--next",
            "-o", "/dev/null", "-w", "%{http_code}\n",
            f"http://{HOST}:{PORT}/api/data_json",
            "--next",
            "-o", "/dev/null", "-w", "%{http_code}\n",
            f"http://{HOST}:{PORT}/",
        ],
        capture_output=True, text=True, timeout=10,
    )
    codes = result.stdout.strip().splitlines()
    _check("curl keep-alive: req 1 (GET /)              → 200", codes[0] if codes else "?", "200")
    _check("curl keep-alive: req 2 (GET /api/data_json) → 200", codes[1] if len(codes) > 1 else "?", "200")
    _check("curl keep-alive: req 3 (GET /)              → 200", codes[2] if len(codes) > 2 else "?", "200")


def test_curl_no_connection_close():
    """curl: HTTP/1.1 plain GET must NOT receive Connection: close (connection stays alive)."""
    if not _tool("curl"):
        print("  SKIP  curl not found"); return
    result = subprocess.run(
        ["curl", "-s", "--http1.1", "-D", "-", "-o", "/dev/null",
         f"http://{HOST}:{PORT}/"],
        capture_output=True, text=True, timeout=10,
    )
    headers = result.stdout.lower()
    # HTTP/1.1 is persistent by default — server must not send Connection: close
    no_close = "connection: close" not in headers
    _check("curl: plain GET response has no 'Connection: close'", no_close, True)


def test_nc_pipeline_3req():
    """nc: 3 pipelined GETs (keep-alive × 2, then close) sent in one write → 3 × 200."""
    nc = _tool("nc", "ncat", "netcat")
    if not nc:
        print("  SKIP  nc/ncat/netcat not found"); return
    payload = (
        b"GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: keep-alive\r\n\r\n"
        b"GET /api/data_json HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: keep-alive\r\n\r\n"
        b"GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"
    )
    try:
        result = subprocess.run(
            ["timeout", "5", nc, HOST, str(PORT)],
            input=payload, capture_output=True, timeout=7,
        )
        data = result.stdout
    except (subprocess.TimeoutExpired, FileNotFoundError):
        data = b""

    responses = [p for p in data.split(b"HTTP/1.1") if p.startswith(b" ")]
    codes = []
    for resp in responses:
        parts = resp.split(b" ", 2)
        if len(parts) >= 2 and parts[1].isdigit():
            codes.append(int(parts[1]))

    _check("nc 3-req pipeline: got >= 3 responses",  len(codes) >= 3, True)
    if len(codes) >= 1: _check("nc 3-req pipeline: resp 1 → 200", codes[0], 200)
    if len(codes) >= 2: _check("nc 3-req pipeline: resp 2 → 200", codes[1], 200)
    if len(codes) >= 3: _check("nc 3-req pipeline: resp 3 → 200", codes[2], 200)


def test_connection_close_header():
    """Raw socket: Connection: close must close the TCP connection right after the response."""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5)
    s.connect((HOST, PORT))
    s.sendall(b"GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n")

    data = b""
    try:
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk
    except socket.timeout:
        pass
    finally:
        s.close()

    status_ok  = data.startswith(b"HTTP/1.1 200")
    header_ok  = b"connection: close" in data.lower()
    _check("conn-close: response status 200",                 status_ok, True)
    _check("conn-close: response contains 'Connection: close'", header_ok, True)


def test_keepalive_mixed_methods():
    """http.client: GET → POST → GET on one persistent connection; last GET must still be 200."""
    c = http.client.HTTPConnection(HOST, PORT, timeout=10)
    results = []

    c.request("GET", "/")
    r = c.getresponse(); results.append(r.status); r.read()

    body = b"field=value"
    c.request("POST", "/",
              body=body,
              headers={"Content-Type": "application/x-www-form-urlencoded",
                       "Content-Length": str(len(body))})
    r = c.getresponse(); results.append(r.status); r.read()

    c.request("GET", "/")
    r = c.getresponse(); results.append(r.status); r.read()

    _check("mixed-methods keep-alive: GET /        → 200",      results[0], 200)
    _check("mixed-methods keep-alive: POST / → any valid code", results[1] in range(100, 600), True)
    _check("mixed-methods keep-alive: GET / again  → 200",      results[2], 200)


def test_split_buffer_order():
    """
    Simulates a client whose TCP stack delivers data in two uneven bursts:
      burst 1: req1 (complete) + first half of req2 headers
      burst 2: second half of req2 headers + req3 + req4

    The server must parse across the TCP boundary and still respond
    req1 → req2 → req3 → req4 in arrival order.
    """
    req1 = b"GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: keep-alive\r\n\r\n"
    req2 = b"GET /api/data_json HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: keep-alive\r\n\r\n"
    req3 = b"GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: keep-alive\r\n\r\n"
    req4 = b"GET /api/data_json HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"

    # split req2 mid-header: "...Host: 127" | ".0.0.1\r\nConnection..."
    split = req2.index(b"Host: ") + 10
    burst1 = req1 + req2[:split]
    burst2 = req2[split:] + req3 + req4

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10)
    s.connect((HOST, PORT))
    s.sendall(burst1)
    time.sleep(0.05)   # give the server a chance to read the first partial burst
    s.sendall(burst2)

    data = b""
    try:
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk
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

    _check("split-buffer: got all 4 responses in order",          len(codes) >= 4, True)
    if len(codes) >= 1: _check("split-buffer: resp 1 (GET /)              → 200", codes[0], 200)
    if len(codes) >= 2: _check("split-buffer: resp 2 (GET /api/data_json) → 200", codes[1], 200)
    if len(codes) >= 3: _check("split-buffer: resp 3 (GET /)              → 200", codes[2], 200)
    if len(codes) >= 4: _check("split-buffer: resp 4 (GET /api/data_json) → 200", codes[3], 200)


def _collect_responses(s, expected_count, timeout=10):
    """
    Read pipelined HTTP/1.1 responses from socket, parsing each one via
    Content-Length so body text (including 'HTTP/1.1') never inflates the count.
    Returns list of integer status codes, one per response.
    """
    s.settimeout(timeout)
    buf = b""
    codes = []

    def _recv_more():
        try:
            chunk = s.recv(16384)
            return chunk  # empty bytes means connection closed
        except socket.timeout:
            return None  # None means timed out

    while len(codes) < expected_count:
        # ensure we have at least one response header block
        while b"\r\n\r\n" not in buf:
            chunk = _recv_more()
            if not chunk:
                return codes
            buf += chunk

        header_end = buf.index(b"\r\n\r\n")
        header_block = buf[:header_end]
        buf = buf[header_end + 4:]

        # parse status code from first line
        first_line = header_block.split(b"\r\n", 1)[0]
        parts = first_line.split(b" ", 2)
        if len(parts) < 2 or not parts[1].isdigit():
            return codes
        codes.append(int(parts[1]))

        # parse Content-Length to skip the body
        body_len = 0
        for line in header_block.split(b"\r\n")[1:]:
            if line.lower().startswith(b"content-length:"):
                try:
                    body_len = int(line.split(b":", 1)[1].strip())
                except ValueError:
                    pass
                break

        while len(buf) < body_len:
            chunk = _recv_more()
            if not chunk:
                return codes
            buf += chunk
        buf = buf[body_len:]

    return codes


PIPELINE_PATHS = [
    "/",
    "/api/data_json",
    "/zombie_kittens",
    "/game_start",
    "/zk_apply_form",
]


def test_pipeline_50_one_shot():
    """
    50 GET requests sent as a single sendall() before reading any response.

    With 50 × ~55 bytes ≈ 2750 bytes, TCP will deliver them across at least
    two recv() calls on the server (MTU ~1460 bytes), so the parser must
    reassemble request boundaries that span separate reads.  Responses must
    come back in the same order as requests were sent.
    """
    n = 50
    paths = [PIPELINE_PATHS[i % len(PIPELINE_PATHS)] for i in range(n)]
    # last request closes the connection so the server flushes immediately
    raw = b"".join(
        (b"GET " + p.encode() + b" HTTP/1.1\r\nHost: 127.0.0.1\r\n"
         + (b"Connection: keep-alive" if i < n - 1 else b"Connection: close")
         + b"\r\n\r\n")
        for i, p in enumerate(paths)
    )

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))
    s.sendall(raw)
    codes = _collect_responses(s, n)
    s.close()

    _check(f"pipeline-50: got all {n} responses", len(codes) >= n, True)
    for i, code in enumerate(codes[:n]):
        _check(f"pipeline-50: resp {i+1:02d} → 200", code, 200)


def test_pipeline_burst_send():
    """
    50 GET requests sent in bursts of 3 with os.sched_yield() between bursts.

    Simulates clients whose TCP stack drains its window in small increments,
    so the server sees intermittent partial data between its event-loop ticks.
    Verifies that no request at a burst boundary is silently dropped.
    """
    import os
    n = 50
    paths = [PIPELINE_PATHS[i % len(PIPELINE_PATHS)] for i in range(n)]
    reqs = [
        (b"GET " + p.encode() + b" HTTP/1.1\r\nHost: 127.0.0.1\r\n"
         + (b"Connection: keep-alive" if i < n - 1 else b"Connection: close")
         + b"\r\n\r\n")
        for i, p in enumerate(paths)
    ]

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))
    burst = 3
    for start in range(0, n, burst):
        s.sendall(b"".join(reqs[start:start + burst]))
        os.sched_yield()  # yield CPU slice — does not sleep, just reschedules
    codes = _collect_responses(s, n)
    s.close()

    _check(f"pipeline-burst: got all {n} responses", len(codes) >= n, True)
    for i, code in enumerate(codes[:n]):
        _check(f"pipeline-burst: resp {i+1:02d} → 200", code, 200)


def test_pipeline_post_interleaved():
    """
    POST requests interleaved in a pipeline of GETs.

    Pattern: GET GET POST GET GET POST … (every 3rd is a POST with a body).
    The critical invariant: the server must not treat body bytes as the start
    of the next request's method line.  Any body/header boundary confusion
    will misparse all subsequent requests in the pipeline.
    """
    n = 30
    reqs = []
    for i in range(n):
        is_last = i == n - 1
        conn = b"Connection: close\r\n" if is_last else b""
        if i % 3 == 2:
            body = b"field=value"
            req = (
                b"POST / HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                b"Content-Type: application/x-www-form-urlencoded\r\n"
                b"Content-Length: " + str(len(body)).encode() + b"\r\n"
                + conn + b"\r\n" + body
            )
        else:
            path = PIPELINE_PATHS[i % len(PIPELINE_PATHS)].encode()
            req = b"GET " + path + b" HTTP/1.1\r\nHost: 127.0.0.1\r\n" + conn + b"\r\n"
        reqs.append(req)

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))
    s.sendall(b"".join(reqs))
    codes = _collect_responses(s, n)
    s.close()

    _check(f"pipeline-post-interleaved: got all {n} responses", len(codes) >= n, True)
    # POST to / returns whatever the server decides (200, 405, etc.) — just not a parse error
    for i, code in enumerate(codes[:n]):
        _check(f"pipeline-post-interleaved: resp {i+1:02d} is valid HTTP",
               100 <= code < 600, True)
    # GETs at positions 0,1,3,4,6,7,… must still be 200
    get_positions = [i for i in range(n) if i % 3 != 2]
    for i in get_positions:
        if i < len(codes):
            _check(f"pipeline-post-interleaved: GET resp {i+1:02d} → 200", codes[i], 200)


TESTS = [
    test_keepalive_reuse,
    test_pipelining,
    test_curl_keepalive_multi,
    test_curl_no_connection_close,
    test_nc_pipeline_3req,
    test_connection_close_header,
    test_keepalive_mixed_methods,
    test_split_buffer_order,
    test_pipeline_50_one_shot,
    test_pipeline_burst_send,
    test_pipeline_post_interleaved,
]


if __name__ == "__main__":
    require_server(HOST, PORT)
    print(f"Testing keep-alive and pipelining on {HOST}:{PORT}\n")
    for t in TESTS:
        t()
    finish()
