#include <sstream>
#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <cctype> // isdigit
#include "../includes/HttpRequest.hpp"

HttpRequest::HttpRequest() : method(UNKNOWN), _state(REQUEST_LINE), _headerCount(0) {}

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
    // MAX_BODY_SIZE is enforced per-location in ProcessRequest, not globally
    // to allow large uploads in designated locations
    const size_t MAX_BODY_SIZE = 268435456; // 256 MB (soft limit, per-location hard limit applies)

    if (_buf.size() > MAX_HEADER_SIZE && _state == HEADERS) {
        _state = ERROR;
        return PARSE_ERROR;
    }
    if (content_length() > MAX_BODY_SIZE) {
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

            case HEADERS: {
                if (!_line_ready()) return INCOMPLETE;
                if (_buf.size() > MAX_HEADER_SIZE) {
                    _state = ERROR;
                    return PARSE_ERROR;
                }
                std::string line = _next_line();
                if (line.empty()) {
                    // blank line = end of headers
                    size_t contentLen = content_length();

                    if (contentLen == (size_t)-1) {
                        std::cout << "[400] Header: Content-Length is not a number\n";
                        _state = ERROR;
                        return PARSE_ERROR;
                    }
                    if (contentLen > MAX_BODY_SIZE) {
                        std::cout << "[400] Header: Content-Length exceeds parser cap\n";
                        _state = ERROR;
                        return PARSE_ERROR;
                    }
                    
                    _state = (contentLen > 0) ? BODY : DONE;
                } else {
                    // Cap header count independently of total size: many tiny headers
                    // (e.g. "A:1\n" x 4000) fit within MAX_HEADER_SIZE but each creates
                    // a std::map heap node, enabling memory exhaustion DoS.
                    if (++_headerCount > 100) {
                        _state = ERROR;
                        return PARSE_ERROR;
                    }
                    if (!_parse_header_line(line)) {
                        _state = ERROR;
                        return PARSE_ERROR;
                    }
                }
                break;
            }
            case BODY: {
                size_t contentLen = content_length();
                if (_buf.size() < contentLen) return INCOMPLETE;
                body = _buf.substr(0, contentLen);
                _buf.erase(0, contentLen);
                _state = DONE;
                break;
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
    if (!_parsePath(p))      return false;
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
bool HttpRequest::_parsePath(const std::string &raw) {
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
// todo: do i want a static func or in .hpp?
static bool is_valid_header_name(const std::string &key) {
    if (key.empty())
        return false;

    for (size_t i = 0; i < key.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(key[i]);

        if (!std::isalnum(c) &&
            c != '!' && c != '#' && c != '$' && c != '%' && c != '&' &&
            c != '\'' && c != '*' && c != '+' && c != '-' && c != '.' &&
            c != '^' && c != '_' && c != '`' && c != '|' && c != '~') {
            return false;
        }
    }
    return true;
}

static bool is_valid_header_value(const std::string &value) {
    for (size_t i = 0; i < value.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(value[i]);

        if (c == '\r' || c == '\n')
            return false;

        if ((c < 32 && c != '\t') || c == 127)
            return false;
    }
    return true;
}

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
 * 
 * lambda function used as a predicate for std::all_of.
[](unsigned char c){ return std::isdigit(c); }
[]: Introduces a lambda (an anonymous function).
(unsigned char c): Takes one argument, c, of type unsigned char.
{ return std::isdigit(c); }: Returns true if c is a digit (0-9), false otherwise.
 */
bool HttpRequest::_parse_header_line(const std::string &line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return false;

    std::string key   = line.substr(0, colon);
    std::string value = line.substr(colon + 1);

    //if (key.find('\r') != std::string::npos || key.find('\n') != std::string::npos ||
    //    value.find('\r') != std::string::npos || value.find('\n') != std::string::npos) {
    //    return false; // Reject header with CRLF injection
    //}

    //size_t s = value.find_first_not_of(" \t");
    //size_t e = value.find_last_not_of(" \t\r\n");
    //value = (s == std::string::npos) ? "" : value.substr(s, e - s + 1);

    //headers[_to_lower(key)] = value;
    //return true;
	size_t s = value.find_first_not_of(" \t");
    size_t e = value.find_last_not_of(" \t\r\n");
    value = (s == std::string::npos) ? "" : value.substr(s, e - s + 1);

    if (!is_valid_header_name(key))
        return false;

    if (!is_valid_header_value(value))
        return false;

    headers[_to_lower(key)] = value;
    return true;
}

// ── helpers ───────────────────────────────────────────────────────────────────

std::string HttpRequest::getHeader(const std::string &key) const {
    std::map<std::string,std::string>::const_iterator it
        = headers.find(_to_lower(key));
    return (it == headers.end()) ? "" : it->second;
}

bool HttpRequest::hasHeader(const std::string &key) const {
    return headers.count(_to_lower(key)) > 0;
}

// Parses the Content-Length header and returns its value as size_t.
// Returns 0 if the header is absent (no body expected).
// Returns (size_t)-1 (INVALID) if the value is non-numeric, overflows, or
// exceeds size_t max — callers treat INVALID as a 400 parse error.
// errno is cleared before strtoull so ERANGE is reliably set on overflow
// and not masked by a leftover errno from a previous syscall.
size_t HttpRequest::content_length() const {
    static const size_t INVALID = static_cast<size_t>(-1);
    std::string val = getHeader("content-length");
    if (val.empty()) return 0;
    if (!std::all_of(val.begin(), val.end(), [](unsigned char c){ return std::isdigit(c); }))
        return INVALID;
    char *end;
    errno = 0;
    unsigned long long n = std::strtoull(val.c_str(), &end, 10);
    if (*end != '\0' || errno == ERANGE || n > static_cast<unsigned long long>(INVALID - 1))
        return INVALID;
    return static_cast<size_t>(n);
}

bool HttpRequest::is_keep_alive() const {
    std::string conn = getHeader("connection");
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
    _headerCount = 0;
    //_buf.clear(); might chunks from next req
}

ParseResult HttpRequest::tryParse() {
	if (_buf.empty()) return INCOMPLETE;
    	return _parse();
}

bool HttpRequest::_line_ready() const {
    return _buf.find('\n') != std::string::npos;  //nginx-like permissive behavior: also accept \n todo: double check what we choose: \r\n or \n too?
}

std::string HttpRequest::_next_line() {
    size_t pos = _buf.find('\n');
    std::string line = _buf.substr(0, pos);
    _buf.erase(0, pos + 1);
    // strip trailing \r if present
    if (!line.empty() && line[line.size() - 1] == '\r')
        line.erase(line.size() - 1);
    return line;
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
//std::string HttpRequest::_next_line() {
//    size_t pos       = _buf.find("\r\n");
//    std::string line = _buf.substr(0, pos);
//    _buf.erase(0, pos + 2);
//    return line;
//}

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

bool HttpRequest::isComplete() const {
    return _state == DONE;
}

bool HttpRequest::hasStarted() const {
    return !_buf.empty() || _state != REQUEST_LINE;
}