#include "../includes/HttpRequest.hpp"
#include <sstream>
#include <algorithm>
#include <iostream>
#include <cstdlib>

HttpRequest::HttpRequest() : method(UNKNOWN), _state(REQUEST_LINE) {}

// ── public feed ───────────────────────────────────────────────────────────────

ParseResult HttpRequest::feed(const char *data, size_t len) {
    _buf.append(data, len);
    return _parse();
}

ParseResult HttpRequest::feed(const std::string &data) {
    _buf += data;
    return _parse();
}

// ── internal parse loop ───────────────────────────────────────────────────────
/**
limit for header size: protects for buffer overflow attacks
limit for body size: protects for DoS attacks( Denial of Service: an attacker tries to make a service unavailable to normal users, usually by overwhelming it with too many requests or exhausting resources.) with large payloads
*/
ParseResult HttpRequest::_parse() {
    const size_t MAX_HEADER_SIZE = 8192; // 8 KB
    const size_t MAX_BODY_SIZE = 10 * 1024 * 1024; // 10 MB parser safety cap; policy 413 is enforced later in ProcessRequest

    if (_buf.size() > MAX_HEADER_SIZE && _state == HEADERS) {
        _state = ERROR;
        return PARSE_ERROR;
    }
    if (content_length() > MAX_BODY_SIZE) {
        std::cerr << "[400] parser rejected request: Content-Length exceeds parser cap"
                  << " content_length=" << content_length()
                  << " parser_max=" << MAX_BODY_SIZE
                  << "\n";
        _state = ERROR;
        return PARSE_ERROR;
    }

    while (true) {
        switch (_state) {

        case REQUEST_LINE:
            if (!_line_ready()) return INCOMPLETE;
            if (!_parse_request_line(_next_line())) {
                _state = ERROR;
                return PARSE_ERROR;
            }
            _state = HEADERS;
            break;

        case HEADERS:
            if (!_line_ready()) return INCOMPLETE;
            {
                std::string line = _next_line();
                if (line.empty()) {
                    // blank line = end of headers
                    _state = (content_length() > 0) ? BODY : DONE;
                } else {
                    if (!_parse_header_line(line)) {
                        _state = ERROR;
                        return PARSE_ERROR;
                    }
                }
            }
            break;

        case BODY:
            {
                size_t expected = content_length();
                if (_buf.size() < expected) return INCOMPLETE;
                body = _buf.substr(0, expected);
                _buf.erase(0, expected);
                _state = DONE;
            }
        case DONE:
            return COMPLETE;
        case ERROR:
            return PARSE_ERROR;
        }
    }
}

// ── request line ─────────────────────────────────────────────────────────────
/**
 * sets fields: version, method, path using parse_method() and parse_path()
 */
bool HttpRequest::_parse_request_line(const std::string &line) {
    std::istringstream ss(line);
    std::string m, p, v;
    if (!(ss >> m >> p >> v)) return false;
    if (!_parse_method(m))    return false;
    if (!_parse_path(p))      return false;
    if (v != "HTTP/1.0" && v != "HTTP/1.1") return false;
    version = v;
    return true;
}

/**
 * setting up method field
 */
bool HttpRequest::_parse_method(const std::string &tok) {
    if      (tok == "GET")    method = GET;
    else if (tok == "POST")   method = POST;
    else if (tok == "DELETE") method = DELETE;
    else { method = UNKNOWN; return false; }
    return true;
}

/**
 * setting up the path field
 */
bool HttpRequest::_parse_path(const std::string &raw) {
    size_t q = raw.find('?');
    if (q != std::string::npos) {
        path         = raw.substr(0, q);
        query_string = raw.substr(q + 1);
    } else {
        path         = raw;
        query_string = "";
    }
    return (!path.empty() && path[0] == '/');
}

// ── header line ──────────────────────────────────────────────────────────────

/**
 * Parses one HTTP header line: "Key: Value".
 *
 * Steps:
 * - Finds ':' separator; if missing, the header is invalid.
 * - Splits into `key` and `value`.
 * - Rejects CR/LF inside key/value (prevents header injection).
 * - Trims leading/trailing spaces from value.
 * - Stores header using lowercase key for case-insensitive lookup.
 *
 * Returns true on success, false on malformed header.
 */
bool HttpRequest::_parse_header_line(const std::string &line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return false;

    std::string key   = line.substr(0, colon);
    std::string value = line.substr(colon + 1);

    if (key.find('\r') != std::string::npos || key.find('\n') != std::string::npos ||
        value.find('\r') != std::string::npos || value.find('\n') != std::string::npos) {
        return false; // Reject header with CRLF injection
    }

    size_t s = value.find_first_not_of(" \t");
    size_t e = value.find_last_not_of(" \t\r\n");
    value = (s == std::string::npos) ? "" : value.substr(s, e - s + 1);

    headers[_to_lower(key)] = value;
    return true;
}

// ── helpers ───────────────────────────────────────────────────────────────────

std::string HttpRequest::get_header(const std::string &key) const {
    std::map<std::string,std::string>::const_iterator it
        = headers.find(_to_lower(key));
    return (it == headers.end()) ? "" : it->second;
}

bool HttpRequest::has_header(const std::string &key) const {
    return headers.count(_to_lower(key)) > 0;
}

size_t HttpRequest::content_length() const {
    std::string val = get_header("content-length");
    return val.empty() ? 0 : static_cast<size_t>(std::atol(val.c_str()));
}

bool HttpRequest::is_keep_alive() const {
    std::string conn = get_header("connection");
    // HTTP/1.1 default = keep-alive unless "close" is set
    // HTTP/1.0 default = close unless "keep-alive" is set
    if (version == "HTTP/1.1") return conn != "close";
    return conn == "keep-alive";
}

/**
 *  Sets length to 0 (becomes "").
	Keeps the object valid and reusable.
	Usually does not guarantee freeing capacity immediately.
 */
void HttpRequest::clear() {
    method = UNKNOWN;
    path.clear();
    query_string.clear();
    version.clear();
    headers.clear();
    body.clear();
    _state = REQUEST_LINE;
    _buf.clear();
}

bool HttpRequest::_line_ready() const {
    return _buf.find("\r\n") != std::string::npos;
}

/**
 * Returns the next CRLF-terminated line from the internal buffer.
 * CR is \r (carriage return, ASCII 13) LF is \n (line feed, ASCII 10)
	In HTTP, each header line ends with \r\n, and headers end with an empty line: \r\n\r\n.
 *
 * Behavior:
 * - Finds the first "\r\n" sequence in _buf.
 * - Copies everything before it into `line`.
 * - Erases the consumed bytes plus the CRLF (pos + 2) from _buf.
 *
 * Note: Caller should ensure a full line is available first (via _line_ready()).
 */
std::string HttpRequest::_next_line() {
    size_t pos       = _buf.find("\r\n");
    std::string line = _buf.substr(0, pos);
    _buf.erase(0, pos + 2);
    return line;
}

/**
 * from cpp algorithm/transform
 *transform(InputIt first1, InputIt last1, OutputIt d_first, UnaryOperation unary_op) 
 */
std::string HttpRequest::_to_lower(const std::string &s) const {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), ::tolower);
    return out;
}

void HttpRequest::debugPrint() const {
    const char *m[] = { "GET", "POST", "DELETE", "UNKNOWN" };
    std::cout << "=== HttpRequest ===\n"
              << "  method : " << m[method]     << "\n"
              << "  path   : " << path          << "\n"
              << "  query  : " << query_string  << "\n"
              << "  version: " << version       << "\n";
    typedef std::map<std::string,std::string>::const_iterator It;
    for (It it = headers.begin(); it != headers.end(); ++it)
        std::cout << "  " << it->first << ": " << it->second << "\n";
    if (!body.empty())
        std::cout << " *** body ***  : " << body << "\n";
    std::cout << "===================\n";
}