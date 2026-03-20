#pragma once

#include <string>

#include "ServerConfig.hpp"

class PathResolver {
public:
    static std::string resolveFilePath(const Location &loc,
                                       const std::string &request_path);
};
