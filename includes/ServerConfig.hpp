#pragma once

#include <string>
#include <vector>
#include <map>
#include <iostream> // for debug logging

struct Location {
    std::string              path;
    std::string              root;
    std::string              index;
    std::vector<std::string> allowedMethod;
    bool                     autoindex            = false;
    bool                     denyAll             = false; // for guarding sensitive locations
    int                      redirect_code        = 0;   // 0 = no redirect
    std::string              redirect_url;
    long                     clientMaxBodySize= -1;  // -1 = inherit from server
    // todo: upload handling
	bool                     upload_enabled = false;
	std::string              upload_path; // absolute path on server to save uploaded files, e.g. "/var/www/uploads";
    std::vector<std::string>  upload_allowed_types; // e.g. {".jpg", ".png", ".pdf"}
	// std::vector<CgiConfig>   cgi;
};

// todo: hard coded values for now, to be replaced by filename.conf parser
struct ServerConfig {
    std::string              	host                 = "127.0.0.1";
    int                      	port                 = 0;
    long                     	clientMaxBodySize= 1048576;  // 1MB default
    std::vector<std::string> 	server_names;
    std::map<int,std::string>	errorPages;
    bool                      	default_server = false;
    std::vector<Location>    	locations;

    // find longest matching location for a URI
    const Location *matchLocation(const std::string &uri) const;
};

// todo: placeholder to be replaced by filename.conf parser
ServerConfig createDefaultServerConfig();

// Returns all server blocks — swap this call for parseConfigFile() when parser is ready
std::vector<ServerConfig> createDefaultServerConfigs();

