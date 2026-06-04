#!/usr/bin/env python3
"""
CGI session demo for webserv.

Demonstrates the server's cookie support end to end:
  - the browser's `Cookie:` request header reaches us as the HTTP_COOKIE env var
    (webserv exports every request header as HTTP_*),
  - we emit a `Set-Cookie:` response header that webserv forwards back verbatim.

Session state is persisted in a small SQLite database (one row per session).
SQLite is an in-process library — no separate server process — so it links
straight into this Python interpreter and reads/writes a single .db file.

First visit  -> mint a uuid4 session id, set the cookie, store visits = 1.
Reload       -> the browser returns the cookie, we look the id up and ++visits.
"""
import os
import sys
import uuid
import html
import sqlite3
from http.cookies import SimpleCookie, CookieError

# Keep the DB next to this script so it works regardless of webserv's CWD
# (webserv does not chdir into the script directory before exec).
DB_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sessions.db")

# Sessions older than this (seconds since last_seen) are treated as expired.
SESSION_TTL = 3600


def read_cookie_sid():
    """Return the `sid` cookie value sent by the browser, or None."""
    raw = os.environ.get("HTTP_COOKIE", "")
    if not raw:
        return None
    try:
        jar = SimpleCookie()
        jar.load(raw)
    except CookieError:
        return None
    morsel = jar.get("sid")
    return morsel.value if morsel else None


def open_db():
    conn = sqlite3.connect(DB_PATH, timeout=5)
    conn.execute(
        "CREATE TABLE IF NOT EXISTS sessions ("
        "  sid       TEXT PRIMARY KEY,"
        "  visits    INTEGER NOT NULL,"
        "  created   TEXT NOT NULL DEFAULT (datetime('now')),"
        "  last_seen TEXT NOT NULL DEFAULT (datetime('now'))"
        ")"
    )
    return conn


def main():
    conn = open_db()

    sid = read_cookie_sid()
    is_new = False
    visits = 0

    row = None
    if sid:
        row = conn.execute(
            "SELECT visits FROM sessions"
            " WHERE sid = ? AND last_seen > datetime('now', ?)",
            (sid, "-%d seconds" % SESSION_TTL),
        ).fetchone()

    if row is None:
        # No cookie, unknown id, or an expired session -> start fresh.
        sid = str(uuid.uuid4())
        is_new = True
        visits = 1
        conn.execute("INSERT OR REPLACE INTO sessions (sid, visits) VALUES (?, 1)", (sid,))
    else:
        visits = row[0] + 1
        conn.execute(
            "UPDATE sessions SET visits = ?, last_seen = datetime('now') WHERE sid = ?",
            (visits, sid),
        )

    conn.commit()
    conn.close()

    # ---- response headers ----
    out = sys.stdout
    out.write("Status: 200 OK\r\n")
    out.write("Content-Type: text/html; charset=utf-8\r\n")
    if is_new:
        # Path=/ -> sent for the whole site; HttpOnly -> not readable from JS;
        # Max-Age -> the browser drops it after SESSION_TTL seconds.
        out.write("Set-Cookie: sid=%s; Path=/; HttpOnly; Max-Age=%d\r\n" % (sid, SESSION_TTL))
    out.write("\r\n")

    # ---- body ----
    safe_sid = html.escape(sid)
    out.write("<!DOCTYPE html>\n<html><head><title>webserv session demo</title></head><body>\n")
    out.write("<h1>webserv &mdash; session demo</h1>\n")
    if is_new:
        out.write("<p><strong>New session.</strong> A cookie was just set in your browser.</p>\n")
    else:
        out.write("<p><strong>Returning visitor.</strong> Your browser sent the session cookie back.</p>\n")
    out.write("<p>Session ID: <code>%s</code></p>\n" % safe_sid)
    out.write("<p>Visit count for this session: <strong>%d</strong></p>\n" % visits)
    out.write("<p>Reload to increment the counter. Clear the <code>sid</code> cookie to start over.</p>\n")
    out.write("</body></html>\n")


if __name__ == "__main__":
    main()
