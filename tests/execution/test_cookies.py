#!/usr/bin/env python3
"""
test_cookies.py — verifies cookie / session-management support (bonus).

Assumes webserv is already running on 127.0.0.1:8080 with tests/conf/default.conf.
The /session location runs www/one/cgi-bin/session.py, which issues an `sid`
cookie on the first visit and persists a per-session visit counter in SQLite.

The server's job (and what these tests really exercise) is the cookie round-trip:
  - the `Cookie:` request header reaches the CGI as HTTP_COOKIE,
  - the CGI's `Set-Cookie:` response header is forwarded back to the client.

Run from repo root:
    python3 tests/execution/test_cookies.py
"""

import http.client
import re
import sys
from helpers import _check, require_server, finish

HOST = "127.0.0.1"
PORT = 8080
PATH = "/session"

UUID = r"[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}"
SID_RE = re.compile(r"sid=(" + UUID + r")")          # from the Set-Cookie header
BODY_SID_RE = re.compile(r"<code>(" + UUID + r")</code>")  # from the page body
VISIT_RE = re.compile(r"Visit count for this session:\s*<strong>(\d+)</strong>")


def _req(cookie=None):
    """GET /session, optionally sending an `sid` cookie. Returns (status, set_cookie, sid, visits)."""
    headers = {}
    if cookie:
        headers["Cookie"] = "sid=" + cookie
    c = http.client.HTTPConnection(HOST, PORT, timeout=10)
    c.request("GET", PATH, headers=headers)
    r = c.getresponse()
    body = r.read().decode(errors="replace")
    set_cookie = r.getheader("Set-Cookie")
    c.close()

    sid = None
    if set_cookie:
        m = SID_RE.search(set_cookie)
        sid = m.group(1) if m else None
    else:
        m = BODY_SID_RE.search(body)    # returning visit: id only appears in the body
        sid = m.group(1) if m else None

    vm = VISIT_RE.search(body)
    visits = int(vm.group(1)) if vm else None
    return r.status, set_cookie, sid, visits


def test_first_visit_sets_cookie():
    """No cookie sent -> 200, a Set-Cookie: sid=<uuid> header, visit count 1."""
    status, set_cookie, sid, visits = _req(cookie=None)
    _check("GET /session (no cookie) -> 200", status, 200)
    _check("first visit sends Set-Cookie", set_cookie is not None, True)
    _check("Set-Cookie carries a uuid sid", sid is not None, True)
    _check("first visit count is 1", visits, 1)


def test_returning_visit_increments():
    """Resending the sid -> same session, no new Set-Cookie, counter advances."""
    _, _, sid, _ = _req(cookie=None)            # mint a fresh session
    _check("minted a session id", sid is not None, True)

    status, set_cookie, sid2, visits = _req(cookie=sid)
    _check("GET /session (with cookie) -> 200", status, 200)
    _check("no Set-Cookie on returning visit", set_cookie, None)
    _check("same session id reused", sid2, sid)
    _check("second visit count is 2", visits, 2)

    _, _, _, visits3 = _req(cookie=sid)
    _check("third visit count is 3", visits3, 3)


def test_unknown_cookie_starts_new_session():
    """An sid the server never issued -> treated as a brand-new session."""
    bogus = "00000000-0000-0000-0000-000000000000"
    status, set_cookie, sid, visits = _req(cookie=bogus)
    _check("GET /session (bogus cookie) -> 200", status, 200)
    _check("unknown cookie gets a fresh Set-Cookie", set_cookie is not None, True)
    _check("unknown cookie is replaced", sid != bogus, True)
    _check("unknown cookie restarts count at 1", visits, 1)


def test_separate_clients_get_separate_sessions():
    """Two cookieless requests get two different session ids."""
    _, _, sid_a, _ = _req(cookie=None)
    _, _, sid_b, _ = _req(cookie=None)
    _check("distinct clients -> distinct sessions", sid_a != sid_b, True)


TESTS = [
    test_first_visit_sets_cookie,
    test_returning_visit_increments,
    test_unknown_cookie_starts_new_session,
    test_separate_clients_get_separate_sessions,
]


if __name__ == "__main__":
    require_server(HOST, PORT)
    print(f"Testing cookies/sessions on {HOST}:{PORT}\n")
    for t in TESTS:
        t()
    finish()
