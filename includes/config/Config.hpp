
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <map>
#include <string>
#include <vector>

struct LocationConfig {
    std::string                                     path;
    std::map<std::string, std::vector<std::string> > directives;
};

struct ServerConfig {
    int                                             listen;
    std::string                                     host;
    std::string                                     server_name;
    std::map<std::string, std::vector<std::string> > directives;
    std::vector<LocationConfig>                     locations;

    ServerConfig();
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
