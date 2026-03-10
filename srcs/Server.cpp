//todo

/**
 * // in your socket read loop
char buf[4096];
HttpRequestParser parser;

while (true) {
    int bytes = recv(client_fd, buf, sizeof(buf), 0);
    if (bytes <= 0) break;

    ParseState state = parser.feed(buf, bytes);

    if (state == PARSE_COMPLETE) {
        HttpRequest &req = parser.get_request();
        req.print();                     // debug
        // → pass to your router
        // router.handle(req, client_fd);
        parser.reset();                  // ready for next request
    }
    else if (state == PARSE_ERROR) {
        // send 400 Bad Request
        std::string err = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(client_fd, err.c_str(), err.size(), 0);
        break;
    }
    // else PARSE_INCOMPLETE → loop back, read more bytes
}
```

---

## Flow summary
```
recv() bytes from socket
       │
       ▼
parser.feed(buf, bytes)
       │
       ├─ appends to internal _buffer
       │
       ├─ state == REQUEST_LINE → parse "GET /path HTTP/1.1"
       │
       ├─ state == HEADERS      → parse "Key: Value" lines until blank line
       │
       ├─ state == BODY         → read exactly Content-Length bytes
       │
       └─ state == COMPLETE     → HttpRequest is ready → pass to router

	   The parser is incremental — it handles the case where TCP splits the request across multiple recv() calls, which absolutely happens in real usage.
 * 
 */