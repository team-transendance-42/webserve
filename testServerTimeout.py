#!/usr/bin/env python3
"""
SERVER TIMEOUT TESTS
Tests that the server correctly closes idle connections with 408.

Each test:
1. Opens a raw TCP socket (like nc localhost 8080)
2. Optionally sends partial/complete HTTP data
3. Waits for server to close connection
4. Checks that server sent 408 and closed within expected time

run in terminal with: python3 testServerTimeout.py
"""

import socket
import time
import sys

HOST = "127.0.0.1"
PORT = 8080
SERVER_TIMEOUT = 6   # must match your SERVER_TIMEOUT constant
MARGIN = 2           # allow 2s margin for timing

pass_count = 0
fail_count = 0

# ── helper ────────────────────────────────────────────────────────────────────

def make_socket():
    """Create TCP socket and connect to server. Like: nc localhost 8080"""
    s = socket.socket()           # create TCP socket (like socket(AF_INET, SOCK_STREAM, 0))
    s.connect((HOST, PORT))       # TCP handshake   (like connect(fd, &addr, sizeof(addr)))
    return s

def wait_for_close(s):
    """
    Block until server closes connection or 15s passes.
    Returns (elapsed_seconds, data_received).
    recv() returns b"" when server calls close(fd) — same as read() returning 0.
    """
    s.settimeout(SERVER_TIMEOUT + 8)   # give server enough time to timeout
    start = time.time()
    data = b""
    try:
        while True:
            chunk = s.recv(4096)   # like recv(fd, buf, 4096, 0)
            if not chunk:          # empty = server closed connection (FIN received)
                break
            data += chunk
    except socket.timeout:
        pass                       # server never closed — test will fail
    elapsed = time.time() - start
    s.close()
    return elapsed, data

def check(name, elapsed, data, expect_408=True):
    """Print PASS/FAIL for a test."""
    global pass_count, fail_count

    timed_out_correctly = SERVER_TIMEOUT - 1 <= elapsed <= SERVER_TIMEOUT + MARGIN
    got_408 = b"408" in data

    if timed_out_correctly and (got_408 if expect_408 else True):
        print(f"PASS  {name}  ({elapsed:.1f}s)")
        pass_count += 1
    else:
        print(f"FAIL  {name}  (elapsed={elapsed:.1f}s, 408={'yes' if got_408 else 'no'})")
        fail_count += 1

# ── tests ─────────────────────────────────────────────────────────────────────

def test_idle():
    """
    nc localhost 8080
    Connect and send nothing. Server should 408 after SERVER_TIMEOUT.
    """
    s = make_socket()
    # send nothing — just like nc sitting idle
    elapsed, data = wait_for_close(s)
    check("idle TCP connection", elapsed, data)


def test_partial_headers():
    """
    (echo -n "GET / HTTP/1.1\\r\\nHost: localhost\\r\\n"; sleep 10) | nc localhost 8080
    Send incomplete headers (no blank line). Parser returns INCOMPLETE.
    Server should 408 after SERVER_TIMEOUT.
    """
    s = make_socket()
    # send partial headers — missing final \r\n blank line
    # parser sees headers but never sees COMPLETE
    s.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n")
    elapsed, data = wait_for_close(s)
    check("partial headers", elapsed, data)


def test_slow_body():
    """
    (echo -e "POST ... Content-Length: 100 ..."; sleep 10; echo data) | nc localhost 8080
    Send complete headers promising 100 bytes, then send nothing.
    Parser returns INCOMPLETE waiting for body. Server should 408.
    """
    s = make_socket()
    # complete headers — parser moves to BODY state, expects 100 bytes
    s.sendall(b"POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 100\r\n\r\n")
    # send 5 bytes of the promised 100 — rest never arrives
    s.sendall(b"hello")
    elapsed, data = wait_for_close(s)
    check("slow body (partial POST)", elapsed, data)


def test_multiple_idle():
    """
    for i in {1..5}; do nc localhost 8080 & done
    Open 5 idle connections simultaneously.
    Server must close ALL of them, not just the first one.
    """
    global pass_count, fail_count
    # open 5 sockets at once — like 5 background nc processes
    sockets = [make_socket() for _ in range(5)]

    closed = 0
    all_got_408 = True
    start = time.time()

    for s in sockets:
        elapsed, data = wait_for_close(s)   # wait for each to be closed
        if b"408" in data:
            closed += 1
        else:
            all_got_408 = False

    total_elapsed = time.time() - start

    # all 5 should close within roughly SERVER_TIMEOUT + MARGIN seconds total
    if closed == 5 and total_elapsed < (SERVER_TIMEOUT + MARGIN) * 2:
        print(f"PASS  multiple idle connections  ({closed}/5 got 408, {total_elapsed:.1f}s total)")
        pass_count += 1
    else:
        print(f"FAIL  multiple idle connections  ({closed}/5 got 408, {total_elapsed:.1f}s total)")
        fail_count += 1


def test_keepalive_then_idle():
    """
    (printf "GET / HTTP/1.1\\r\\nConnection: keep-alive\\r\\n\\r\\n"; sleep 10) | nc localhost 8080
    Send complete request, get 200 response, then go idle.
    Server should send 408 after SERVER_TIMEOUT of inactivity.
    """
    s = make_socket()
    # complete valid request — server will respond with 200
    s.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n")

    # drain the 200 response first
    s.settimeout(1.0)
    response = b""
    try:
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            response += chunk
    except socket.timeout:
        pass   # stop reading after 3s — 200 should have arrived by then

    if b"200" not in response:
        print("FAIL  keep-alive idle  (never got 200 response)")
        fail_count += 1
        s.close()
        return

    # now go idle — server should timeout and send 408
    elapsed, data = wait_for_close(s)
    check("keep-alive then idle", elapsed, data)


# ── run all ───────────────────────────────────────────────────────────────────

print(f"Testing {HOST}:{PORT}  SERVER_TIMEOUT={SERVER_TIMEOUT}s\n")
print(f"{'TEST':<30} {'RESULT'}")
print("-" * 50)

test_idle()
test_partial_headers()
test_slow_body()
test_keepalive_then_idle()
test_multiple_idle()

print("-" * 50)
print(f"Final: {pass_count} passed, {fail_count} failed")
sys.exit(0 if fail_count == 0 else 1)