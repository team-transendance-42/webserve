#!/usr/bin/env python3
"""
CGI script used by test_cgi.py.
Echoes the four standard CGI env vars and the request body so tests can
assert on each one independently.
"""
import os
import sys

body = sys.stdin.read()
print("Status: 200 OK")
print("Content-Type: text/plain")
print()
print("REQUEST_METHOD=" + os.getenv("REQUEST_METHOD", ""))
print("QUERY_STRING="   + os.getenv("QUERY_STRING",   ""))
print("CONTENT_TYPE="   + os.getenv("CONTENT_TYPE",   ""))
print("CONTENT_LENGTH=" + os.getenv("CONTENT_LENGTH", ""))
print("body=" + body)
