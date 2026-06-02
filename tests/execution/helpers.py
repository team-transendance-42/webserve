#!/usr/bin/env python3
# Shared utilities for test_*.py files in tests/execution/.
# Import with: from helpers import _check, require_server, finish

import socket
import sys

_passed = 0
_failed = 0


def _check(label, got, want):
    global _passed, _failed
    if got == want:
        _passed += 1
        print(f"  PASS  {label}")
    else:
        _failed += 1
        print(f"  FAIL  {label}: got {got}, want {want}")


def require_server(host, port, config="tests/conf/default.conf"):
    try:
        socket.create_connection((host, port), timeout=1).close()
    except OSError:
        print(f"ERROR: no server on {host}:{port} — start with: ./webserv {config}")
        sys.exit(1)


def finish():
    total = _passed + _failed
    print(f"\n{'=' * 42}")
    print(f"  {total} tests: {_passed} passed, {_failed} failed")
    print(f"{'=' * 42}")
    sys.exit(0 if _failed == 0 else 1)
