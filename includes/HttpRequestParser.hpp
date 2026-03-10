#pragma once
#include "HttpRequest.hpp"

enum ParseState {
    PARSE_REQUEST_LINE,
    PARSE_HEADERS,
    PARSE_BODY,
    PARSE_COMPLETE,
    PARSE_ERROR
};

class HttpRequestParser {
public:
    HttpRequestParser();

    // feed raw bytes from socket — call repeatedly as data arrives
    ParseState  feed(const std::string &data);
    ParseState  feed(const char *data, size_t len);

    bool        is_complete() const;
    bool        has_error()   const;

    HttpRequest &get_request();
    void         reset();

private:
    HttpRequest  _req;
    ParseState   _state;
    std::string  _buffer;   // accumulates raw bytes across multiple reads

    // internal parsers — each returns true on success
    bool _parse_request_line(const std::string &line);
    bool _parse_header_line (const std::string &line);
    bool _parse_body        ();
    bool _parse_method      (const std::string &token);
    bool _parse_path        (const std::string &raw_path);

    // helpers
    std::string _next_line  ();   // pulls one \r\n-terminated line from _buffer
    bool        _line_ready () const;
};