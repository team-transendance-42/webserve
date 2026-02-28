#include "../includes/HttpRequest.hpp"
#include <sstream>
#include <algorithm>  // std::transform
#include <cctype>     // std::tolower

// ─────────────────────────────────────────────
// Construction / feed
// ─────────────────────────────────────────────

HttpRequest::HttpRequest(const std::string& raw)
{
    if (!raw.empty())
        feed(raw);
}

void HttpRequest::feed(const std::string& chunk)
{
    if (_complete) return; // already done, ignore extra data
    _raw += chunk;
    _checkComplete();
}

// ─────────────────────────────────────────────
// _checkComplete()
// Returns true (and sets _complete) when we have:
//   - the full header block (\r\n\r\n)
//   - AND body bytes equal to Content-Length (0 if header absent)
// ─────────────────────────────────────────────

bool HttpRequest::_checkComplete()
{
    static const std::string SEP = "\r\n\r\n";

    size_t sepPos = _raw.find(SEP);
    if (sepPos == std::string::npos)
        return false; // headers not fully received yet

    // Parse headers exactly once
    if (!_headersParsed)
    {
        _parse();
        _headersParsed = true;
    }

    size_t bodyStart  = sepPos + SEP.size();
    size_t bodyAvail  = _raw.size() - bodyStart;

    if (bodyAvail >= _contentLength)
    {
        _body     = _raw.substr(bodyStart, _contentLength);
        _complete = true;
    }

    return _complete;
}

// ─────────────────────────────────────────────
// _parse()  — called once when \r\n\r\n found
// ─────────────────────────────────────────────

void HttpRequest::_parse()
{
    size_t sepPos   = _raw.find("\r\n\r\n");
    std::string headerSection = _raw.substr(0, sepPos);

    std::istringstream stream(headerSection);
    std::string firstLine;
    std::getline(stream, firstLine);

    // strip trailing \r if present (getline eats \n, not \r)
    if (!firstLine.empty() && firstLine.back() == '\r')
        firstLine.pop_back();

    _parseRequestLine(firstLine);

    // Read the rest as headers
    std::string headerBlock;
    std::getline(stream, headerBlock, '\0'); // read rest of stream
    _parseHeaders(headerBlock);

    _splitUri();

    // Content-Length
    std::string cl = header("content-length");
    if (!cl.empty())
    {
        try { _contentLength = static_cast<size_t>(std::stoul(cl)); }
        catch (...) { _contentLength = 0; }
    }
}

// ─────────────────────────────────────────────
// _parseRequestLine()  e.g. "GET /path HTTP/1.1"
// ─────────────────────────────────────────────

void HttpRequest::_parseRequestLine(const std::string& line)
{
    std::istringstream ss(line);
    if (!(ss >> _method >> _uri >> _version))
        throw ParseError("Malformed request line: [" + line + "]");

    // Basic method validation
    if (_method != "GET"    && _method != "POST"   &&
        _method != "DELETE" && _method != "PUT"    &&
        _method != "HEAD"   && _method != "OPTIONS")
        throw ParseError("Unsupported method: " + _method);
}

// ─────────────────────────────────────────────
// _parseHeaders()  — "Key: Value\r\n" lines
// ─────────────────────────────────────────────

void HttpRequest::_parseHeaders(const std::string& block)
{
    std::istringstream ss(block);
    std::string line;

    while (std::getline(ss, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty()) continue;

        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string key   = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        // trim leading space from value
        size_t start = value.find_first_not_of(' ');
        value = (start == std::string::npos) ? "" : value.substr(start);

        // lowercase the key
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char c){ return std::tolower(c); });

        _headers[key] = value;
    }
}

// ─────────────────────────────────────────────
// _splitUri()  "/path?key=val" → _path + _query
// ─────────────────────────────────────────────

void HttpRequest::_splitUri()
{
    size_t q = _uri.find('?');
    if (q == std::string::npos)
    {
        _path  = _uri;
        _query = "";
    }
    else
    {
        _path  = _uri.substr(0, q);
        _query = _uri.substr(q + 1);
    }
}

// ─────────────────────────────────────────────
// header()  — case-insensitive lookup (keys stored lowercase)
// ─────────────────────────────────────────────

std::string HttpRequest::header(const std::string& key) const
{
    std::string lower = key;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    auto it = _headers.find(lower);
    return (it != _headers.end()) ? it->second : "";
}

// ─────────────────────────────────────────────
// dump()  — human-readable debug string
// ─────────────────────────────────────────────

std::string HttpRequest::dump() const
{
    std::ostringstream os;
    os << "Method:  " << _method  << "\n"
       << "URI:     " << _uri     << "\n"
       << "Path:    " << _path    << "\n"
       << "Query:   " << _query   << "\n"
       << "Version: " << _version << "\n"
       << "Headers:\n";
    for (auto& [k, v] : _headers)
        os << "  [" << k << "]: " << v << "\n";
    os << "Body (" << _body.size() << " bytes): " << _body << "\n";
    return os.str();
}