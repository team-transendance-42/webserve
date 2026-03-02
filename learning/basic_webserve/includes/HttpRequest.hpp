#pragma once

#include <string>
#include <map>
#include <stdexcept>

/*
** HttpRequest
** -----------
** Parses a raw HTTP/1.1 request string (everything up to and including
** the blank line after headers, plus body if Content-Length is present).
**
** Usage:
**   HttpRequest req(rawString);
**   req.method()   → "GET"
**   req.uri()      → "/index.html"
**   req.header("content-type") → "text/html"   (keys lowercased)
**   req.body()     → ""  (or POST payload)
**   req.isComplete() → true once \r\n\r\n + full body received
**
** Throws HttpRequest::ParseError on malformed request line.
*/

class HttpRequest
{
public:
    // Thrown on structural parse failures (bad request line etc.)
    // inherits from std::runtime_error so you can catch as std::exception if you like
    struct      ParseError : public std::runtime_error {
        explicit ParseError(const std::string& msg)
            : std::runtime_error(msg) {}
    };

    // Feed raw bytes as they arrive from read().
    // Call isComplete() after each feed — only parse once complete.
    explicit    HttpRequest(const std::string& raw = "");

    // Append more data (chunked reads from poll loop)
    void        feed(const std::string& chunk);

    // True once we have headers + full body (if any)
    bool        isComplete() const { return _complete; }

    // Accessors — only valid after isComplete()
    const std::string& method()  const { return _method;  }
    const std::string& uri()     const { return _uri;     }
    const std::string& version() const { return _version; }
    const std::string& body()    const { return _body;    }
    const std::string& path()    const { return _path;    } // uri without query string
    const std::string& query()   const { return _query;   } // everything after '?'

    // Header lookup — keys are stored lowercase
    // Returns "" if header not present
    std::string header(const std::string& key) const;

    // Convenience
    bool        hasBody()       const { return !_body.empty(); }
    size_t      contentLength() const { return _contentLength; }

    // Debug
    std::string dump() const;

private:
    void _parse();
    void _parseRequestLine(const std::string& line);
    void _parseHeaders(const std::string& block);
    void _splitUri();
    bool _checkComplete();

    std::string                        _raw;          // accumulated bytes
    std::string                        _method;
    std::string                        _uri;
    std::string                        _path;
    std::string                        _query;
    std::string                        _version;
    std::map<std::string, std::string> _headers;      // lowercase keys
    std::string                        _body;
    size_t                             _contentLength = 0;
    bool                               _headersParsed = false;
    bool                               _complete      = false;
};