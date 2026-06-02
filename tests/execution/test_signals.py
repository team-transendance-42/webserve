#!/usr/bin/env python3
"""
test_signals.py — signal-handling tests:

  - SIGTERM / SIGINT  graceful shutdown (process exits, port released)
  - SIGPIPE           client disconnects before reading response; server must
                      survive (SIG_IGN or MSG_NOSIGNAL required)
  - SIGTERM during CGI  server kills its child and exits; no orphaned process

Starts its own webserv instance (port 19091) so it can be run standalone
without interfering with other tests.

Run from repo root:
    python3 tests/execution/test_signals.py
"""

import os
import signal
import socket
import subprocess
import sys
import threading
import time

PORT   = 19091
HOST   = "127.0.0.1"
CONFIG = "tests/conf/signal_test.conf"

_passed = 0
_failed = 0


def _check(label, ok, note=""):
    global _passed, _failed
    if ok:
        _passed += 1
        print(f"  PASS  {label}")
    else:
        _failed += 1
        msg = f": {note}" if note else ""
        print(f"  FAIL  {label}{msg}")


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
    if proc.returncode is None:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()


def _port_free():
    try:
        socket.create_connection((HOST, PORT), timeout=0.5).close()
        return False
    except OSError:
        return True


# ── shutdown signal tests ─────────────────────────────────────────────────────

def _test_signal(sig, label):
    proc = _start_server()

    try:
        socket.create_connection((HOST, PORT), timeout=1).close()
        alive = True
    except OSError:
        alive = False
    _check(f"{label}: server responds before signal", alive)

    timed_out = False
    proc.send_signal(sig)
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        timed_out = True
        proc.kill()
        proc.wait()

    rc = proc.returncode
    clean = rc in (0, -signal.SIGTERM, -signal.SIGINT)
    _check(f"{label}: process exits within 5 s", not timed_out)
    _check(f"{label}: exit code is clean (got {rc})", clean, f"returncode={rc}")

    time.sleep(0.3)
    _check(f"{label}: port {PORT} released after shutdown", _port_free())


def test_sigterm():
    _test_signal(signal.SIGTERM, "SIGTERM")


def test_sigint():
    _test_signal(signal.SIGINT, "SIGINT")


# ── SIGPIPE: client disconnects before reading the response ───────────────────

def test_sigpipe_client_disconnect():
    """
    Connect, send a complete HTTP request, then close the socket before reading
    the response.  The kernel delivers SIGPIPE to the server on the next write().
    Default handler is SIG_DFL (terminate), so an unprotected server dies here.
    We repeat 5 times to maximise the chance of actually hitting the write, then
    confirm the server still responds to a normal request.
    """
    proc = _start_server()
    try:
        for _ in range(5):
            try:
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.connect((HOST, PORT))
                s.sendall(b"GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n")
                s.close()   # close before reading → EPIPE on server's response write
            except OSError:
                break

        time.sleep(0.2)

        alive = False
        try:
            s2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s2.settimeout(3)
            s2.connect((HOST, PORT))
            s2.sendall(b"GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n")
            data = b""
            while True:
                chunk = s2.recv(4096)
                if not chunk:
                    break
                data += chunk
            s2.close()
            alive = data.startswith(b"HTTP/1.1 200")
        except OSError:
            pass
        _check("SIGPIPE: server alive after repeated client disconnects", alive)
    finally:
        _stop_server(proc)


# ── SIGTERM during active CGI ─────────────────────────────────────────────────

def test_sigterm_during_cgi():
    """
    SIGTERM while slow.py (10 s sleep) is running as a CGI child.  The server
    must kill the child and exit cleanly — pgrep must find no orphaned slow.py.
    Requires signal_test.conf to expose /cgi-bin pointing at tests/cgi-bin.
    """
    proc = _start_server()
    try:
        def _kick_cgi():
            try:
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.settimeout(15)
                s.connect((HOST, PORT))
                s.sendall(b"GET /cgi-bin/slow.py HTTP/1.1\r\n"
                          b"Host: 127.0.0.1\r\nConnection: close\r\n\r\n")
                s.recv(4096)
                s.close()
            except OSError:
                pass

        t = threading.Thread(target=_kick_cgi, daemon=True)
        t.start()
        time.sleep(0.5)   # give the CGI child time to start

        timed_out = False
        proc.send_signal(signal.SIGTERM)
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            timed_out = True
            proc.kill()
            proc.wait()

        _check("SIGTERM-during-CGI: server exits within 5 s", not timed_out)

        time.sleep(0.2)
        ps = subprocess.run(["pgrep", "-f", "slow.py"], capture_output=True)
        no_orphan = ps.returncode != 0   # pgrep returns 1 when no match
        _check("SIGTERM-during-CGI: no orphaned slow.py process", no_orphan)
    finally:
        _stop_server(proc)


# ── main ─────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print(f"Testing signal handling (port {PORT})\n")

    print("--- SIGTERM ---")
    test_sigterm()
    print()

    print("--- SIGINT ---")
    test_sigint()
    print()

    print("--- SIGPIPE (client disconnect) ---")
    test_sigpipe_client_disconnect()
    print()

    print("--- SIGTERM during active CGI ---")
    test_sigterm_during_cgi()

    total = _passed + _failed
    print(f"\n{'=' * 42}")
    print(f"  {total} tests: {_passed} passed, {_failed} failed")
    print(f"{'=' * 42}")
    sys.exit(0 if _failed == 0 else 1)
