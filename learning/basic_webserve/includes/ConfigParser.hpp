#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include "ServerConfig.hpp"

class ConfigParser
{
public:
    ServerConfig parse(const std::string& filename);
};

#endif