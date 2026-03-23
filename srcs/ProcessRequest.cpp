#include <cerrno>
#include <iostream>
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
    if (loc.redirect_code != 0) {
        if (loc.redirect_code == 301)
            client.writeBuf = HttpResponse::make_301(loc.redirect_url).serialize();
        else
            client.writeBuf = HttpResponse::make_302(loc.redirect_url).serialize();
        return true;
    }
    return false;
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
    req.debug_print();
    client.keep_alive = req.is_keep_alive();

    if (req.method == UNKNOWN) {
        client.writeBuf = HttpResponse::make_400().serialize();
        return;
    }

    const Location *loc = _resolveLocationOrError(req, client);
    if (!loc) return;

    if (!_validateLocationRulesOrError(req, *loc, client)) return;
    if (_handleRedirectIfNeeded(*loc, client)) return;

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
