#pragma once

#include <map>
#include <string>
#include <vector>

struct Location {
    std::string              path;
    std::string              root;
    std::string              index;
    std::string              cgi_extension;
    std::string              cgi_pass;
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

    bool hasCgi() const {
        return (!cgi_extension.empty() && !cgi_pass.empty());
    }
};

// todo: hard coded values for now, to be replaced by filename.conf parser
struct ServerConfig {
    std::string              	host; //                 = "127.0.0.1";
    int                      	port                 = 0;
    long                     	clientMaxBodySize; //= 1048576;  // 1MB default
    std::vector<std::string> 	server_names; // for Host: header matching in virtual hosting
    std::map<int,std::string>	errorPages;
    bool                      	default_server = false;
    std::vector<Location>    	locations;

    // find longest matching location for a URI
    const Location *matchLocation(const std::string &uri) const;
};

struct ConfigFile {
    std::vector<ServerConfig> servers;
};

class ConfigParser {
public:
    ConfigFile parseFile(const std::string& filePath) const;
    ConfigFile parseString(const std::string& text) const;
};
