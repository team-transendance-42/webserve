// includes/ServerConfig.hpp
#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>
#include <vector>
#include <map>

struct Location {
    std::string              path;
    std::string              root;
    std::string              index;
    std::string              upload_store;
    std::string              cgi_extension;
    std::string              cgi_pass;
    std::vector<std::string> allowed_methods;
    bool                     autoindex            = false;
    long                     client_max_body_size = -1;  // -1 = inherit from server
    int                      redirect_code        = 0;   // 0 = no redirect
    std::string              redirect_url;
};

struct ServerConfig {
    std::string              host                 = "127.0.0.1";
    int                      port                 = 0;
    long                     client_max_body_size = 1048576;  // 1MB default
    std::vector<std::string> server_names;
    std::map<int,std::string> error_pages;
    std::vector<Location>    locations;

    // find longest matching location for a URI
    const Location *match_location(const std::string &uri) const {
        const Location *best     = nullptr;
        size_t          best_len = 0;
        for (const auto &loc : locations) {
            if (uri.compare(0, loc.path.size(), loc.path) == 0) {
                if (loc.path.size() > best_len) {
                    best_len = loc.path.size();
                    best     = &loc;
                }
            }
        }
        return best;
    }
};

ServerConfig createDefaultServerConfig();

#endif
