#include "../includes/HttpResponse.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>   // std::filesystem::canonical  (C++17)
#include <sys/stat.h>   // stat()

namespace fs = std::filesystem;

// ─────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────

HttpResponse::HttpResponse(const HttpRequest& req, const ServerConfig& config)
    : _req(req), _config(config)
{}

// ─────────────────────────────────────────────
// build()  — public entry point
// ─────────────────────────────────────────────

std::string HttpResponse::build()
{
    _resolve();
    return _buildRaw();
}

// ─────────────────────────────────────────────
// _resolve()
// Decides status + body. Order of checks:
//   1. method allowed?
//   2. path traversal safe?
//   3. file/directory exists?
//   4. readable?
// ─────────────────────────────────────────────

void HttpResponse::_resolve()
{
    // 1. Method check
    const auto& allowed = _config.methods;
    bool methodOk = std::find(allowed.begin(), allowed.end(),
                              _req.method()) != allowed.end();
    if (!methodOk)
    {
        _serveError(405);
        return;
    }

    // 2. Build absolute path from root + request path
    std::string reqPath = _req.path();

    // Strip query string safety (path() already excludes '?...')
    // Prevent null bytes
    if (reqPath.find('\0') != std::string::npos)
    {
        _serveError(400);
        return;
    }

    std::string absPath = _config.root + reqPath;

    // If path ends with '/' or is a directory → append index
    struct stat st{};
    if (stat(absPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
    {
        if (absPath.back() != '/')
            absPath += '/';
        absPath += _config.index;
    }

    // 3. Traversal check — resolve symlinks/.. after we've appended index
    //    so that canonical() has a real path to work with
    try
    {
        fs::path canonical = fs::weakly_canonical(absPath);
        fs::path root      = fs::weakly_canonical(_config.root);

        if (!_pathIsSafe(root.string(), canonical.string()))
        {
            _serveError(403);
            return;
        }
        absPath = canonical.string();
    }
    catch (...)
    {
        // weakly_canonical can throw on permission errors
        _serveError(403);
        return;
    }

    // 4. Serve the file
    _serveFile(absPath);
}

// ─────────────────────────────────────────────
// _serveFile()
// ─────────────────────────────────────────────

void HttpResponse::_serveFile(const std::string& absPath)
{
    bool readable = false;
    std::string content = _readFile(absPath, readable);

    if (!readable)
    {
        // Distinguish not-found from permission error
        struct stat st{};
        if (stat(absPath.c_str(), &st) != 0)
            _serveError(404);
        else
            _serveError(500);
        return;
    }

    _status = 200;
    _body   = content;
    _headers["Content-Type"]   = _mimeType(absPath);
    _headers["Content-Length"] = std::to_string(_body.size());
}

// ─────────────────────────────────────────────
// _serveError()
// Tries to serve the configured 404 page for 404,
// falls back to a built-in HTML snippet for everything.
// ─────────────────────────────────────────────

void HttpResponse::_serveError(int code)
{
    _status = code;
    _headers["Content-Type"] = "text/html";

    // For 404 try the configured error page first
    if (code == 404 && !_config.error_404.empty())
    {
        bool ok = false;
        std::string absErr = _config.root + "/" + _config.error_404;
        std::string content = _readFile(absErr, ok);
        if (ok)
        {
            _body = content;
            _headers["Content-Length"] = std::to_string(_body.size());
            return;
        }
    }

    // Built-in fallback
    std::ostringstream oss;
    oss << "<html><body><h1>"
        << code << " " << _statusText(code)
        << "</h1></body></html>";
    _body = oss.str();
    _headers["Content-Length"] = std::to_string(_body.size());
}

// ─────────────────────────────────────────────
// _buildRaw()
// Assembles status line + headers + blank line + body
// ─────────────────────────────────────────────

std::string HttpResponse::_buildRaw() const
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << _status << " " << _statusText(_status) << "\r\n";

    for (auto& [k, v] : _headers)
        oss << k << ": " << v << "\r\n";

    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << _body;
    return oss.str();
}

// ─────────────────────────────────────────────
// header()
// ─────────────────────────────────────────────

std::string HttpResponse::header(const std::string& key) const
{
    auto it = _headers.find(key);
    return it != _headers.end() ? it->second : "";
}

// ─────────────────────────────────────────────
// _pathIsSafe()
// absPath must start with root — prevents ../../../etc/passwd
// ─────────────────────────────────────────────

bool HttpResponse::_pathIsSafe(const std::string& root,
                                const std::string& absPath)
{
    // absPath must start with root + separator
    if (absPath.size() < root.size())
        return false;
    return absPath.compare(0, root.size(), root) == 0;
}

// ─────────────────────────────────────────────
// _readFile()
// ─────────────────────────────────────────────

std::string HttpResponse::_readFile(const std::string& path, bool& ok)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) { ok = false; return ""; }
    std::ostringstream ss;
    ss << f.rdbuf();
    ok = !f.fail();
    return ss.str();
}

// ─────────────────────────────────────────────
// _mimeType()  — extension → Content-Type
// ─────────────────────────────────────────────

std::string HttpResponse::_mimeType(const std::string& path)
{
    static const std::map<std::string, std::string> types = {
        { ".html", "text/html"              },
        { ".htm",  "text/html"              },
        { ".css",  "text/css"               },
        { ".js",   "application/javascript" },
        { ".json", "application/json"       },
        { ".png",  "image/png"              },
        { ".jpg",  "image/jpeg"             },
        { ".jpeg", "image/jpeg"             },
        { ".gif",  "image/gif"              },
        { ".svg",  "image/svg+xml"          },
        { ".ico",  "image/x-icon"           },
        { ".txt",  "text/plain"             },
        { ".pdf",  "application/pdf"        },
        { ".zip",  "application/zip"        },
    };

    size_t dot = path.rfind('.');
    if (dot != std::string::npos)
    {
        std::string ext = path.substr(dot);
        auto it = types.find(ext);
        if (it != types.end())
            return it->second;
    }
    return "application/octet-stream";
}

// ─────────────────────────────────────────────
// _statusText()
// ─────────────────────────────────────────────

std::string HttpResponse::_statusText(int code)
{
    switch (code)
    {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}