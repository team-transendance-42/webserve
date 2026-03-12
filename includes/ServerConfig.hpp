#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>
#include <vector>
#include <map>

// todo: this is placeholder for future CGI support, not used in current implementation
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
    bool                     autoindex            = false;
    int                      redirect_code        = 0;   // 0 = no redirect
    std::string              redirect_url;
    long                     client_max_body_size = -1;  // -1 = inherit from server
    // todo: upload handling
	// bool                     upload_enabled;
	// std::string              upload_path;
	// std::vector<CgiConfig>   cgi;
};

// todo: hard coded values for now, to be replaced by filename.conf parser
struct ServerConfig {
    std::string              	host                 = "127.0.0.1";
    int                      	port                 = 0;
    long                     	client_max_body_size = 1048576;  // 1MB default
    std::vector<std::string> 	server_names;
    std::map<int,std::string>	error_pages;
    bool                      	default_server; 
    std::vector<Location>    	locations;

    // find longest matching location for a URI
    const Location *matchLocation(const std::string &uri) const;
};

// todo: placeholder to be replaced by filename.conf parser
ServerConfig createDefaultServerConfig();

#endif
