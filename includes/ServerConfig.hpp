// includes/ServerConfig.hpp
#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>
#include <vector>
#include <map>

struct CgiConfig {
    std::string extension;   // e.g. ".php", ".py"
    std::string interpreter; // e.g. "/usr/bin/python3"
};


struct Location {
    std::string              path;
    std::string              root;
	std::string              alias;   
    std::string              index;
    std::vector<std::string> allowed_methods;
	std::string              redirect;
    bool                     autoindex            = false;
    long                     client_max_body_size = -1;  // -1 = inherit from server
    int                      redirect_code        = 0;   // 0 = no redirect
	bool                     upload_enabled;
	std::string              upload_path;
    std::string              redirect_url;
	std::vector<CgiConfig>   cgi;
};

// hard-coded: are those default values to fall into or?
struct ServerConfig {
    std::string              	host                 = "127.0.0.1";
    int                      	port                 = 0;
    long                     	client_max_body_size = 1048576;  // 1MB default
    std::vector<std::string> 	server_names;
    std::map<int,std::string>	error_pages;
    bool                      	default_server; 
    std::vector<Location>    	locations;

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

// todo: placeholder to be replaced by filename.conf parser
ServerConfig createDefaultServerConfig();

#endif
