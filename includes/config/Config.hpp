
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <map>
#include <string>
#include <vector>

// struct LocationConfig {
//     std::string                                     path;
//     std::map<std::string, std::vector<std::string> > directives;
// };

// struct ServerConfig {
//     int                                             listen;
//     std::string                                     host;
//     std::string                                     server_name;
//     std::map<std::string, std::vector<std::string> > directives;
//     std::vector<LocationConfig>                     locations;

//     ServerConfig();
// };

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
    bool                     deny_all             = false; // for guarding sensitive locations
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
    std::string              	host;
    int                      	port;
    long                     	client_max_body_size;
    std::vector<std::string> 	server_names;
    std::map<int,std::string>	error_pages;
    bool                      	default_server;
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

#endif // CONFIG_HPP
