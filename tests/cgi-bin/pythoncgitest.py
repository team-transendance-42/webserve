#!/usr/bin/env python3
import os
import sys

# run directly as test:
#REQUEST_METHOD=GET QUERY_STRING="foo=bar" python3 tests/cgi-bin/pythoncgitest.py
# then press ctr+d

#echo "hello" | REQUEST_METHOD=POST QUERY_STRING="foo=bar" python3 tests/cgi-bin/pythoncgitest.py


body = sys.stdin.read()
print("Status: 200 OK")
print("Content-Type: text/plain")
print()
print("method=" + os.getenv("REQUEST_METHOD", ""))
print("query=" + os.getenv("QUERY_STRING", ""))
print("body=" + body)
print()
print("This is a test of the Python CGI script.")
print("If you see this message, the CGI script is working correctly.")
