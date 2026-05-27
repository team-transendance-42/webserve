#!/usr/bin/env python3
# Sleeps longer than the server's 5000ms CGI timeout to trigger 504.
# Never produces output — the server kills it and returns 504 Gateway Timeout.
import time
time.sleep(10)
print("Status: 200 OK")
print("Content-Type: text/plain")
print()
print("this line should never be sent")
