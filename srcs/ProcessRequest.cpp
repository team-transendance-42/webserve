#include <cerrno>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>

#include "../includes/ErrorResponseBuilder.hpp"
#include "../includes/ProcessRequest.hpp"
#include "../includes/StaticFileHandler.hpp"

/**
 * Serialized = converting structured data into a flat byte/text format that can be sent or stored. HttpResponse is an object (status, headers, body).
.serialize() turns it into raw HTTP text
 */
ProcessRequest::ProcessRequest(const ServerConfig &config)
    : _config(config) {}

const Location *ProcessRequest::_resolveLocationOrError(const HttpRequest &req, Client &client) const {
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
        std::cerr << "[413] payload too large"
                  << " path=" << req.path
                  << " body_size=" << req.body.size()
                  << " max_allowed=" << maxBody
                  << " content_length_header=" << req.content_length()
                  << "\n";
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

bool ProcessRequest::_sanitizeFilename(std::string &filename) const {
    if (filename.empty() || filename.size() > 128) return false;
    if (filename.find("..") != std::string::npos) return false;
    if (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos) return false;

    for (size_t i = 0; i < filename.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(filename[i]);
        if (!(std::isalnum(c) || c == '.' || c == '_' || c == '-'))
            return false;
    }
    return true;
}

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

bool ProcessRequest::_extractMultipartFile(const HttpRequest &req,
                                          std::string &filename,
                                          std::string &content) const {
    std::string contentType = req.get_header("content-type");
    const std::string key = "boundary=";
    size_t bpos = contentType.find(key);
    if (bpos == std::string::npos) return false;

    std::string boundary = contentType.substr(bpos + key.size());
    if (!boundary.empty() && boundary[0] == '"' && boundary[boundary.size() - 1] == '"')
        boundary = boundary.substr(1, boundary.size() - 2);
    if (boundary.empty()) return false;

    const std::string delim = "--" + boundary;
    size_t partStart = req.body.find(delim);
    if (partStart == std::string::npos) return false;
    partStart += delim.size();
    if (req.body.compare(partStart, 2, "\r\n") == 0) partStart += 2;

    size_t headersEnd = req.body.find("\r\n\r\n", partStart);
    if (headersEnd == std::string::npos) return false;

    std::string partHeaders = req.body.substr(partStart, headersEnd - partStart);
    size_t disp = partHeaders.find("Content-Disposition:");
    if (disp == std::string::npos) disp = partHeaders.find("content-disposition:");
    if (disp == std::string::npos) return false;

    size_t fnamePos = partHeaders.find("filename=");
    if (fnamePos == std::string::npos) return false;
    fnamePos += 9;
    if (fnamePos >= partHeaders.size()) return false;

    if (partHeaders[fnamePos] == '"') {
        ++fnamePos;
        size_t endQ = partHeaders.find('"', fnamePos);
        if (endQ == std::string::npos) return false;
        filename = partHeaders.substr(fnamePos, endQ - fnamePos);
    } else {
        size_t end = partHeaders.find(';', fnamePos);
        if (end == std::string::npos) end = partHeaders.size();
        filename = partHeaders.substr(fnamePos, end - fnamePos);
    }

    size_t dataStart = headersEnd + 4;
    size_t dataEnd = req.body.find("\r\n" + delim, dataStart);
    if (dataEnd == std::string::npos) return false;
    content = req.body.substr(dataStart, dataEnd - dataStart);
    return true;
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
        if (!_extractMultipartFile(req, filename, content)) {
            client.writeBuf = ErrorResponseBuilder::buildErrorResponse(400, _config).serialize();
            return true;
        }
    } else {
        filename = req.get_header("x-filename");
        content = req.body;
    }

    filename = normalizeUploadFilename(filename);

    if (!_sanitizeFilename(filename)) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(400, _config).serialize();
        return true;
    }

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

std::string ProcessRequest::_resolveFilePath(const Location &loc,
                                             const std::string &requestPath) const {
    if (requestPath == loc.path)
        return loc.root + "/" + loc.index;

    return loc.root + requestPath.substr(loc.path.length());
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
    // req.debug_print();
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
