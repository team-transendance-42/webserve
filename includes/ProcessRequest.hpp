#pragma once

#include "Client.hpp"
#include "CgiExecutor.hpp"

#include <sys/stat.h>

class ProcessRequest {
public:
    ProcessRequest(const ServerConfig &config);

    void handle(Client &client) const;

private:
    const Location *_resolveLocationOrError(const HttpRequest &req, Client &client) const;
    bool _validateLocationRulesOrError(const HttpRequest &req, const Location &loc, Client &client) const;
    bool _handleRedirectIfNeeded(const Location &loc, Client &client) const;
    std::string _resolveFilePath(const Location &loc, const std::string &request_path) const;
    bool _resolvePathStatOrError(const std::string &filepath, Client &client, struct stat &st) const;
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
                        const std::string &url_path,
                        const std::string &filepath,
                        const struct stat &st,
                        Client &client) const;

    const ServerConfig &_config;
};
