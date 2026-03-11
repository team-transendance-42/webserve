// srcs/ServerConfig.cpp
#include "../includes/ServerConfig.hpp"


// placeholder: todo
ServerConfig createDefaultServerConfig() {
    ServerConfig config;

    config.host = "127.0.0.1";
    config.port = 8080;
    config.server_names.push_back("one");

    // error pages
    config.error_pages[404] = "./www/errors/404.html";
    config.error_pages[500] = "./www/errors/500.html";

    // default location
    Location loc;
    loc.path                 = "/";
    loc.root                 = "./www/one";
    loc.index                = "index.html";
    loc.autoindex            = false;
    loc.client_max_body_size = 1000000;
    loc.allowed_methods.push_back("GET");
    loc.allowed_methods.push_back("POST");
    loc.allowed_methods.push_back("DELETE");
    config.locations.push_back(loc);

    return config;
}