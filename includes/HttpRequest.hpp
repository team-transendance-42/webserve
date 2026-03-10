// HttpRequest.hpp
#pragma once
#include <string>
#include <map>

// global — used by Server/Client to check status
enum Method      { GET, POST, DELETE, UNKNOWN };
enum ParseResult { INCOMPLETE, COMPLETE, PARSE_ERROR };

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

    // feed raw bytes from recv() — call repeatedly until COMPLETE
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
    // internal parse state — never exposed outside
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