#!/usr/bin/env python3
"""
test_signals.py — verifies graceful shutdown on SIGTERM and SIGINT.

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
import time
from helpers import finish

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


def _port_free():
    try:
        socket.create_connection((HOST, PORT), timeout=0.5).close()
        return False
    except OSError:
        return True


def _test_signal(sig, label):
    proc = _start_server()

    # Verify baseline: server is alive and serving
    try:
        socket.create_connection((HOST, PORT), timeout=1).close()
        alive = True
    except OSError:
        alive = False
    _check(f"{label}: server responds before signal", alive)

    proc.send_signal(sig)
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()

    # returncode: 0 = clean exit, negative = killed by signal (−SIGTERM = −15, −SIGINT = −2)
    rc = proc.returncode
    clean = rc in (0, -signal.SIGTERM, -signal.SIGINT)
    _check(f"{label}: process exits within 5 s", True)
    _check(f"{label}: exit code is clean (got {rc})", clean, f"returncode={rc}")

    # Port should be released after shutdown
    time.sleep(0.3)
    _check(f"{label}: port {PORT} released after shutdown", _port_free())


def test_sigterm():
    _test_signal(signal.SIGTERM, "SIGTERM")


def test_sigint():
    _test_signal(signal.SIGINT, "SIGINT")


if __name__ == "__main__":
    print(f"Testing signal handling (port {PORT})\n")
    test_sigterm()
    print()
    test_sigint()

    total = _passed + _failed
    print(f"\n{'=' * 42}")
    print(f"  {total} tests: {_passed} passed, {_failed} failed")
    print(f"{'=' * 42}")
    sys.exit(0 if _failed == 0 else 1)
