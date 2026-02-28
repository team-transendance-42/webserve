#pragma once

#include "HttpRequest.hpp"
#include "ConfigParser.hpp"
#include <string>
#include <map>

/*
** HttpResponse
** ------------
** Builds a complete HTTP/1.1 response from a parsed request + server config.
**
** Usage:
**   HttpResponse res(request, config);
**   std::string raw = res.build();   // ready to write() to the socket
**
** Handles:
**   - 200  file found and readable
**   - 403  path escapes root (directory traversal attempt)
**   - 404  file not found  (serves config.error_404 if available)
**   - 405  method not allowed by config.methods
**   - 500  file found but unreadable
**
** Content-Type is inferred from the file extension.
** Directory requests are resolved to config.index automatically.
*/

class HttpResponse
{
public:
    HttpResponse(const HttpRequest& req, const ServerConfig& config);

    // Build and return the full raw HTTP response (headers + body)
    std::string build();

    // Accessors (useful for tests without building full raw string)
    int                status()      const { return _status; }
    const std::string& body()        const { return _body;   }
    std::string        header(const std::string& key) const;

private:
    void        _resolve();           // figure out status + body
    void        _serveFile(const std::string& absPath);
    void        _serveError(int code);
    std::string _buildRaw() const;

    static std::string _mimeType(const std::string& path);
    static std::string _statusText(int code);
    static bool        _pathIsSafe(const std::string& root,
                                   const std::string& absPath);
    static std::string _readFile(const std::string& path, bool& ok);

    const HttpRequest&               _req;
    const ServerConfig&              _config;
    int                              _status = 200;
    std::string                      _body;
    std::map<std::string,std::string> _headers;
};