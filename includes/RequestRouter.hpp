#pragma once

#include "ServerConfig.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

/**
 * Validates HTTP methods and handles request redirects based on location rules.
 */
class RequestRouter {
public:
    explicit RequestRouter(const ServerConfig &config);

    bool isMethodAllowed(const Location &loc, Method method) const;

private:
    const ServerConfig &_config;
    static std::string _methodToString(Method method);
};
