#include <sstream>
#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <cctype> // isdigit
#include "../includes/HttpRequest.hpp"

/* path, query_string, version, headers,_buf and body are default-initialized to empty by their own constructors */
HttpRequest::HttpRequest() : method(UNKNOWN), _state(REQUEST_LINE), _headerCount(0) {}

// ── public feed ───────────────────────────────────────────────────────────────

/* Feeds data into the HTTP request parser; 
Sometimes, data arrives as a raw buffer (const char*), such as from a socket read.
var.append(..) is for ptr to char array(we use this in ConnectionManager)
_buf += data; is used when you have a std::string. It appends the entire string
*/
ParseResult HttpRequest::feed(const char *data, size_t len) {
    _buf.append(data, len);
    return _parse();
}

// ── internal parse loop ───────────────────────────────────────────────────────
/**
limit for header size: protects for buffer overflow attacks
limit for body size: protects for DoS attacks( Denial of Service: an attacker tries to make a service unavailable to normal users, usually by overwhelming it with too many requests or exhausting resources.) with large payloads
*/
ParseResult HttpRequest::_parse() {
    const size_t MAX_HEADER_SIZE = 8192; // 8 KB
    const size_t MAX_BODY_SIZE = 10 * 1024 * 1024; // 10 MB parser safety cap; policy 413 is enforced later in ProcessRequest: prevents memory excaustion from malicious clients sending huge bodies; can be adjusted based on server capacity and expected use cases

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
                if (_buf.size() > MAX_HEADER_SIZE) {
                    /* without \n, the parser never consumes any data from _buf,
                       so the buffer just keeps growing until the size limit catches it. */
                    _state = ERROR;
                    return PARSE_ERROR;
                }
                if (!_line_ready()) return INCOMPLETE;
                std::string line = _next_line();
                if (line.empty()) {
                    // blank line = end of headers
                    if (headers.count("transfer-encoding")) {
                        /* Chunked (and any TE) not supported — reject cleanly
                           rather than misreading the body as zero-length. */
                        _state = ERROR;
                        return PARSE_ERROR;
                    }

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
    if (!(ss >> m >> p >> v)) return false; // stream extraction operator (>>) used with input streams (like std::cin(char input, usually the keyboard), std::ifstream, std::istringstream) to extract (read) data from the stream into variables. 
    if (!_parse_method(m))    return false;
    if (!_parsePath(p))      return false;
    if (v != "HTTP/1.0" && v != "HTTP/1.1") return false;
    version = v;
    return true;
}

/**
 * setting up method field
 * tok(token)
 */
bool HttpRequest::_parse_method(const std::string &tok) {
    if      (tok == "GET")    { method = GET;    return true; }
    else if (tok == "POST")   { method = POST;   return true; }
    else if (tok == "DELETE") { method = DELETE; return true; }

    /* Real HTTP verbs this server doesn't implement → UNKNOWN → 501 in handle().
       Anything else (garbage like "BLA") → false → PARSE_ERROR → 400. */
    static const char *known[] = {
        "PUT", "PATCH", "HEAD", "OPTIONS", "TRACE", "CONNECT", NULL
    };
    for (int i = 0; known[i]; ++i) {
        if (tok == known[i]) { method = UNKNOWN; return true; }
    }
    return false;
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
/* checks that every character in the header name is allowed by the HTTP specification */
static bool is_valid_header_name(const std::string &key) {
    if (key.empty())
        return false;

    for (size_t i = 0; i < key.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(key[i]);

        if (!std::isalnum(c) && // alphanumeric characters are allowed in header names
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

        if ((c < 32 && c != '\t') || c == 127) // 32(space, <32 control chars, 127 delete)
            return false;
    }
    return true;
}

/*
    HTTP/1.0 and HTTP/1.1 share the same "Name: Value" header format.
    HTTP/1.1 (RFC 7230) defines stricter token rules for header names.
    We apply 1.1 rules to both versions — safe, simpler, no real-world compat loss.
Choices:
  Line endings : parser accepts \r\n AND bare \n (nginx-style permissive).
                 _next_line() strips the trailing \r, so this function sees clean text.
  Key charset  : RFC 7230 token chars only (is_valid_header_name). Rejects space, colon, etc.
  Value charset: is_valid_header_value rejects \r, \n, and all other control chars
                 (ASCII < 32 except \t, and DEL 127). This is the CRLF-injection guard —
                 stricter than a bare find('\r') check.
  Value trim   : leading/trailing SP and HT stripped (RFC 7230 §3.2 OWS rule).
  Key storage  : lowercased so all lookups are case-insensitive (RFC 7230 §3.2).
*/
bool HttpRequest::_parse_header_line(const std::string &line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return false;

    std::string key   = line.substr(0, colon);
    std::string value = line.substr(colon + 1);

    size_t s = value.find_first_not_of(" \t"); // index of first non-space/tab char(start)
    size_t e = value.find_last_not_of(" \t\r\n"); // index of last non-space/tab char(end)
    value = (s == std::string::npos) ? "" : value.substr(s, e - s + 1);

    if (!is_valid_header_name(key))
        return false;

    if (!is_valid_header_value(value))
        return false;

    std::string lkey = _to_lower(key);

    /* Duplicate Content-Length with a different value is a request-smuggling
       vector (RFC 9112 §6.3). Reject it. Identical duplicates are allowed. */
    if (lkey == "content-length") {
        std::map<std::string,std::string>::const_iterator it = headers.find(lkey);
        if (it != headers.end() && it->second != value)
            return false;
    }

    headers[lkey] = value;
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

/*
Parses the Content-Length header and returns its value as size_t.
Returns 0 if the header is absent (no body expected).
Returns (size_t)-1 (INVALID) if the value is non-numeric, overflows, or
exceeds size_t max — callers treat INVALID as a 400 parse error.
errno is cleared before strtoull so ERANGE is reliably set on overflow
and not masked by a leftover errno from a previous syscall.

Declaring static const inside a func avoids polluting the global or class namespace, keeps it private to the func, and ensures it’s only initialized once (not every call).
This is a common C++ pattern for function-local constants that don’t need to be global or class members.

The literal -1 is an int by default in C++.
Casting to size_t (static_cast<size_t>(-1)) converts -1 to the maximum possible value for size_t (since size_t is unsigned).
This is a common trick to represent an “invalid” or “not found” value for size_t, because size_t can never be negative.
*/
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
    if (version == "HTTP/1.1") return conn != "close"; // default = keep-alive unless "close" is set
    return conn == "keep-alive"; // HTTP/1.0 default = close unless "keep-alive" is set
}

/*
    Sets length to 0 (becomes "").
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
    //_buf.clear(); might have chunks from next req, dont clear
}

/*
check if there is any data to parse. If _buf is empty, it means no new data has arrived from the client, so the parser cannot make progress and must wait for more data.
*/
ParseResult HttpRequest::tryParse() {
	if (_buf.empty()) return INCOMPLETE;
    	return _parse();
}

/*
    Both RFC 1945 (HTTP/1.0) and RFC 7230 (HTTP/1.1) require lines to end with "\r\n" (CRLF).
    However, in practice, many clients and tools send just "\n". Major servers (nginx, Apache) accept both for compatibility, even though the standard only requires "\r\n". This is a pragmatic choice for robustness, not a requirement of either HTTP/1.0 or 1.1
*/
bool HttpRequest::_line_ready() const {
    return _buf.find('\n') != std::string::npos;
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

/*  convert a string to lowercase
    The first out.begin() and out.end() define what to read (input).
    The second out.begin() is where to write the result (output).
    This allows std::transform to copy from one place to another, or overwrite in place
*/
std::string HttpRequest::_to_lower(const std::string &s) const {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), ::tolower);
    return out;
}

/* in ProcessRequest, inspect it */
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