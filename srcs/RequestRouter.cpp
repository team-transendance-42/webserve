#include "../includes/RequestRouter.hpp"

RequestRouter::RequestRouter(const ServerConfig &config) : _config(config) {}

const Location *RequestRouter::matchLocation(const std::string &path) const {
    return _config.matchLocation(path);
}

/**
 * Method is a plain enum
 */
bool RequestRouter::isMethodAllowed(const Location &loc, Method method) const {
    std::string req_method = _methodToString(method);
    if (req_method.empty())
        return false;

    for (size_t i = 0; i < loc.allowed_methods.size(); i++) {
        if (loc.allowed_methods[i] == req_method)
            return true;
    }
    return false;
}

bool RequestRouter::hasRedirect(const Location &loc) const {
    return loc.redirect_code != 0;
}

HttpResponse RequestRouter::makeRedirectResponse(const Location &loc) const {
    if (loc.redirect_code == 301)
        return HttpResponse::make_301(loc.redirect_url);
    return HttpResponse::make_302(loc.redirect_url);
}

std::string RequestRouter::_methodToString(Method method) {
    switch (method) {
        case GET: return "GET";
        case POST: return "POST";
        case DELETE: return "DELETE";
        default: return "";
    }
}
