#include "ConfigParser.hpp"
#include <iostream>
#include <sstream>

/*
>> extraction operator.
"Extract data from the stream and store it into a variable.
*/

//  c++ main.cpp ConfigParser.cpp
int main(int argc, char* argv[])
{
    std::string text = "10 20 30";
    std::stringstream ss(text);

    int a, b, c;
    ss >> a >> b >> c;
    std::cout << a + b + c << std::endl;  // 60

    try {
        ConfigParser parser;
        ServerConfig config = parser.parse("config.conf");

        std::cout << "Host: " << config.host << std::endl;
        std::cout << "Port: " << config.port << std::endl;
        std::cout << "Root: " << config.root << std::endl;
        std::cout << "Index: " << config.index << std::endl;
        std::cout << "Error 404: " << config.error_404 << std::endl;
        std::cout << "Max body size: " << config.client_max_body_size << std::endl;

        std::cout << "Methods: ";
        for (size_t i = 0; i < config.methods.size(); i++)
            std::cout << config.methods[i] << " ";
        std::cout << std::endl;
    } catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}