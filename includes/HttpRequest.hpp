#pragma once

#include <string>
#include <map>

// global — used by Server/Client to check status
enum Method      { GET, POST, DELETE, UNKNOWN };
enum ParseResult { INCOMPLETE, COMPLETE, PARSE_ERROR };


/**
 * _buf is the parser’s internal byte queue.
Every time recv() gets bytes from socket, feed() appends them into _buf.
_buf stores raw, not-yet-fully-consumed bytes.

TCP(Transmission Control Protocol) is a byte stream, not message packets.
One recv() may return half a request.
Or one recv() may return one full request plus part/all of the next one.
HTTP keep-alive reuses same connection.
Client can send request #2 on same socket after request #1.
Those bytes can arrive before the parser finishes processing #1.
HTTP pipelining (or fast clients) can send multiple requests back-to-back.
Example stream in one recv():
REQ1_HEADERS + REQ1_BODY + REQ2_HEADERS...
After parsing REQ1, leftover bytes belong to REQ2, so they stay in _buf.
 */
class HttpRequest {
public:
    // ── parsed data — read these after feed() returns COMPLETE ────────────
    Method                             method;
    std::string                        path;
    std::string                        query_string;
    std::string                        version;
    std::map<std::string, std::string> headers;
    std::string                        body;

    HttpRequest();

    /**
	 * parse raw HTTP request bytes coming from the client socket(TCP stream bytes), in recv(client.fd, ...) in server read loop
	 * feed() appends them to _buf, then _parse() extracts fields.
	 */
    ParseResult feed(const std::string &data);
    ParseResult feed(const char *data, size_t len);  // convenience overload

    // helpers
    std::string get_header    (const std::string &key) const;
    bool        has_header    (const std::string &key) const;
    size_t      content_length()                        const;
    bool        is_keep_alive ()                        const;

    void        clear();        // reset for next request (keep-alive)
    void        debug_print()   const;

private:
    // internal parse state
    enum ParseState { REQUEST_LINE, HEADERS, BODY, DONE, ERROR };

    ParseState  _state;
    std::string _buf;

    ParseResult _parse();
    bool        _parse_request_line(const std::string &line);
    bool        _parse_header_line (const std::string &line);
    bool        _parse_method      (const std::string &tok);
    bool        _parse_path        (const std::string &raw);

    std::string _next_line   ();
    bool        _line_ready  () const;
    std::string _to_lower    (const std::string &s) const;
};