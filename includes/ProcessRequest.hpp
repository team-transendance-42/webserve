#pragma once

#include <vector>
#include "Client.hpp"
#include "ServerConfig.hpp"
#include "CgiExecutor.hpp"

#include <sys/stat.h>

/*
* Converts a parsed HttpRequest (stored in Client) into a serialized HTTP response. (convert from in-mem cpp obj into plain text http format that can be sent over socket)
*
* Responsibilities:
*   - Match URI to Location
*   - Enforce location rules (allowed methods, denyAll, max body size)
*   - Apply redirects
*   - Resolve filesystem path and serve file/dir/autoindex responses
*
* This class does not manage sockets or epoll; Server/ConnectionManager handle I/O.
*/
class ProcessRequest {
public:
    explicit ProcessRequest(const std::vector<ServerConfig> &configs);

    void handle(Client &client) const;

private:
    const ServerConfig &_selectConfig(const HttpRequest &req) const;

    const Location *_resolveLocationOrError(const HttpRequest &req,
                                            Client &client,
                                            const ServerConfig &cfg) const;
    bool _validateLocationRulesOrError(const HttpRequest &req,
                                       const Location &loc,
                                       Client &client,
                                       const ServerConfig &cfg) const;
    bool _handleRedirectIfNeeded(const Location &loc, Client &client) const;
    bool _handleUploadIfNeeded(const HttpRequest &req,
                               const Location &loc,
                               Client &client,
                               const ServerConfig &cfg) const;
    bool _handleDeleteIfNeeded(const HttpRequest &req,
                               const Location &loc,
                               Client &client,
                               const ServerConfig &cfg) const;

    std::string         _resolveFilePath(const Location &loc,
                                         const std::string &requestPath) const;
    static std::string  _canonicalizeWithinRoot(const std::string &root,
                                                const std::string &rawPath);
    bool _resolvePathStatOrError(const std::string &filepath,
                                 Client &client,
                                 struct stat &st,
                                 const ServerConfig &cfg) const;
    bool _saveUpload(const Location &loc,
                     const std::string &filename,
                     const std::string &content,
                     std::string &savedPath) const;
    bool _shouldExecuteCgi(const Location &loc, const std::string &filepath) const;
    bool _executeCgiOrError(const HttpRequest &req,
                            const Location &loc,
                            const std::string &filepath,
                            Client &client) const;
    CgiRequest _buildCgiRequest(const HttpRequest &req,
                                const std::string &filepath) const;
    bool _buildHttpResponseFromCgiOutput(const std::string &raw,
                                         HttpResponse &response) const;
    void _serveFromStat(const Location &loc,
                        const std::string &urlPath,
                        const std::string &filepath,
                        const struct stat &st,
                        Client &client,
                        const ServerConfig &cfg) const;

    static std::string methodToString(Method method);

    std::vector<ServerConfig> _configs;
};
