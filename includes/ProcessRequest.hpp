#pragma once

/*
* ProcessRequest
* --------------
* Converts a parsed HttpRequest (stored in Client) into a serialized HTTP response. (convert from in-mem cpp ojb into plain text http format that can be sent over socket)
*
* Responsibilities:
*   - Match URI to Location
*   - Enforce location rules (allowed methods, denyAll, max body size)
*   - Apply redirects
*   - Resolve filesystem path and serve file/dir/autoindex responses
*
* This class does not manage sockets or epoll; Server/ConnectionManager handle I/O.
*/

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
    std::string _resolveFilePath(const Location &loc, const std::string &requestPath) const;
    bool _resolvePathStatOrError(const std::string &filepath, Client &client, struct stat &st) const;
    void _serveFromStat(const Location &loc,
                        const std::string &urlPath,
                        const std::string &filepath,
                        const struct stat &st,
                        Client &client) const;
	static std::string methodToString(Method method);

    const ServerConfig &_config;
};
