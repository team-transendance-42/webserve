#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <sys/stat.h>

#include "../includes/CgiExecutor.hpp"
#include "../includes/ErrorResponseBuilder.hpp"
#include "../includes/ProcessRequest.hpp"
#include "../includes/StaticFileHandler.hpp"


// Anonymous namespace (namespace { ... }) gurantees “visible only inside this .cpp file”.
namespace {
std::string methodToString(Method method) {
    switch (method) {
        case GET: return "GET";
        case POST: return "POST";
        case DELETE: return "DELETE";
        default: return "";
    }
}

std::string trim(const std::string &value) {
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    if (begin == value.size()) {
        return ("");
    }

    std::size_t end = value.size() - 1;
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end]))) {
        --end;
    }

    return (value.substr(begin, end - begin + 1));
}

std::string toLower(const std::string &value) {
    std::string out = value;
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    }
    return (out);
}

bool endsWith(const std::string &text, const std::string &suffix) {
    if (suffix.size() > text.size()) {
        return (false);
    }
    return (text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0);
}

bool parseStatusCode(const std::string &statusLine, int &statusCodeOut) {
    std::istringstream iss(statusLine);
    int code = 0;
    iss >> code;
    if (!iss || code < 100 || code > 599) {
        return (false);
    }
    statusCodeOut = code;
    return (true);
}
}

ProcessRequest::ProcessRequest(const ServerConfig &config)
    : _config(config) {}

const Location *ProcessRequest::_resolveLocationOrError(const HttpRequest &req, Client &client) const {
    const Location *loc = _config.matchLocation(req.path);
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
    // Check method first: if method not allowed, return 405
    std::string req_method = methodToString(req.method);
    bool allowed = false;
    for (size_t i = 0; i < loc.allowed_methods.size(); i++) {
        if (loc.allowed_methods[i] == req_method) {
            allowed = true;
            break;
        }
    }
    if (!allowed) {
        client.write_buf = ErrorResponseBuilder::buildErrorResponse(405, _config).serialize();
        return false;
    }

    // Then check deny_all: if access forbidden, return 403
    if (loc.deny_all == true) {
        client.write_buf = ErrorResponseBuilder::buildErrorResponse(403, _config).serialize();
        return false;
    }

    long max_body = (loc.client_max_body_size >= 0)
                    ? loc.client_max_body_size
                    : _config.client_max_body_size;
    if ((long)req.body.size() > max_body) {
        client.write_buf = HttpResponse::make_413().serialize();
        return (false);
    }

    return (true);
}

// Returns a redirect response when location has redirect rules.
bool ProcessRequest::_handleRedirectIfNeeded(const Location &loc, Client &client) const {
    if (loc.redirect_code != 0) {
        if (loc.redirect_code == 301)
            client.write_buf = HttpResponse::make_301(loc.redirect_url).serialize();
        else
            client.write_buf = HttpResponse::make_302(loc.redirect_url).serialize();
        return (true);
    }
    return (false);
}

std::string ProcessRequest::_resolveFilePath(const Location &loc,
                                             const std::string &request_path) const {
    if (request_path == loc.path)
        return loc.root + "/" + loc.index;

    return (loc.root + request_path.substr(loc.path.length()));
}

// Stats the resolved path and maps filesystem errors to HTTP errors.
bool ProcessRequest::_resolvePathStatOrError(const std::string &filepath,
                                             Client &client,
                                             struct stat &st) const {
    if (stat(filepath.c_str(), &st) != 0) {
        if (errno == EACCES) {
            client.write_buf = ErrorResponseBuilder::buildErrorResponse(403, _config).serialize();
            return (false);
        }
        client.write_buf = ErrorResponseBuilder::buildErrorResponse(404, _config).serialize();
        return (false);
    }
    return (true);
}

bool ProcessRequest::_shouldExecuteCgi(const Location &loc, const std::string &filepath) const {
    return loc.hasCgi() && endsWith(filepath, loc.cgi_extension);
}

CgiRequest ProcessRequest::_buildCgiRequest(const HttpRequest &req,
                                            const std::string &filepath) const {
    CgiRequest cgiReq;
    cgiReq.method = methodToString(req.method);
    cgiReq.script_path = filepath;
    cgiReq.script_name = req.path;
    cgiReq.query_string = req.query_string;
    cgiReq.path_info = "";
    cgiReq.body = req.body;
    cgiReq.content_type = req.get_header("content-type");
    cgiReq.server_protocol = req.version;
    cgiReq.server_name = _config.server_names.empty() ? _config.host : _config.server_names[0];
    cgiReq.server_port = std::to_string(_config.port);
    cgiReq.remote_addr = "127.0.0.1";
    cgiReq.headers = req.headers;
    return (cgiReq);
}

// Executes CGI and builds HTTP response from CGI output. On failure, writes error response to client.
bool ProcessRequest::_buildHttpResponseFromCgiOutput(const std::string &raw,
                                                     HttpResponse &response) const {
    std::size_t splitPos = raw.find("\r\n\r\n");
    std::size_t sepLen = 4;
    if (splitPos == std::string::npos) {
        splitPos = raw.find("\n\n");
        sepLen = 2;
    }

    if (splitPos == std::string::npos) {
        return (false);
    }

    const std::string headerBlock = raw.substr(0, splitPos);
    const std::string body = raw.substr(splitPos + sepLen);

    response = HttpResponse();
    response.set_status(200);

    // Parse headers line by line. If any header is malformed, return false to indicate CGI response parsing failure.
    std::size_t start = 0;
    while (start <= headerBlock.size()) {
        std::size_t end = headerBlock.find('\n', start);
        std::string line;
        if (end == std::string::npos) {
            line = headerBlock.substr(start);
            start = headerBlock.size() + 1;
        } else {
            line = headerBlock.substr(start, end - start);
            start = end + 1;
        }

        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }

        line = trim(line);
        if (line.empty()) {
            continue;
        }

        std::size_t colon = line.find(':');
        if (colon == std::string::npos) {
            return (false);
        }

        const std::string key = trim(line.substr(0, colon));
        const std::string value = trim(line.substr(colon + 1));
        if (key.empty()) {
            return (false);
        }

        if (toLower(key) == "status") {
            int statusCode = 200;
            if (!parseStatusCode(value, statusCode)) {
                return (false);
            }
            response.set_status(statusCode);
        } else if (toLower(key) != "content-length") {
            response.set_header(key, value);
        }
    }

    if (response.headers.find("Content-Type") == response.headers.end()) {
        response.set_header("Content-Type", "text/html");
    }
    response.body = body;
    response.set_header("Content-Length", std::to_string(body.size()));
    return (true);
}

bool ProcessRequest::_executeCgiOrError(const HttpRequest &req,
                                        const Location &loc,
                                        const std::string &filepath,
                                        Client &client) const {
    CgiExecutor executor;
    CgiRequest cgiReq = _buildCgiRequest(req, filepath);
    CgiResult cgiResult = executor.execute(cgiReq, loc);

    if (cgiResult.timed_out) {
        HttpResponse timeoutResp;
        timeoutResp.set_status(504).set_body("CGI execution timed out", "text/plain");
        client.write_buf = timeoutResp.serialize();
        return false;
    }

    if (!cgiResult.success) {
        HttpResponse badGateway;
        badGateway.set_status(502).set_body("CGI execution failed", "text/plain");
        client.write_buf = badGateway.serialize();
        return false;
    }

    // Parse CGI output into HTTP response. If parsing fails, return 502.
    HttpResponse fromCgi;
    if (!_buildHttpResponseFromCgiOutput(cgiResult.raw_output, fromCgi)) {
        HttpResponse badGateway;
        badGateway.set_status(502).set_body("Invalid CGI response format", "text/plain");
        client.write_buf = badGateway.serialize();
        return false;
    }

    client.write_buf = fromCgi.serialize();
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
    std::string filepath = _resolveFilePath(*loc, url_path);

    struct stat st;
    if (!_resolvePathStatOrError(filepath, client, st)) return;

    // If location has CGI config and filepath matches CGI extension, execute CGI. Otherwise serve static.
    if (_shouldExecuteCgi(*loc, filepath)) {
        if (!S_ISREG(st.st_mode)) {
            client.write_buf = ErrorResponseBuilder::buildErrorResponse(403, _config).serialize();
            return;
        }

        _executeCgiOrError(req, *loc, filepath, client);
        return;
    }

    _serveFromStat(*loc, url_path, filepath, st, client);
}
