#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

/*
std::getline(ss, key, '=')
Read from ss
Put everything until '='
Store it in key

std::stringstream ms(value);
std::stringstream → built-in C++ class (from <sstream>)
ms → just a variable name you chose
value → string used to initialize the stream
creates a string stream object and loads it with the string: if
value = "GET,POST,DELETE"
Then ms now behaves like a stream containing:
GET,POST,DELETE
And you can read from it like a file:
std::getline(ms, method, ',');
*/

ServerConfig ConfigParser::parse(const std::string& filename)
{
    ServerConfig config;
    std::ifstream file(filename);
    std::string line;

    if (!file || !file.is_open())
    {
        throw std::runtime_error("Cannot open config file");
    }

    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string key, value;

        if (std::getline(ss, key, '=') && std::getline(ss, value))
        {
            if (key == "host")
                config.host = value;
            else if (key == "port")
                config.port = std::atoi(value.c_str());
            else if (key == "root")
                config.root = value;
            else if (key == "index")
                config.index = value;
            else if (key == "error_404")
                config.error_404 = value;
            else if (key == "client_max_body_size")
                config.client_max_body_size = std::atoi(value.c_str());
            else if (key == "methods")
            {
                std::stringstream ms(value);
                std::string method;
                while (std::getline(ms, method, ','))
                    config.methods.push_back(method);
            }
        }
    }

    return config;
}