// HttpRequestParser.cpp
#include "../includes/HttpRequestParser.hpp"
#include <sstream>
#include <algorithm>
#include <iostream>

HttpRequestParser::HttpRequestParser() : _state(PARSE_REQUEST_LINE) {}

// ── public feed ───────────────────────────────────────────────────────────────

ParseState HttpRequestParser::feed(const char *data, size_t len) {
    _buffer.append(data, len);
    return feed("");
}

ParseState HttpRequestParser::feed(const std::string &data) {
    if (!data.empty())
        _buffer += data;

    // keep processing as long as there's something to parse
    while (_state != PARSE_COMPLETE && _state != PARSE_ERROR) {

        if (_state == PARSE_REQUEST_LINE) {
            if (!_line_ready()) break;          // wait for more data
            std::string line = _next_line();
            if (!_parse_request_line(line)) {
                _state = PARSE_ERROR;
                break;
            }
            _state = PARSE_HEADERS;
        }

        else if (_state == PARSE_HEADERS) {
            if (!_line_ready()) break;
            std::string line = _next_line();

            if (line.empty()) {
                // blank line = end of headers
                // decide: do we expect a body?
                if (_req.content_length() > 0)
                    _state = PARSE_BODY;
                else
                    _state = PARSE_COMPLETE;
            } else {
                if (!_parse_header_line(line)) {
                    _state = PARSE_ERROR;
                    break;
                }
            }
        }

        else if (_state == PARSE_BODY) {
            if (!_parse_body()) break;   // wait for more data
            _state = PARSE_COMPLETE;
        }
    }
    return _state;
}

// ── private: request line ─────────────────────────────────────────────────────
//  "POST /upload?user=42 HTTP/1.1"

bool HttpRequestParser::_parse_request_line(const std::string &line) {
    std::istringstream ss(line);
    std::string method_str, raw_path, version;

    if (!(ss >> method_str >> raw_path >> version))
        return false;

    if (!_parse_method(method_str)) return false;
    if (!_parse_path(raw_path))     return false;

    // validate version
    if (version != "HTTP/1.0" && version != "HTTP/1.1")
        return false;

    _req.version = version;
    return true;
}

bool HttpRequestParser::_parse_method(const std::string &token) {
    if      (token == "GET")    _req.method = GET;
    else if (token == "POST")   _req.method = POST;
    else if (token == "DELETE") _req.method = DELETE;
    else {
        _req.method = UNKNOWN;
        return false;
    }
    return true;
}

bool HttpRequestParser::_parse_path(const std::string &raw) {
    // split on '?' to separate path from query string
    size_t q = raw.find('?');
    if (q != std::string::npos) {
        _req.path         = raw.substr(0, q);
        _req.query_string = raw.substr(q + 1);
    } else {
        _req.path         = raw;
        _req.query_string = "";
    }
    if (_req.path.empty() || _req.path[0] != '/')
        return false;
    return true;
}

// ── private: header line ──────────────────────────────────────────────────────
//  "Content-Type: application/json"

bool HttpRequestParser::_parse_header_line(const std::string &line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return false;

    std::string key   = line.substr(0, colon);
    std::string value = line.substr(colon + 1);

    // trim whitespace
    auto trim = [](std::string &s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        size_t end   = s.find_last_not_of(" \t\r\n");
        s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    };
    trim(key);
    trim(value);

    // store keys lowercase → case-insensitive lookup
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    if (key.empty()) return false;

    _req.headers[key] = value;
    return true;
}

// ── private: body ─────────────────────────────────────────────────────────────

bool HttpRequestParser::_parse_body() {
    size_t expected = _req.content_length();
    if (_buffer.size() < expected)
        return false;   // haven't received full body yet

    _req.body = _buffer.substr(0, expected);
    _buffer.erase(0, expected);
    return true;
}

// ── private: buffer helpers ───────────────────────────────────────────────────

bool HttpRequestParser::_line_ready() const {
    return _buffer.find("\r\n") != std::string::npos;
}

std::string HttpRequestParser::_next_line() {
    size_t pos = _buffer.find("\r\n");
    if (pos == std::string::npos) return "";

    std::string line = _buffer.substr(0, pos);
    _buffer.erase(0, pos + 2);   // consume line + \r\n
    return line;
}

// ── public helpers ────────────────────────────────────────────────────────────

bool        HttpRequestParser::is_complete() const { return _state == PARSE_COMPLETE; }
bool        HttpRequestParser::has_error()   const { return _state == PARSE_ERROR; }
HttpRequest &HttpRequestParser::get_request()      { return _req; }

void HttpRequestParser::reset() {
    _req.clear();
    _state = PARSE_REQUEST_LINE;
    _buffer.clear();
}