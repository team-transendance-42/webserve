#include <cerrno>
#include <sys/stat.h>

#include "../includes/ErrorResponseBuilder.hpp"
#include "../includes/PathResolver.hpp"
#include "../includes/ProcessRequest.hpp"
#include "../includes/StaticFileHandler.hpp"

// Stores shared config/router references used by request-processing helpers.
ProcessRequest::ProcessRequest(const ServerConfig &config, const RequestRouter &router)
    : _config(config), _router(router) {}

// Resolves request path to a location or writes a 404 response.
const Location *ProcessRequest::_resolveLocationOrError(const HttpRequest &req, Client &client) const {
    const Location *loc = _router.matchLocation(req.path);
    if (!loc) {
        client.write_buf = ErrorResponseBuilder::buildErrorResponse(404, _config).serialize();
        return NULL;
    }
    return loc;
}

// Applies deny/method/body-size rules and writes error responses on failure.
bool ProcessRequest::_validateLocationRulesOrError(const HttpRequest &req,
                                                   const Location &loc,
                                                   Client &client) const {
    if (loc.deny_all == true) {
        client.write_buf = ErrorResponseBuilder::buildErrorResponse(403, _config).serialize();
        return false;
    }

    if (!_router.isMethodAllowed(loc, req.method)) {
        client.write_buf = HttpResponse::make_405().serialize();
        return false;
    }

    long max_body = (loc.client_max_body_size >= 0)
                    ? loc.client_max_body_size
                    : _config.client_max_body_size;
    if ((long)req.body.size() > max_body) {
        client.write_buf = HttpResponse::make_413().serialize();
        return false;
    }

    return true;
}

// Returns a redirect response when location has redirect rules.
bool ProcessRequest::_handleRedirectIfNeeded(const Location &loc, Client &client) const {
    if (_router.hasRedirect(loc)) {
        client.write_buf = _router.makeRedirectResponse(loc).serialize();
        return true;
    }
    return false;
}

// Stats the resolved path and maps filesystem errors to HTTP errors.
bool ProcessRequest::_resolvePathStatOrError(const std::string &filepath,
                                             Client &client,
                                             struct stat &st) const {
    if (stat(filepath.c_str(), &st) != 0) {
        if (errno == EACCES) {
            client.write_buf = ErrorResponseBuilder::buildErrorResponse(403, _config).serialize();
            return false;
        }
        client.write_buf = ErrorResponseBuilder::buildErrorResponse(404, _config).serialize();
        return false;
    }
    return true;
}

// Serves file content or directory index/autoindex fallback for resolved path.
void ProcessRequest::_serveFromStat(const Location &loc,
                                    const std::string &url_path,
                                    const std::string &filepath,
                                    const struct stat &st,
                                    Client &client) const {
    if (!S_ISDIR(st.st_mode)) {
        client.write_buf = StaticFileHandler::serveStatic(filepath).serialize();
        return;
    }

    std::string index_path = filepath;
    if (index_path[index_path.size() - 1] != '/') index_path += '/';
    index_path += loc.index;

    struct stat ist;
    if (stat(index_path.c_str(), &ist) == 0 && S_ISREG(ist.st_mode)) {
        client.write_buf = StaticFileHandler::serveStatic(index_path).serialize();
        return;
    }

    if (loc.autoindex) {
        client.write_buf = StaticFileHandler::autoindex(filepath, url_path).serialize();
        return;
    }

    client.write_buf = ErrorResponseBuilder::buildErrorResponse(403, _config).serialize();
}

// Orchestrates full request handling from validation to final response build.
void ProcessRequest::handle(Client &client) const {
    HttpRequest &req = client.request;
    req.debug_print();
    client.keep_alive = req.is_keep_alive();

    if (req.method == UNKNOWN) {
        client.write_buf = HttpResponse::make_400().serialize();
        return;
    }

    const Location *loc = _resolveLocationOrError(req, client);
    if (!loc) return;

    if (!_validateLocationRulesOrError(req, *loc, client)) return;
    if (_handleRedirectIfNeeded(*loc, client)) return;

    std::string url_path = req.path;
    std::string filepath = PathResolver::resolveFilePath(*loc, url_path);

    struct stat st;
    if (!_resolvePathStatOrError(filepath, client, st)) return;

    _serveFromStat(*loc, url_path, filepath, st, client);
}
