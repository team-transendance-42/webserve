#pragma once

#include "Client.hpp"
#include "ServerConfig.hpp"

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
    void _serveFromStat(const Location &loc,
                        const std::string &url_path,
                        const std::string &filepath,
                        const struct stat &st,
                        Client &client) const;

    const ServerConfig &_config;
};
