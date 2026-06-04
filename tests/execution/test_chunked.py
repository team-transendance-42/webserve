import socket
import sys

def send_chunked(host, port, path, chunks):
    """Send a chunked POST request."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))

    request = f"POST {path} HTTP/1.1\r\nHost: {host}:{port}\r\nTransfer-Encoding: chunked\r\n\r\n"
    sock.sendall(request.encode())

    for chunk in chunks:
        hex_len = hex(len(chunk))[2:]
        sock.sendall(f"{hex_len}\r\n".encode())
        sock.sendall(chunk.encode())
        sock.sendall(b"\r\n")

    sock.sendall(b"0\r\n\r\n")

    response = sock.recv(8192)
    sock.close()
    return response.decode()

# Test 1: Simple chunked body
print("[TEST] Simple chunked POST to existing CGI script")
resp = send_chunked("localhost", 8080, "/cgi-bin/pythoncgitest.py", ["Hello", " ", "World"])
assert "200" in resp or "simple CGI" in resp, f"Failed: {resp}"
print("✓ Passed")

# Test 2: Single chunk
print("[TEST] Single chunk")
resp = send_chunked("localhost", 8080, "/cgi-bin/pythoncgitest.py", ["Complete body"])
assert "200" in resp, f"Failed: {resp}"
print("✓ Passed")

# Test 3: Empty body (just 0\r\n\r\n)
print("[TEST] Empty chunked body")
resp = send_chunked("localhost", 8080, "/cgi-bin/pythoncgitest.py", [])
assert "200" in resp or "400" in resp, f"Failed: {resp}"
print("✓ Passed")

print("\nAll chunked tests passed")
