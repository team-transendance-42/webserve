#!/usr/bin/env python3
"""
test_err_codes.py — verifies that webserv emits the correct status-line text
and required headers for every HTTP code it can produce.

Starts the server with tests/conf/test_err_codes.conf on port 19090,
runs all cases, then shuts down.

Run from repo root, webserv should not be running:
    python3 tests/test_err_codes.py
"""

import http.client
import os
import stat
import socket
import subprocess
import sys
import time

PORT   = 19090
HOST   = "127.0.0.1"
CONFIG = "tests/conf/test_err_codes.conf"

# Upload dir created by the server on first upload; cleaned up after tests.
UPLOAD_DIR = "./www/testfiles"

_passed = 0
_failed = 0

# ── helpers ──────────────────────────────────────────────────────────────────

def _check(label, got_code, want_code, got_reason, want_reason, extras=None):
    """
    extras: list of (description, bool) — each False entry is a failure.
    Prints PASS/FAIL and updates global counters.
    """
    global _passed, _failed
    issues = []
    if got_code != want_code:
        issues.append(f"code {got_code} != {want_code}")
    if got_reason != want_reason:
        issues.append(f"reason '{got_reason}' != '{want_reason}'")
    for desc, ok in (extras or []):
        if not ok:
            issues.append(desc)
    if issues:
        _failed += 1
        print(f"  FAIL  {label}: {', '.join(issues)}")
    else:
        _passed += 1
        print(f"  PASS  {label}")


def _request(method, path, headers=None, body=None):
    """One HTTP/1.1 request; returns (status_code, reason_phrase, response)."""
    c = http.client.HTTPConnection(HOST, PORT, timeout=10)
    c.request(method, path, body=body, headers=headers or {})
    r = c.getresponse()
    r.read()  # consume body so the connection stays clean
    return r.status, r.reason, r


def _raw_request(raw_bytes):
    """
    Send raw bytes over TCP (bypasses http.client which always adds Host).
    Returns (status_code, reason_phrase) parsed from the status line.
    Used to test 400 by omitting the Host header that HTTP/1.1 requires.
    """
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5)
    s.connect((HOST, PORT))
    s.sendall(raw_bytes)
    data = b""
    try:
        while b"\r\n" not in data:
            chunk = s.recv(256)
            if not chunk:
                break
            data += chunk
    except socket.timeout:
        pass # do nth here
    finally:
        s.close()
    first_line = data.split(b"\r\n")[0].decode(errors="replace") # substitude ? for bad bytes, dont crash
    parts = first_line.split(" ", 2)
    if len(parts) < 3:
        return None, first_line
    return int(parts[1]), parts[2]

# ── server lifecycle ──────────────────────────────────────────────────────────

def _start_server():
    """Start webserv and wait up to 3 s for the port to be ready."""
    proc = subprocess.Popen( # ./webserv tests/conf/test_err_codes.conf
        ["./webserv", CONFIG],
        stdout=subprocess.DEVNULL, # black whole: discards logs
        stderr=subprocess.DEVNULL,
    )
    for _ in range(30): # 30 x 0.1s = 3s
        try:
            socket.create_connection((HOST, PORT), timeout=0.1).close()
            return proc
        except OSError:
            time.sleep(0.1)
    proc.kill() # please stop: clean
    raise RuntimeError(f"webserv did not start on port {PORT}")


def _stop_server(proc):
    proc.terminate() # die now
    try:
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        proc.kill()

# ── individual tests ──────────────────────────────────────────────────────────

def test_200():
    code, reason, _ = _request("GET", "/")
    _check("200 OK — index page", code, 200, reason, "OK")


def test_201():
    # POST with X-Filename header → file upload → 201 Created; X- headers are custom
    code, reason, _ = _request(
        "POST", "/testfiles",
        headers={"X-Filename": "test_upload.txt", "Content-Type": "text/plain"},
        body=b"hello from test",
    )
    _check("201 Created — file upload", code, 201, reason, "Created")


def test_204():
    # Upload a file first, then DELETE it → 204 No Content
    _request(
        "POST", "/testfiles",
        headers={"X-Filename": "test_delete.txt", "Content-Type": "text/plain"},
        body=b"to be deleted",
    )
    code, reason, _ = _request("DELETE", "/testfiles/test_delete.txt")
    _check("204 No Content — file delete", code, 204, reason, "No Content")


def test_400():
    # HTTP/1.1 without Host header → 400 Bad Request
    # http.client always adds Host, so we use raw sockets here.
    code, reason = _raw_request(b"GET / HTTP/1.1\r\n\r\n")
    _check("400 Bad Request — missing Host header", code, 400, reason, "Bad Request")


def test_403():
    code, reason, _ = _request("GET", "/secret")
    _check("403 Forbidden — deny all location", code, 403, reason, "Forbidden")


def test_404():
    code, reason, _ = _request("GET", "/this_path_does_not_exist")
    _check("404 Not Found — unknown path", code, 404, reason, "Not Found")


def test_405():
    # POST to GET-only location.
    # RFC 9110 §15.5.6: 405 MUST include an Allow header.
    code, reason, r = _request("POST", "/")
    allow = r.getheader("Allow") or ""
    _check(
        "405 Method Not Allowed — wrong method + Allow header",
        code, 405, reason, "Method Not Allowed",
        [("Allow header present (RFC 9110 §15.5.6)", len(allow) > 0)],
    )


def test_413():
    # Body larger than clientMaxBodySize 5 in /limited → 413
    code, reason, _ = _request(
        "POST", "/limited",
        headers={"Content-Type": "text/plain"},
        body=b"X" * 100,
    )
    _check("413 Payload Too Large — body over limit", code, 413, reason, "Payload Too Large")


def test_415():
    # multipart/form-data upload → UploadHandler returns 415
    code, reason, _ = _request(
        "POST", "/testfiles",
        headers={"Content-Type": "multipart/form-data; boundary=abc"},
        body=b"--abc\r\n\r\nhello\r\n--abc--",
    )
    _check("415 Unsupported Media Type — multipart upload", code, 415, reason, "Unsupported Media Type")


def test_504():
    # slow.py sleeps 10s; server CGI timeout is 5000ms → 504 Gateway Timeout
    print("  NOTE  test_504 waits ~6 s for the CGI timeout to fire …", flush=True)
    c = http.client.HTTPConnection(HOST, PORT, timeout=15)
    c.request("GET", "/cgi-bin/slow.py")
    r = c.getresponse()
    r.read()
    _check("504 Gateway Timeout — CGI script timeout", r.status, 504, r.reason, "Gateway Timeout")


def test_501():
    # PUT is a known-but-unimplemented method → parser sets method=UNKNOWN → 501
    code, reason = _raw_request(b"PUT / HTTP/1.1\r\nHost: localhost\r\n\r\n")
    _check("501 Not Implemented — PUT method", code, 501, reason, "Not Implemented")


def test_400_garbage_method():
    # garbage method → PARSE_ERROR → 400 (same as nginx)
    code, reason = _raw_request(b"BLA / HTTP/1.1\r\nHost: localhost\r\n\r\n")
    _check("400 Bad Request — garbage method BLA", code, 400, reason, "Bad Request")


def test_409():
    # unlink() on a directory returns EISDIR(error is dir) → 409 Conflict
    subdir = f"{UPLOAD_DIR}/test_subdir"
    os.makedirs(subdir, exist_ok=True)
    try:
        code, reason, _ = _request("DELETE", "/testfiles/test_subdir")
        _check("409 Conflict — DELETE on a directory", code, 409, reason, "Conflict")
    finally:
        try:
            os.rmdir(subdir)
        except OSError:
            pass


def test_500():
    # Make upload dir read-only → write fails → 500 Internal Server Error
    upload_dir = UPLOAD_DIR
    os.makedirs(upload_dir, exist_ok=True)
    try:
        os.chmod(upload_dir, stat.S_IRUSR | stat.S_IXUSR)  # 500 — no write permission
        code, reason, _ = _request(
            "POST", "/testfiles",
            headers={"X-Filename": "wontwork.txt", "Content-Type": "text/plain"},
            body=b"data",
        )
        _check("500 Internal Server Error — upload dir not writable", code, 500, reason, "Internal Server Error")
    finally:
        os.chmod(upload_dir, 0o755)


def test_408():
    # Connect and send nothing — server closes idle connection after 60 s → 408
    print("  NOTE  test_408 waits ~61 s for the idle timeout to fire …", flush=True)
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(70)
    s.connect((HOST, PORT))
    data = b""
    try:
        while True:
            chunk = s.recv(256)
            if not chunk:
                break
            data += chunk
            if b"\r\n\r\n" in data:
                break
    except socket.timeout:
        pass
    finally:
        s.close()
    first_line = data.split(b"\r\n")[0].decode(errors="replace")
    parts = first_line.split(" ", 2)
    code   = int(parts[1]) if len(parts) >= 3 else None
    reason = parts[2]      if len(parts) >= 3 else first_line
    _check("408 Request Timeout — idle connection", code, 408, reason, "Request Timeout")

# ── main ─────────────────────────────────────────────────────────────────────

TESTS = [
    test_200,
    test_201,
    test_204,
    test_400,
    test_403,
    test_404,
    test_405,
    test_413,
    test_415,
    test_501,
    test_400_garbage_method,
    test_409,
    test_500,
    test_504,
    test_408,   # slow — 61 s, always last
]

if __name__ == "__main__":
    if not os.path.exists("./webserv"):
        print("ERROR: ./webserv not found — run make first")
        sys.exit(1)

    # Clean up leftover files from previous runs so upload names are predictable
    for f in [f"{UPLOAD_DIR}/test_upload.txt", f"{UPLOAD_DIR}/test_delete.txt"]:
        try:
            os.remove(f)
        except OSError:
            pass

    print(f"Starting webserv ({CONFIG}) on {HOST}:{PORT} …")
    server = _start_server()
    print("Server ready.\n")

    try:
        for t in TESTS:
            t()
    finally:
        _stop_server(server)
        # Clean up uploaded test files
        for f in [f"{UPLOAD_DIR}/test_upload.txt", f"{UPLOAD_DIR}/test_delete.txt"]:
            try:
                os.remove(f)
            except OSError:
                pass

    print(f"\n{'=' * 42}")
    total = _passed + _failed
    print(f"  {total} tests: {_passed} passed, {_failed} failed")
    print(f"{'=' * 42}")
    sys.exit(0 if _failed == 0 else 1)
