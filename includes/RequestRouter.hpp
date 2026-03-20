#pragma once

#include "ServerConfig.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

class RequestRouter {
public:
    explicit RequestRouter(const ServerConfig &config);

    const Location *matchLocation(const std::string &path) const;
    bool isMethodAllowed(const Location &loc, Method method) const;
    bool hasRedirect(const Location &loc) const;
    HttpResponse makeRedirectResponse(const Location &loc) const;

private:
    const ServerConfig &_config;
    static std::string _methodToString(Method method);
};
