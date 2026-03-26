#include <cerrno>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../includes/ErrorResponseBuilder.hpp"
#include "../includes/ProcessRequest.hpp"
#include "../includes/StaticFileHandler.hpp"

/**
 * Serialized = converting structured data(like obj) into a flat byte/text format that can be sent or stored. HttpResponse is an object (status, headers, body).
.serialize() turns it into raw HTTP text
 */
ProcessRequest::ProcessRequest(const ServerConfig &config)
    : _config(config) {}

const Location *ProcessRequest::_resolveLocationOrError(const HttpRequest &req, Client &client) const {
        // std::cerr << "[DEBUG] Incoming request path: '" << req.path << "'" << std::endl;
    const Location *loc = _config.matchLocation(req.path);
    if (!loc) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(404, _config).serialize();
        return NULL;
    }
    return loc;
}

// Applies deny/method/body-size rules and writes error responses on failure.
bool ProcessRequest::_validateLocationRulesOrError(const HttpRequest &req,
                                                   const Location &loc,
                                                   Client &client) const {
    // Check method first: if method not allowed, return 405
    std::string reqMethod = ProcessRequest::methodToString(req.method);
    bool allowed = false;
    for (size_t i = 0; i < loc.allowedMethod.size(); i++) {
        if (loc.allowedMethod[i] == reqMethod) {
            allowed = true;
            break;
        }
    }
    if (!allowed) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(405, _config).serialize();
        return false;
    }

    // Then check denyAll: if access forbidden, return 403
    if (loc.denyAll == true) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(403, _config).serialize();
        return false;
    }

    long maxBody = (loc.clientMaxBodySize>= 0)
                    ? loc.clientMaxBodySize
                    : _config.clientMaxBodySize;
    if ((long)req.body.size() > maxBody) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(413, _config).serialize();
        return false;
    }

    return true;
}

// Returns a redirect response when location has redirect rules.
bool ProcessRequest::_handleRedirectIfNeeded(const Location &loc, Client &client) const {
    if (loc.redirect_code == 301 || loc.redirect_code == 302) {
        client.writeBuf = HttpResponse::make_redirect(loc.redirect_code, loc.redirect_url).serialize();
        return true;
    }
    return false;
}

// bool ProcessRequest::_sanitizeFilename(std::string &filename) const {
//     return true; // Placeholder to maintain function signature
// }

static std::string normalizeUploadFilename(const std::string &rawName) {
    std::string name = rawName;

    // Trim common surrounding whitespace.
    size_t start = name.find_first_not_of(" \t\r\n");
    size_t end = name.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    name = name.substr(start, end - start + 1);

    // Keep basename only for user agents that send full paths.
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos)
        name = name.substr(slash + 1);

    // Collapse repeated trailing extension: "a.png.png" -> "a.png".
    while (true) {
        size_t dot = name.find_last_of('.');
        if (dot == std::string::npos || dot == 0) break;
        std::string ext = name.substr(dot);
        std::string base = name.substr(0, dot);
        if (base.size() >= ext.size() &&
            base.compare(base.size() - ext.size(), ext.size(), ext) == 0) {
            name = base;
            continue;
        }
        break;
    }

    return name;
}




bool ProcessRequest::_saveUpload(const Location &loc,
                                 const std::string &filename,
                                 const std::string &content,
                                 std::string &savedPath) const {
    std::string baseDir = loc.upload_path.empty() ? "./www/uploads" : loc.upload_path;

    struct stat dst;
    if (stat(baseDir.c_str(), &dst) != 0) {
        if (mkdir(baseDir.c_str(), 0755) != 0) return false;
    } else if (!S_ISDIR(dst.st_mode)) {
        return false;
    }

    std::string safeName = filename;
    std::string fullPath = baseDir + "/" + safeName;
    size_t dot = safeName.find_last_of('.');
    std::string stem = (dot == std::string::npos) ? safeName : safeName.substr(0, dot);
    std::string ext = (dot == std::string::npos) ? "" : safeName.substr(dot);

    int suffix = 1;
    while (stat(fullPath.c_str(), &dst) == 0) {
        fullPath = baseDir + "/" + stem + "_" + std::to_string(suffix++) + ext;
    }

    std::ofstream out(fullPath.c_str(), std::ios::binary | std::ios::out);
    if (!out.is_open()) return false;

    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!out.good()) {
        out.close();
        return false;
    }
    out.close();
    savedPath = fullPath;
    return true;
}

bool ProcessRequest::_handleUploadIfNeeded(const HttpRequest &req,
                                           const Location &loc,
                                           Client &client) const {
    if (!loc.upload_enabled || req.method != POST) return false;

    std::string filename;
    std::string content;
    std::string contentType = req.get_header("content-type");

    if (contentType.find("multipart/form-data") == 0) {
            // Removed _extractMultipartFile usage
    } else {
        filename = req.get_header("x-filename");
        content = req.body;
    }

    filename = normalizeUploadFilename(filename);



    if (content.empty()) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(400, _config).serialize();
        return true;
    }

    std::string savedPath;
    if (!_saveUpload(loc, filename, content, savedPath)) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(500, _config).serialize();
        return true;
    }

    std::string body = "<html><body><h1>201 Created</h1><p>Saved: " + savedPath + "</p></body></html>";
    HttpResponse response;
    response.setStatus(201)
            .setHeader("Content-Type", "text/html")
            .setHeader("Content-Length", std::to_string(body.size()))
            .setBody(body, "text/html");
    client.writeBuf = response.serialize();
    return true;
}

bool ProcessRequest::_handleDeleteIfNeeded(const HttpRequest &req,
                                           const Location &loc,
                                           Client &client) const {
    if (req.method != DELETE) return false;

    std::string urlPath = req.path;

    // Step 1 hardening: accept only /files_auto/<single-safe-filename>
    if (urlPath.size() <= loc.path.size() || urlPath[loc.path.size()] != '/') {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(400, _config).serialize();
        return true;
    }

    std::string filename = urlPath.substr(loc.path.size() + 1);
    // Never allow deleting the configured index file for this location.
    if (!loc.index.empty() && filename == loc.index) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(403, _config).serialize();
        return true;
    }

    std::string filepath = loc.root + "/" + filename;
    if (unlink(filepath.c_str()) == 0) {
        HttpResponse response;
        response.setStatus(204)
                .setHeader("Content-Length", "0");
        client.writeBuf = response.serialize();
    } else if (errno == ENOENT) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(404, _config).serialize();
    } else if (errno == EACCES || errno == EPERM) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(403, _config).serialize();
    } else {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(500, _config).serialize();
    }
    return true;
}

// Given a location and request path, build the absolute file path to serve.
// Example:
//   loc.path = "/delete_create_file", loc.root = "./www/files", loc.index = "index.html"
//   requestPath = "/delete_create_file/foo.txt"
//   => returns "./www/files/foo.txt"
//   requestPath = "/delete_create_file"
//   => returns "./www/files/index.html"
std::string ProcessRequest::_resolveFilePath(const Location &loc, const std::string &requestPath) const {
    std::string resolved;
    if (requestPath == loc.path) {
        resolved = loc.root + "/" + loc.index;
        // std::cerr << "[ProcessRequest::_resolveFilePath--1] '" << requestPath << "' => '" << resolved << "'\n";
        return resolved;
    }

    std::string suffix = requestPath.substr(loc.path.length());
    if (!suffix.empty() && suffix[0] != '/' &&
        (loc.path.empty() || loc.path[loc.path.size() - 1] != '/')) {
        suffix = "/" + suffix;
    }

    if (!loc.root.empty() && loc.root[loc.root.size() - 1] == '/' &&
        !suffix.empty() && suffix[0] == '/') {
        resolved = loc.root.substr(0, loc.root.size() - 1) + suffix;
        // std::cerr << "[ProcessRequest::_resolveFilePath--2] '" << requestPath << "' => '" << resolved << "'\n";
        return resolved;
    }

    resolved = loc.root + + "/" + suffix;
    // std::cerr << "[ProcessRequest::_resolveFilePath--3] '" << requestPath << "' => '" << resolved << "'\n";
    return resolved;
}

// Stats the resolved path and maps filesystem errors to HTTP errors.
bool ProcessRequest::_resolvePathStatOrError(const std::string &filepath,
                                             Client &client,
                                             struct stat &st) const {
    if (stat(filepath.c_str(), &st) != 0) {
        if (errno == EACCES) {
            client.writeBuf = ErrorResponseBuilder::buildErrorResponse(403, _config).serialize();
            return false;
        }
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(404, _config).serialize();
        return false;
    }
    return true;
}

// Serves file content or directory index/autoindex fallback for resolved path.
void ProcessRequest::_serveFromStat(const Location &loc,
                                    const std::string &urlPath,
                                    const std::string &filepath,
                                    const struct stat &st,
                                    Client &client) const {
    if (!S_ISDIR(st.st_mode)) {
        HttpResponse fileResponse = StaticFileHandler::serveStatic(filepath);
        if (fileResponse.statusCode >= 400)
            client.writeBuf = ErrorResponseBuilder::buildErrorResponse(fileResponse.statusCode, _config).serialize();
        else
            client.writeBuf = fileResponse.serialize();
        return;
    }

    std::string indexPath = filepath;
    if (indexPath[indexPath.size() - 1] != '/') indexPath += '/';
    indexPath += loc.index;

    struct stat ist;
    if (stat(indexPath.c_str(), &ist) == 0 && S_ISREG(ist.st_mode)) {
        HttpResponse indexResponse = StaticFileHandler::serveStatic(indexPath);
        if (indexResponse.statusCode >= 400)
            client.writeBuf = ErrorResponseBuilder::buildErrorResponse(indexResponse.statusCode, _config).serialize();
        else
            client.writeBuf = indexResponse.serialize();
        return;
    }

    if (loc.autoindex) {
        client.writeBuf = StaticFileHandler::autoindex(filepath, urlPath).serialize();
        return;
    }

    client.writeBuf = ErrorResponseBuilder::buildErrorResponse(403, _config).serialize();
}

// Orchestrates full request handling from validation to final response build.
void ProcessRequest::handle(Client &client) const {
    HttpRequest &req = client.request;
    // req.debugPrint();
    client.keep_alive = req.is_keep_alive();

    if (req.method == UNKNOWN) {
        client.writeBuf = HttpResponse::make_400().serialize();
        return;
    }

    const Location *loc = _resolveLocationOrError(req, client);
    if (!loc) return;

    if (!_validateLocationRulesOrError(req, *loc, client)) return;
    if (_handleRedirectIfNeeded(*loc, client)) return;
    if (_handleUploadIfNeeded(req, *loc, client)) return;

    if (_handleDeleteIfNeeded(req, *loc, client)) return;

    std::string urlPath = req.path;
    std::string filepath = _resolveFilePath(*loc, urlPath);

    struct stat st;
    if (!_resolvePathStatOrError(filepath, client, st)) return;

    _serveFromStat(*loc, urlPath, filepath, st, client);
}

// converts enum values to std::string
std::string ProcessRequest::methodToString(Method method) {
    switch (method) {
        case GET: return "GET";
        case POST: return "POST";
        case DELETE: return "DELETE";
        default: return "";
    }
}
