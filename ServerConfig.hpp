#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>
#include <vector>

struct ServerConfig
{
    std::string                 host = "localhost";
    int                         port = 0;
    std::string                 root;
    std::string                 index;
    std::string                 error_404;
    size_t                      client_max_body_size = 0;
    std::vector<std::string>    methods;
};

#endif