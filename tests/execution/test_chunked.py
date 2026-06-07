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


def send_chunked_raw(host, port, path, raw_chunks):
    """Send a chunked POST with hand-crafted raw chunk framing
    (lets us include chunk-size extensions, odd casing, etc.)."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))

    request = f"POST {path} HTTP/1.1\r\nHost: {host}:{port}\r\nTransfer-Encoding: chunked\r\n\r\n"
    sock.sendall(request.encode())
    sock.sendall(raw_chunks)

    sock.settimeout(5)
    response = b""
    try:
        while True:
            data = sock.recv(8192)
            if not data:
                break
            response += data
    except socket.timeout:
        pass
    sock.close()
    return response.decode(errors="replace")

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

# Test 4: Chunk-size lines carrying extensions (";key=value") must be parsed
# by hex size only — the extensions are metadata and must not end up in the body.
print("[TEST] Chunk-size extensions are parsed and stripped from the body")
raw = (
    b"5;ext=ignored\r\nHello\r\n"
    b"6;foo=bar;baz=qux\r\n World\r\n"
    b"0\r\n\r\n"
)
resp = send_chunked_raw("localhost", 8080, "/cgi-bin/env_dump.py", raw)
assert "200" in resp, f"Failed: {resp}"
assert "body=Hello World" in resp, f"chunk extensions leaked into or corrupted the body: {resp}"
assert "ext=ignored" not in resp and "foo=bar" not in resp, f"chunk extension text leaked into the body: {resp}"
print("✓ Passed")

# Test 5: Many small chunks must reassemble into the exact original body —
# catches off-by-one / boundary bugs that a single big chunk would hide.
print("[TEST] Many small chunks reassemble into the exact original body")
pieces = [f"part{i:03d}-" for i in range(200)]
expected_body = "".join(pieces)
resp = send_chunked("localhost", 8080, "/cgi-bin/env_dump.py", pieces)
assert "200" in resp, f"Failed: {resp}"
assert f"body={expected_body}" in resp, "reassembled body does not match what was sent (corruption/truncation across chunk boundaries)"
print("✓ Passed")

print("\nAll chunked tests passed")
