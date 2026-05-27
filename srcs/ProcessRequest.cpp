#include <cerrno>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

#include "../includes/CgiExecutor.hpp"
#include "../includes/ErrorResponseBuilder.hpp"
#include "../includes/ProcessRequest.hpp"
#include "../includes/StaticFileHandler.hpp"
#include "../includes/UploadHandler.hpp"
#include "../includes/config/Config.hpp"


/* Stores all server configs; the correct one is selected per-request via _selectConfig. */
ProcessRequest::ProcessRequest(const std::vector<ServerConfig> &configs) : _configs(configs) {}

/* Picks the best matching ServerConfig by comparing the Host header against server_names;
   falls back to the default_server or first config. */
const ServerConfig &ProcessRequest::_selectConfig(const HttpRequest &req) const {
    if (_configs.size() == 1) return _configs[0];

    std::string host = req.getHeader("host");

    size_t colon = host.rfind(':');
    if (colon != std::string::npos) {
        std::string portPart = host.substr(colon + 1);
        bool allDigits = !portPart.empty();
        for (size_t i = 0; i < portPart.size(); ++i)
            if (!isdigit(portPart[i])) { allDigits = false; break; }
        if (allDigits) host = host.substr(0, colon);
    }
    for (size_t i = 0; i < host.size(); ++i)
        host[i] = tolower(host[i]);

    for (size_t i = 0; i < _configs.size(); ++i) {
        const std::vector<std::string> &names = _configs[i].server_names;
        for (size_t j = 0; j < names.size(); ++j) {
            std::string lname = names[j];
            for (size_t k = 0; k < lname.size(); ++k) lname[k] = tolower(lname[k]);
            if (lname == host) return _configs[i];
        }
    }
    for (size_t i = 0; i < _configs.size(); ++i)
        if (_configs[i].default_server) return _configs[i];
    return _configs[0];
}

/**
 * Resolves the best-matching Location for the request path.
 * If no match is found, writes a 404 error response to the client and returns NULL.
 * The debug print is commented out to avoid excessive logging in production, but can be enabled for troubleshooting.
 */
const Location *ProcessRequest::_resolveLocationOrError(const HttpRequest &req, Client &client, const ServerConfig &cfg) const {
        // std::cerr << "[DEBUG] Incoming request path: '" << req.path << "'" << std::endl;
    const Location *loc = cfg.matchLocation(req.path);
    if (!loc) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(404, cfg).serialize(); /* converts it into raw HTTP text */
        return NULL;
    }
    return loc;
}

/* Applies deny/method/body-size rules and writes error responses on failure. */
bool ProcessRequest::_validateLocationRulesOrError(const HttpRequest &req, const Location &loc, Client &client, const ServerConfig &cfg) const {
    // denyAll first: protected locations return 403 regardless of method; 403 Forbidden
    if (loc.denyAll) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(403, cfg).serialize();
        return false;
    }

    std::string reqMethod = ProcessRequest::methodToString(req.method);
    bool allowed = false;
    for (size_t i = 0; i < loc.allowedMethod.size(); i++) {
        if (loc.allowedMethod[i] == reqMethod) {
            allowed = true;
            break;
        }
    }
    if (!allowed) {
        // RFC 9110 §15.5.6: 405 MUST include an Allow header listing permitted methods.
        std::string allow;
        for (size_t i = 0; i < loc.allowedMethod.size(); i++) {
            if (i > 0) allow += ", ";
            allow += loc.allowedMethod[i];
        }
        HttpResponse r = ErrorResponseBuilder::buildErrorResponse(405, cfg);
        r.setHeader("Allow", allow);
        client.writeBuf = r.serialize();
        return false;
    }

    long maxBody = (loc.clientMaxBodySize >= 0) ? loc.clientMaxBodySize : cfg.clientMaxBodySize;
    if ((long)req.body.size() > maxBody) { // 413 payload too large
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(413, cfg).serialize();
        return false;
    }

    return (true);
}

/* Returns a redirect response when the location has a 301 or 302 redirect rule. */
bool ProcessRequest::_handleRedirectIfNeeded(const Location &loc, Client &client) const {
    if (loc.redirect_code == 301 || loc.redirect_code == 302) {
        client.writeBuf = HttpResponse::make_redirect(loc.redirect_code, loc.redirect_url).serialize();
        return true;
    }
    return (false);
}

/* Sanitizes the raw filename: trims whitespace, strips path prefixes, and collapses
   repeated extensions (e.g. "a.png.png" -> "a.png"). */
static std::string normalizeUploadFilename(const std::string &rawName) {
    std::string name = rawName;

    // Trim common surrounding whitespace.
    size_t start = name.find_first_not_of(" \t\r\n");
    size_t end = name.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    name = name.substr(start, end - start + 1);

    // Keep basename only for user agents that send full paths.
    size_t slash = name.find_last_of("/\\"); // Handle both Unix and Windows separators; returns the index of the last occurrence of either character, or std::string::npos
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

/* Writes content to upload_path/<filename>, creating the directory if needed.
   Appends _N suffix to avoid overwriting existing files. */
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

    std::string safeName = filename; // not to modify the original filename for error messages
    std::string fullPath = baseDir + "/" + safeName;
    size_t dot = safeName.find_last_of('.');
    std::string stem = (dot == std::string::npos) ? safeName : safeName.substr(0, dot);
    std::string ext = (dot == std::string::npos) ? "" : safeName.substr(dot);

    int suffix = 1;
    while (stat(fullPath.c_str(), &dst) == 0) {
        fullPath = baseDir + "/" + stem + "_" + std::to_string(suffix++) + ext;
    }

    std::ofstream out(fullPath.c_str(), std::ios::binary | std::ios::out); // The | symbol is the bitwise OR operator. When used with file open modes like std::ios::binary and std::ios::out, it combines them into a single set of flags. This tells the file stream to open the file with both options enabled: binary mode and output (write) mode.
    if (!out.is_open()) return false;

    out.write(content.data(), static_cast<std::streamsize>(content.size())); //the number of bytes to write, cast to the required type. std::streamsize is a signed integer type defined by the C++ standard library to represent the number of characters or bytes to read or write in a stream operation. It is used for sizes and counts in stream I/O functions, ensuring compatibility with very large files. It is typically at least as large as long, and is used for specifying buffer sizes in functions like write() and read().
    if (!out.good()) { // return false if there was a write error, disk full, or other stream problem.
        out.close();
        return false;
    }
    out.close();
    savedPath = fullPath;
    return true;
}

/**
 * Handles file upload requests for a given location.
 *
 * - Checks if upload is enabled and method is POST.
 * - Extracts filename and content from the request (supports simple and multipart forms).
 * - Normalizes the filename (trims, strips path, collapses extensions).
 * - Validates filename and content are not empty.
 * - Attempts to save the file in the configured upload directory, avoiding name collisions.
 * - On success, responds with 201 Created and the saved path; on failure, responds with an error.
 * - Returns true if the request was handled (upload attempted), false otherwise.
 */
bool ProcessRequest::_handleUploadIfNeeded(const HttpRequest &req,
                                           const Location &loc,
                                           Client &client,
                                           const ServerConfig &cfg) const {
    if (loc.upload_path.empty() || req.method != POST) return false;

    std::string filename;
    std::string content;
    std::string contentType = req.getHeader("content-type");

    if (contentType.find("multipart/form-data") == 0) {
        // Multipart parsing is not implemented; reject with 415 so the client
        // knows to use a plain POST with X-Filename instead of a form upload.
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(415, cfg).serialize();
        return true;
    } else {
        filename = req.getHeader("x-filename"); // Custom header for simple uploads; in a real implementation, this would depend on the client-side upload method.
        content = req.body;
    }

    filename = normalizeUploadFilename(filename);

    if (filename.empty()) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(400, cfg).serialize();
        return true;
    }

    if (content.empty()) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(400, cfg).serialize();
        return true;
    }

    std::string savedPath;
    if (!_saveUpload(loc, filename, content, savedPath)) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(500, cfg).serialize();
        return true;
    }

    std::string body = "<html><body><h1>201 Created</h1><p>Saved: " + savedPath + "</p></body></html>";
    HttpResponse response;
    response.setStatus(201).setBody(body, "text/html");
    client.writeBuf = response.serialize();
    return true;
}

/* Handles DELETE requests: validates path stays within the location root,
   refuses to delete the configured index file, and responds 204/404/403/500. */
bool ProcessRequest::_handleDeleteIfNeeded(const HttpRequest &req,
                                           const Location &loc,
                                           Client &client,
                                           const ServerConfig &cfg) const {
    if (req.method != DELETE) return false;

    std::string urlPath = req.path;

    // Step 1 hardening: accept only /files_auto/<single-safe-filename>
    if (urlPath.size() <= loc.path.size() || urlPath[loc.path.size()] != '/') {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(400, cfg).serialize();
        return true;
    }

    std::string filename = urlPath.substr(loc.path.size() + 1);

    // Reject empty names, dot/dot-dot, and sub-directory references.
    if (filename.empty() || filename == "." || filename == ".." ||
        filename.find('/') != std::string::npos) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(400, cfg).serialize();
        return true;
    }

    // Never allow deleting the configured index file for this location.
    if (!loc.index.empty() && filename == loc.index) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(403, cfg).serialize();
        return true;
    }

    /* Canonicalize the root only (it must exist). Appending the validated
       filename with string concat avoids calling realpath() on the full path,
       which would return NULL for nonexistent files and produce a false 400. */
    char canonRootBuf[PATH_MAX];
    if (!realpath(loc.root.c_str(), canonRootBuf)) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(500, cfg).serialize();
        return true;
    }
    std::string filepath = std::string(canonRootBuf) + "/" + filename;

    if (unlink(filepath.c_str()) == 0) {
        HttpResponse response;
        response.setStatus(204)
                .setHeader("Content-Length", "0");
        client.writeBuf = response.serialize();
    } else if (errno == ENOENT) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(404, cfg).serialize();
    } else if (errno == EACCES || errno == EPERM) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(403, cfg).serialize();
    } else if (errno == EISDIR) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(409, cfg).serialize();
    } else {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(500, cfg).serialize();
    }
    return true;
}

/* Resolves rawPath to a canonical absolute path and verifies it stays within root.
 Uses realpath() to expand symlinks and collapse ./../ sequences.
 Returns the canonical path on success, or "" if outside root or path doesn't exist.
 An empty return maps to a 404 in all callers. */
std::string ProcessRequest::_canonicalizeWithinRoot(const std::string &root,
                                                    const std::string &rawPath) {
    char canonRootBuf[PATH_MAX];
    if (!realpath(root.c_str(), canonRootBuf))
        return "";
    std::string canonRoot(canonRootBuf);

    char canonBuf[PATH_MAX];
    if (!realpath(rawPath.c_str(), canonBuf))
        return "";
    std::string canon(canonBuf);

    if (canon.size() < canonRoot.size() ||
        canon.compare(0, canonRoot.size(), canonRoot) != 0 ||
        (canon.size() > canonRoot.size() && canon[canonRoot.size()] != '/'))
        return "";

    return canon;
}

/* Given a location and request path, build the absolute file path to serve.
 Example:
   loc.path = "/delete_create_file", loc.root = "./www/files", loc.index = "index.html"
   requestPath = "/delete_create_file/foo.txt"
   => returns "./www/files/foo.txt"
   requestPath = "/delete_create_file"
   => returns "./www/files/index.html"
   */
std::string ProcessRequest::_resolveFilePath(const Location &loc, const std::string &requestPath) const {
    std::string resolved;
    if (requestPath == loc.path) {
        resolved = loc.root + "/" + loc.index;
    } else {
        std::string suffix = requestPath.substr(loc.path.length());
        if (suffix.empty() || suffix[0] != '/')
            suffix = "/" + suffix;
        std::string root = loc.root;
        if (!root.empty() && root[root.size() - 1] == '/')
            root.erase(root.size() - 1);
        resolved = root + suffix;
    }
    return _canonicalizeWithinRoot(loc.root, resolved);
}

/* Stats the resolved path and maps filesystem errors to HTTP error responses. */
bool ProcessRequest::_resolvePathStatOrError(const std::string &filepath,
                                             Client &client,
                                             struct stat &st,
                                             const ServerConfig &cfg) const {
    if (stat(filepath.c_str(), &st) != 0) {
        if (errno == EACCES) {
            client.writeBuf = ErrorResponseBuilder::buildErrorResponse(403, cfg).serialize();
            return (false);
        }
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(404, cfg).serialize();
        return false;
    }

    // client.writeBuf = fromCgi.serialize(); TODO
    return (true);
}

/* Serves file content or directory index/autoindex fallback for the resolved path. */
void ProcessRequest::_serveFromStat(const Location &loc,
                                    const std::string &urlPath,
                                    const std::string &filepath,
                                    const struct stat &st,
                                    Client &client,
                                    const ServerConfig &cfg) const {
    if (S_ISDIR(st.st_mode) && !urlPath.empty() && urlPath[urlPath.size() - 1] != '/') {
        client.writeBuf = HttpResponse::make_redirect(301, urlPath + "/").serialize();
        return;
    }

    if (!S_ISDIR(st.st_mode)) {
        HttpResponse fileResponse = StaticFileHandler::serveStatic(filepath);
        if (fileResponse.statusCode >= 400)
            client.writeBuf = ErrorResponseBuilder::buildErrorResponse(fileResponse.statusCode, cfg).serialize();
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
            client.writeBuf = ErrorResponseBuilder::buildErrorResponse(indexResponse.statusCode, cfg).serialize();
        else
            client.writeBuf = indexResponse.serialize();
        return;
    }

    if (loc.autoindex) {
        client.writeBuf = StaticFileHandler::autoindex(filepath, urlPath).serialize();
        return;
    }

    client.writeBuf = ErrorResponseBuilder::buildErrorResponse(403, cfg).serialize();
}

/* Orchestrates full request handling: validates method/rules, dispatches to
   upload/delete/CGI/static handlers, and injects the Connection header. */
void ProcessRequest::handle(Client &client) const {
    HttpRequest &req = client.request;
    // req.debugPrint();
    client.keep_alive = req.is_keep_alive();

    if (req.method == UNKNOWN) {
        client.writeBuf = HttpResponse::make_501().serialize();
        client.keep_alive = false;
        HttpResponse::injectConnectionHeader(client.writeBuf, false);
        return;
    }

    const ServerConfig &cfg = _selectConfig(req);

    // Check for required Host header in HTTP/1.1
    if (req.version == "HTTP/1.1" && !req.hasHeader("Host")) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(400, cfg).serialize();
        client.keep_alive = false;
        HttpResponse::injectConnectionHeader(client.writeBuf, false);
        return;
    }

    const Location *loc = _resolveLocationOrError(req, client, cfg);
    if (!loc) {
        client.keep_alive = false;
        HttpResponse::injectConnectionHeader(client.writeBuf, false);
        return;
    }

    if (!_validateLocationRulesOrError(req, *loc, client, cfg)) {
        client.keep_alive = false;
        HttpResponse::injectConnectionHeader(client.writeBuf, false);
        return;
    }

    if (_handleRedirectIfNeeded(*loc, client)) {
        HttpResponse::injectConnectionHeader(client.writeBuf, client.keep_alive);
        return;
    }

    if (_handleUploadIfNeeded(req, *loc, client, cfg)) {
        HttpResponse::injectConnectionHeader(client.writeBuf, client.keep_alive);
        return;
    }

    if (_handleDeleteIfNeeded(req, *loc, client, cfg)) {
        HttpResponse::injectConnectionHeader(client.writeBuf, client.keep_alive);
        return;
    }

    std::string urlPath  = req.path;
    std::string filepath = _resolveFilePath(*loc, urlPath);

    struct stat st;
    if (!_resolvePathStatOrError(filepath, client, st, cfg)) {
        client.keep_alive = false;
        HttpResponse::injectConnectionHeader(client.writeBuf, false);
        return;
    }

    // Check if this is a CGI executable before serving as static
    if (!S_ISDIR(st.st_mode) && _shouldExecuteCgi(*loc, filepath)) {
        if (_executeCgiOrError(req, *loc, filepath, client)) {
            HttpResponse::injectConnectionHeader(client.writeBuf, client.keep_alive);
            return;
        }
    }

    _serveFromStat(*loc, urlPath, filepath, st, client, cfg);
    HttpResponse::injectConnectionHeader(client.writeBuf, client.keep_alive);
}

/* converts enum values to std::string */
std::string ProcessRequest::methodToString(Method method) {
    switch (method) {
        case GET: return "GET";
        case POST: return "POST";
        case DELETE: return "DELETE";
        default: return "";
    }
}

/* Returns true if filepath ends with the location's cgi_extension. */
bool ProcessRequest::_shouldExecuteCgi(const Location &loc, const std::string &filepath) const {
    if (loc.cgi_extension.empty()) {
        return false;
    }
    
    // Check if filepath ends with cgi_extension (e.g., ".py")
    if (filepath.size() >= loc.cgi_extension.size()) {
        std::string ext = filepath.substr(filepath.size() - loc.cgi_extension.size());
        return ext == loc.cgi_extension;
    }
    
    return false;
}

/* Runs the CGI script for the request; writes 500/504 on failure/timeout,
   or the parsed CGI output as the HTTP response. Always returns true. */
bool ProcessRequest::_executeCgiOrError(const HttpRequest &req,
                                        const Location &loc,
                                        const std::string &filepath,
                                        Client &client) const {
    CgiRequest cgiReq = _buildCgiRequest(req, filepath);
    CgiExecutor executor;
    CgiResult result = executor.execute(cgiReq, loc);
    
    // timed_out must be checked before !success: a timeout sets success=false,
    // so checking success first would swallow the timeout and return 500 instead of 504.
    if (result.timed_out) {
        client.writeBuf = HttpResponse::make_504().serialize();
        return true;
    }

    if (!result.success) {
        client.writeBuf = HttpResponse::make_500().serialize();
        return true;
    }
    
    // Parse CGI output into HTTP response
    HttpResponse response;
    if (!_buildHttpResponseFromCgiOutput(result.raw_output, response)) {
        // If parsing failed, treat output as plain text body
        response.setStatus(200)
                .setBody(result.raw_output, "text/plain");
    }
    
    client.writeBuf = response.serialize();
    return true;
}

/* Populates a CgiRequest from the incoming HttpRequest and resolved file path. */
CgiRequest ProcessRequest::_buildCgiRequest(const HttpRequest &req,
                                            const std::string &filepath) const {
    CgiRequest cgiReq;
    cgiReq.method = methodToString(req.method);
    cgiReq.script_path = filepath;
    cgiReq.script_name = req.path;
    cgiReq.query_string = req.query_string;
    cgiReq.body = req.body;
    
    // Get Content-Type header and Content-Length from body
    std::string contentType = req.getHeader("content-type");
    cgiReq.content_type = (contentType.empty() ? "" : contentType);
    
    cgiReq.server_protocol = req.version;
    
    // Parse host header to get server_name and server_port
    std::string host = req.getHeader("host");
    size_t colon = host.find(':');
    if (colon != std::string::npos) {
        cgiReq.server_name = host.substr(0, colon);
        cgiReq.server_port = host.substr(colon + 1);
    } else {
        cgiReq.server_name = host;
        cgiReq.server_port = "80"; // default HTTP port
    }
    
    cgiReq.remote_addr = "127.0.0.1"; // localhost for now
    cgiReq.path_info = ""; // PATH_INFO for extra path after script
    cgiReq.headers = req.headers;
    
    return cgiReq;
}
/*cpp way (in c it is static func) private for this file*/
namespace {
	/* Find the separator and split into headers + body. No parsing. */
	bool _splitCgiOutput(const std::string &raw,std::string &headers, std::string &body) {
		size_t pos = raw.find("\r\n\r\n");
		if (pos != std::string::npos) {
			headers = raw.substr(0, pos);
			body	= raw.substr(pos + 4);
			return true;
		}
		pos = raw.find("\n\n");
		if (pos != std::string::npos) {
			headers = raw.substr(0, pos);
			body	= raw.substr(pos + 2);
			return true;
		}
		return false;
	}

	/* Parses one already-split line and apply it to the response */
	void _applyCgiHeaderLine(const std::string &line, HttpResponse &response, bool &statusSet) {
		size_t colonPos = line.find(':');
		if (colonPos == std::string::npos) return;
		std::string key = line.substr(0, colonPos);
		std::string val = (colonPos + 1 < line.size()) ? line.substr(colonPos + 1) : "";
		if (!val.empty() && val[0] == ' ') val.erase(0, 1);
		if (key == "Status") {
			response.setStatus(atoi(val.c_str()));
			statusSet = true;
		} else if (key != "Content-Length")
			response.setHeader(key, val);
	}

	/* Iterate lines (handle both \r\n and \n via getline + \r strip), call _applyCgiHeaderLine, sets 200 fallback. */
	void _applyCgiHeaders(const std::string &headerSection, HttpResponse &response) {
		std::istringstream iss(headerSection);
		std::string line;
		bool statusSet = false;
		while (std::getline(iss, line)) {
			if (!line.empty() && line[line.size() - 1] == '\r')
				line.erase(line.size() - 1);
			_applyCgiHeaderLine(line, response, statusSet);
		}
		if (!statusSet)
			response.setStatus(200);
	}
}

bool ProcessRequest::_buildHttpResponseFromCgiOutput(const std::string &raw, HttpResponse &response) const {
	std::string headerSection, body;
	if (!_splitCgiOutput(raw, headerSection, body)) return false;
	response.setBody(body, "text/html");
	_applyCgiHeaders(headerSection, response);
	return true;
}
    
